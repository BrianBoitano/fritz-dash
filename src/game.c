// Fritz Dash - the screens around play: title menu, level select, pause,
// level complete, Customize, Stats, Settings, music, and the saved progress,
// settings, look, stats and best score.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "badges.h"
#include "draw.h"
#include "game.h"
#include "platform.h"
#include "rate.h"
#include "synth.h"

#define COL_CUBE      RGB(90, 255, 40)
#define COL_CUBE_CORE RGB(0, 230, 255)
#define COL_PINK      RGB(255, 50, 200)
#define COL_YELLOW    RGB(255, 235, 40)
#define COL_WHITE     RGB(255, 255, 255)
#define COL_RED       RGB(255, 90, 90)
#define COL_DIM       RGB(130, 130, 170)
#define COL_DARK      RGB(10, 6, 26)

#define PI_F 3.14159265f

// The level being played or edited; also scratch space while scanning files.
static Level level;
static char file_buf[512 * 1024];
static char diag_text[128];

static const LevelColors TITLE_COLORS = { RGB(10, 4, 40), RGB(62, 10, 92), RGB(20, 6, 52), RGB(0, 240, 255) };

static const char *const TITLE_ITEMS[TITLE_COUNT] = { "LEVELS", "FRITZ WORLD", "2 PLAYER", "ENDLESS", "BADGES", "STATS",
                                                      "MODE LAB",
                                                      "EDITOR", "SONG MAKER", "ICON MAKER", "CUSTOMIZE", "SETTINGS",
                                                      "SECRETS" };
enum { PAUSE_RESUME, PAUSE_RESTART, PAUSE_PRACTICE, PAUSE_EXIT, PAUSE_COUNT };
enum { SET_PROGRESS, SET_AUTO_CP, SET_MUSIC, SET_SFX, SET_CLEAN, SET_GHOST, SET_PRACTICE_SONG, SET_SECRETS, SET_BACK,
       SET_COUNT };
static const char *const LOOK_ROWS[LOOK_PARTS] = { "COLOUR 1", "COLOUR 2", "FACE", "SHIP", "TRAIL", "DEATH" };
static const char *const LOOK_KEYS[LOOK_PARTS] = { "col1", "col2", "face", "ship", "trail", "death" };
static const char *const STAT_KEYS[STAT_COUNT] = { "attempts", "jumps", "deaths", "wins", "practice_wins",
                                                   "seconds", "orbs" };

void game_set_diag(const char *text)
{
    snprintf(diag_text, sizeof(diag_text), "%s", text);
}

// ---------------------------------------------------------------------------
// Saved files

// Settings, look and stats files are lines of "<key> <number>".
static int setting(const char *text, const char *key, int def)
{
    size_t k = strlen(key);
    for (const char *p = text; p; p = strchr(p, '\n')) {
        if (*p == '\n')
            p++;
        if (!strncmp(p, key, k) && p[k] == ' ')
            return atoi(p + k + 1);
    }
    return def;
}

static void read_small(const char *path)
{
    int n = plat_read_file(path, file_buf, 4096);
    file_buf[n > 0 ? n : 0] = 0;
}

static int clamp(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static void load_settings(Game *g)
{
    read_small(SETTINGS_FILE);
    g->show_progress = setting(file_buf, "progress_bar", 1) != 0;
    g->auto_checkpoints = setting(file_buf, "auto_checkpoints", 0) != 0;
    g->clean_hud = setting(file_buf, "clean_hud", 0) != 0;
    g->music_vol = clamp(setting(file_buf, "music", 7), 0, 10);
    g->sfx_vol = clamp(setting(file_buf, "sfx", 8), 0, 10);
    g->show_ghost = setting(file_buf, "ghost", 1) != 0;
    g->practice_song = setting(file_buf, "practice_song", 1) != 0;
    synth_volume(g->music_vol, g->sfx_vol);
}

static void save_settings(const Game *g)
{
    char buf[240];
    int n = snprintf(buf, sizeof(buf),
                     "progress_bar %d\nauto_checkpoints %d\nclean_hud %d\nmusic %d\nsfx %d\nghost %d\npractice_song %d\n",
                     g->show_progress, g->auto_checkpoints, g->clean_hud, g->music_vol, g->sfx_vol, g->show_ghost,
                     g->practice_song);
    plat_write_file(SETTINGS_FILE, buf, n);
}

static void load_look(Game *g)
{
    static const int DEFAULTS[LOOK_PARTS] = { 0, 1, 0, 0, 0, 0 };
    read_small(CUSTOM_FILE);
    for (int i = 0; i < LOOK_PARTS; i++)
        g->look.v[i] = clamp(setting(file_buf, LOOK_KEYS[i], DEFAULTS[i]), 0, LOOK_COUNT[i] - 1);
    int s = setting(file_buf, "stock", 0);
    g->look.stock = s > 0 && s <= STOCK_COUNT ? s : 0;
    g->browse = g->look;
}

static void save_look(const Game *g)
{
    char buf[160];
    int n = 0;
    for (int i = 0; i < LOOK_PARTS; i++)
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s %d\n", LOOK_KEYS[i], g->look.v[i]);
    n += snprintf(buf + n, sizeof(buf) - (size_t)n, "stock %d\n", g->look.stock);
    plat_write_file(CUSTOM_FILE, buf, n);
}

static void load_stats(Game *g)
{
    read_small(STATS_FILE);
    for (int i = 0; i < STAT_COUNT; i++)
        g->stats[i] = setting(file_buf, STAT_KEYS[i], 0);
}

static void save_stats(const Game *g)
{
    char buf[256];
    int n = 0;
    for (int i = 0; i < STAT_COUNT; i++)
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s %d\n", STAT_KEYS[i], g->stats[i]);
    plat_write_file(STATS_FILE, buf, n);
}

// title.txt: "tagline <words>" and "newest <level file>".
static void load_title(Game *g)
{
    read_small(TITLE_FILE);
    g->tagline[0] = g->newest[0] = 0;
    for (char *line = file_buf; line && *line;) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = 0;
        if (!strncmp(line, "tagline ", 8))
            snprintf(g->tagline, sizeof(g->tagline), "%s", line + 8);
        else if (!strncmp(line, "newest ", 7))
            snprintf(g->newest, sizeof(g->newest), "%s", line + 7);
        line = nl ? nl + 1 : NULL;
    }
}

static void save_title(const Game *g)
{
    char buf[160];
    int n = snprintf(buf, sizeof(buf), "tagline %s\nnewest %s\n", g->tagline, g->newest);
    plat_write_file(TITLE_FILE, buf, n);
}

static int lworld_of(int builtin);

// The world a level key belongs to.
static int lworld_of_key(const char *key)
{
    if (strncmp(key, "builtin:", 8))
        return LWORLD_MINE;
    for (int i = 0; i < BUILTIN_COUNT; i++)
        if (!strcmp(BUILTIN_IDS[i], key + 8))
            return lworld_of(i);
    return -1;
}

// last.txt: "lworld N" (the world card last picked) and "<world> <level key>"
// for the level last chosen in each world. 3.1 wrote "world N" with MY LEVELS
// as 6; each key goes back to the world it belongs to.
static void load_last(Game *g)
{
    read_small(LAST_FILE);
    int w = setting(file_buf, "lworld", -1);
    if (w < 0)
        w = setting(file_buf, "world", 0) == 6 ? LWORLD_MINE : setting(file_buf, "world", 0);
    g->lworld_sel = clamp(w, 0, NUM_LWORLDS - 1);
    memset(g->last_key, 0, sizeof(g->last_key));
    for (char *line = file_buf; line && *line;) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = 0;
        if (line[0] >= '0' && line[0] <= '9' && line[1] == ' ' && lworld_of_key(line + 2) >= 0)
            snprintf(g->last_key[lworld_of_key(line + 2)], sizeof(g->last_key[0]), "%s", line + 2);
        line = nl ? nl + 1 : NULL;
    }
}

static void save_last(const Game *g)
{
    char buf[NUM_LWORLDS * 90 + 16];
    int n = snprintf(buf, sizeof(buf), "lworld %d\n", g->lworld_sel);
    for (int w = 0; w < NUM_LWORLDS; w++)
        if (g->last_key[w][0])
            n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%d %s\n", w, g->last_key[w]);
    plat_write_file(LAST_FILE, buf, n);
}

static int load_endless_best(void)
{
    read_small(BEST_FILE);
    return atoi(file_buf);
}

// Progress is one line per level: "<attempts> <best> <practice best> <key>".
static void load_progress(Game *g)
{
    int n = plat_read_file(PROGRESS_FILE, file_buf, (int)sizeof(file_buf) - 1);
    if (n <= 0)
        return;
    file_buf[n] = 0;
    for (char *line = file_buf; line && *line;) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = 0;
        char *p = line;
        int v[3];
        for (int i = 0; i < 3; i++)
            v[i] = (int)strtol(p, &p, 10);
        if (*p == ' ')
            p++;
        for (int i = 0; i < g->nlevels; i++)
            if (!strcmp(g->levels[i].key, p)) {
                g->levels[i].attempts = v[0];
                g->levels[i].best = v[1];
                g->levels[i].practice_best = v[2];
            }
        line = nl ? nl + 1 : NULL;
    }
}

// Coins are one line per level: "<coin bits> <key>". A coin counts once the
// level is finished with it in normal mode.
static void load_coins(Game *g)
{
    int n = plat_read_file(COINS_FILE, file_buf, (int)sizeof(file_buf) - 1);
    if (n <= 0)
        return;
    file_buf[n] = 0;
    for (char *line = file_buf; line && *line;) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = 0;
        char *p = line;
        int mask = (int)strtol(p, &p, 10);
        if (*p == ' ')
            p++;
        for (int i = 0; i < g->nlevels; i++)
            if (!strcmp(g->levels[i].key, p))
                g->levels[i].coins = mask & ((1 << g->levels[i].ncoins) - 1);
        line = nl ? nl + 1 : NULL;
    }
}

static void save_coins(const Game *g)
{
    static char out[MAX_LEVELS * 100];
    int n = 0;
    for (int i = 0; i < g->nlevels; i++)
        if (g->levels[i].coins)
            n += snprintf(out + n, sizeof(out) - (size_t)n, "%d %s\n", g->levels[i].coins, g->levels[i].key);
    plat_write_file(COINS_FILE, out, n);
}

static int bits(int v)
{
    int n = 0;
    for (; v; v &= v - 1)
        n++;
    return n;
}

static void save_progress(const Game *g)
{
    static char out[MAX_LEVELS * 128];
    int n = 0;
    for (int i = 0; i < g->nlevels; i++) {
        const LevelInfo *li = &g->levels[i];
        if (li->attempts || li->best || li->practice_best)
            n += snprintf(out + n, sizeof(out) - (size_t)n, "%d %d %d %s\n",
                          li->attempts, li->best, li->practice_best, li->key);
    }
    plat_write_file(PROGRESS_FILE, out, n);
}

// ---------------------------------------------------------------------------
// Level list: the built-in levels, then every .json file in LEVELS_DIR.

const char *const LWORLD_NAMES[NUM_LWORLDS] = { "FIRST JUMPS", "SUNRISE ISLES", "CORAL DEEP", "LAVA FORGE",
                                                "NEON CITY", "COSMIC CORE", "SHAPESHIFT", "THE VAULT", "MY LEVELS" };
static const LevelColors LWORLD_COLORS[NUM_LWORLDS] = {
    { RGB(11, 58, 140), RGB(47, 164, 224), RGB(10, 42, 94), RGB(255, 225, 77) },
    { RGB(255, 96, 70), RGB(255, 196, 90), RGB(16, 110, 100), RGB(255, 245, 190) },
    { RGB(4, 24, 84), RGB(0, 132, 156), RGB(2, 16, 56), RGB(255, 120, 150) },
    { RGB(36, 4, 0), RGB(196, 46, 0), RGB(20, 8, 8), RGB(255, 200, 40) },
    { RGB(24, 0, 56), RGB(130, 0, 150), RGB(10, 0, 30), RGB(0, 255, 230) },
    { RGB(2, 0, 14), RGB(64, 22, 116), RGB(4, 0, 20), RGB(150, 255, 235) },
    { RGB(70, 20, 140), RGB(20, 160, 200), RGB(16, 6, 40), RGB(255, 255, 255) },    // a prism (the card is a rainbow)
    { RGB(20, 12, 0), RGB(110, 70, 0), RGB(14, 8, 0), RGB(255, 210, 60) },
    { RGB(34, 6, 66), RGB(160, 30, 130), RGB(20, 4, 40), RGB(255, 90, 220) },
};

static int lworld_of(int builtin)
{
    if (builtin < 0)
        return LWORLD_MINE;
    char c = BUILTIN_IDS[builtin][0];
    return c >= '0' && c < '0' + LWORLD_MINE ? c - '0' : 0;
}

static void level_info(LevelInfo *li, const char *key, const char *file, int builtin)
{
    memset(li, 0, sizeof(*li));
    snprintf(li->name, sizeof(li->name), "%s", level.name);
    snprintf(li->author, sizeof(li->author), "%s", level.author);
    snprintf(li->key, sizeof(li->key), "%s", key);
    snprintf(li->file, sizeof(li->file), "%s", file);
    li->builtin = builtin;
    li->nobj = level.nobj;
    li->speed = level.speed;
    li->stars = builtin >= 0 ? level.stars : 0;
    li->colors = level.colors;
    li->ncoins = level.ncoins;
    li->verified = level_verified(&level);
    li->rating = level_rating(&level);
    li->world = lworld_of(builtin);
}

static void add_level(Game *g, const char *key, const char *file, int builtin)
{
    level_info(&g->levels[g->nlevels++], key, file, builtin);
}

// The built-in levels are parsed once, at the first scan; rescans copy them
// and only read his files.
static LevelInfo builtin_info[MAX_LEVELS];
static int nbuiltin_info = -1;

static void parse_builtins(void)
{
    nbuiltin_info = 0;
    for (int i = 0; i < BUILTIN_COUNT && nbuiltin_info < MAX_LEVELS; i++) {
        if (level_parse(&level, BUILTIN_JSON[i], (int)strlen(BUILTIN_JSON[i])) != LEVEL_OK)
            continue;
        char key[80];
        snprintf(key, sizeof(key), "builtin:%s", BUILTIN_IDS[i]);
        level_info(&builtin_info[nbuiltin_info++], key, "", i);
        eggs_register(i, &level);
    }
}

static int find_key(const Game *g, const char *key)
{
    for (int i = 0; i < g->nlevels && key[0]; i++)
        if (!strcmp(g->levels[i].key, key))
            return i;
    return -1;
}

// Skips hidden files, including the "._name" files macOS FTP clients leave.
static int is_level_file(const char *name)
{
    size_t n = strlen(name);
    if (name[0] == '.' || n < 6 || n >= sizeof(((LevelInfo *)0)->file))
        return 0;
    const char *ext = name + n - 5;
    const char *want = ".json";
    for (int i = 0; i < 5; i++) {
        char c = ext[i];
        if (c >= 'A' && c <= 'Z')
            c += 32;
        if (c != want[i])
            return 0;
    }
    return 1;
}

static int read_level_file(const char *name)
{
    char path[160];
    snprintf(path, sizeof(path), "%s/%s", LEVELS_DIR, name);
    int n = plat_read_file(path, file_buf, (int)sizeof(file_buf));
    return n >= 0 && n < (int)sizeof(file_buf) && level_parse(&level, file_buf, n) == LEVEL_OK;
}

static void scan_file(const char *name, void *ctx)
{
    Game *g = ctx;
    if (!is_level_file(name) || g->nlevels >= MAX_LEVELS)
        return;
    if (!read_level_file(name)) {
        g->bad_files++;
        return;
    }
    char key[80];
    snprintf(key, sizeof(key), "file:%s", name);
    add_level(g, key, name, -1);
}

// Keeps the level select on the same level, even when files came or went.
static void scan_levels(Game *g)
{
    char was[80];
    snprintf(was, sizeof(was), "%s", g->nlevels ? g->levels[g->level_sel].key : g->last_key[g->lworld_sel]);
    if (nbuiltin_info < 0)
        parse_builtins();
    memcpy(g->levels, builtin_info, sizeof(LevelInfo) * (size_t)nbuiltin_info);
    g->nlevels = nbuiltin_info;
    g->bad_files = 0;
    int first_file = g->nlevels;
    plat_list_dir(LEVELS_DIR, scan_file, g);
    // Directory order is arbitrary, so files are listed by name.
    for (int i = first_file + 1; i < g->nlevels; i++) {
        LevelInfo v = g->levels[i];
        int k = i;
        while (k > first_file && strcmp(v.file, g->levels[k - 1].file) < 0) {
            g->levels[k] = g->levels[k - 1];
            k--;
        }
        g->levels[k] = v;
    }
    load_progress(g);
    load_coins(g);
    int i = find_key(g, was);
    g->level_sel = i >= 0 ? i : g->level_sel < g->nlevels ? g->level_sel : 0;
}

static int load_level(const Game *g, int i)
{
    const LevelInfo *li = &g->levels[i];
    if (li->builtin >= 0)
        return level_parse(&level, BUILTIN_JSON[li->builtin], (int)strlen(BUILTIN_JSON[li->builtin])) == LEVEL_OK;
    return read_level_file(li->file);
}

int game_stars(const Game *g)
{
    int n = 0;
    for (int i = 0; i < g->nlevels; i++)
        if (g->levels[i].builtin >= 0 && g->levels[i].best == 100)
            n += g->levels[i].stars;
    return n;
}

int game_coins(const Game *g)
{
    int n = 0;
    for (int i = 0; i < g->nlevels; i++)
        if (g->levels[i].builtin >= 0)
            n += bits(g->levels[i].coins);
    return n;
}

static int is_boss(const LevelInfo *li)
{
    return li->builtin >= 0 && BUILTIN_IDS[li->builtin][1] == '9';
}

void game_lworld_stats(const Game *g, int w, LWorldStats *s)
{
    memset(s, 0, sizeof(*s));
    s->boss = -1;
    for (int i = 0; i < g->nlevels; i++) {
        const LevelInfo *li = &g->levels[i];
        if (li->world != w)
            continue;
        s->count++;
        s->beaten += li->best == 100;
        s->stars += li->best == 100 ? li->stars : 0;
        s->max_stars += li->stars;
        s->coins += bits(li->coins);
        s->max_coins += li->ncoins;
        if (li->builtin >= 0 && (s->boss < 0 || is_boss(li) ||
                                 (!is_boss(&g->levels[s->boss]) && li->stars >= g->levels[s->boss].stars)))
            s->boss = i;
    }
}

// The next level (dir 1) or the one before (-1) in the same world, round and round.
static int lworld_next(const Game *g, int from, int dir)
{
    int n = g->nlevels;
    for (int k = 1; k < n; k++) {
        int i = ((from + dir * k) % n + n) % n;
        if (g->levels[i].world == g->levels[from].world)
            return i;
    }
    return from;
}

// Built-in worlds show once they have levels; MY LEVELS always does. The
// VAULT shows (locked) once he has found an easter egg.
static int lworld_shown(const Game *g, int w)
{
    if (w == LWORLD_VAULT && !eggs_count())
        return 0;
    for (int i = 0; i < g->nlevels; i++)
        if (g->levels[i].world == w)
            return 1;
    return w == LWORLD_MINE;
}

// The level select comes back to this world and level next time.
static void remember(Game *g, int save)
{
    if (!g->nlevels)
        return;
    const LevelInfo *li = &g->levels[g->level_sel];
    g->lworld_sel = li->world;
    snprintf(g->last_key[li->world], sizeof(g->last_key[0]), "%s", li->key);
    if (save)
        save_last(g);
}

// ---------------------------------------------------------------------------
// State machine

void game_init(Game *g, uint32_t seed)
{
    memset(g, 0, sizeof(*g));
    g->seed = seed ? seed : 0x12345678u;
    g->preview_rng = g->seed | 1u;
    g->prev_input = ~0;     // a button held at boot must be released first
    g->song = -2;
    load_settings(g);
    icons_load();
    load_look(g);
    load_stats(g);
    load_title(g);
    load_last(g);
    g->endless_best = load_endless_best();
    scan_levels(g);
    eggs_load();
    badges_load(g);
    songs_load_all();
    g->egg_look = g->look;
}

static void go(Game *g, int state)
{
    g->state = state;
    g->state_frames = 0;
}

static void music(Game *g, int song)
{
    if (g->song != song) {
        g->song = song;
        synth_song(song);
    }
}

static void toast(Game *g, const char *m)
{
    snprintf(g->msg, sizeof(g->msg), "%s", m);
    g->msg_frames = 100;
}

// Up/down move with wrap-around, X picks. Returns the picked index, or -1.
static int menu_update(int *sel, int count, int edges)
{
    int was = *sel;
    if (edges & IN_UP)
        *sel = (*sel + count - 1) % count;
    if (edges & IN_DOWN)
        *sel = (*sel + 1) % count;
    if (*sel != was)
        synth_sfx(SND_MOVE);
    if (edges & IN_CROSS) {
        synth_sfx(SND_SELECT);
        return *sel;
    }
    return -1;
}

static int level_song(void)
{
    return level.user_song ? SONG_USER + (level.user_song - 1) % USER_SONGS : level.song % NUM_SONGS;
}

// Practice plays the calm song unless Settings say otherwise; editor
// playtests keep the level's own song.
static int run_song(const Game *g)
{
    return g->run.practice && g->practice_song && !g->playtest ? SONG_PRACTICE : level_song();
}

static int ghost_mode(const Game *g)
{
    return !g->playtest && !g->run.practice;
}

// A new attempt: its presses are recorded from here, and the ghost of the
// best run sets off beside it.
static void hint_off(Game *g)
{
    g->hint.state = HINT_IDLE;
    g->hint_t = -1;
    g->hint_npath = 0;
}

// Dots ahead of the hint ghost: where it goes in the next 48 frames.
static void hint_path(Game *g)
{
    static Run ahead;
    g->hint_npath = 0;
    if (g->hint_t < 0 || g->hint_t >= HINT_FRAMES || g->hint_t >= g->hint.len)
        return;
    ahead = g->hint_run;
    for (int t = g->hint_t, k = 1; t < g->hint.len && g->hint_npath < HINT_PATH && ahead.state == RUN_PLAY; t++, k++) {
        run_step(&ahead, g->hint.trace[t]);
        if (k % 4 == 0) {
            g->hint_path[g->hint_npath][0] = ahead.b.x;
            g->hint_path[g->hint_npath++][1] = ahead.b.y;
        }
    }
}

static void attempt_begins(Game *g)
{
    g->rec.frames = 0;
    g->ghost_t = -1;
    if (g->show_ghost && ghost_mode(g) && g->ghost.frames > 0) {
        run_start(&g->ghost_run, &level, 0, 0);
        g->ghost_run.look = g->look;
        g->ghost_t = 0;
    }
}

static void begin_run(Game *g, int practice)
{
    run_start(&g->run, &level, practice, g->auto_checkpoints);
    g->run.look = g->look;
    if (g->playtest == 1 && g->ed.test_x > 0)
        run_start_at(&g->run, g->ed.test_x);
    g->new_coins = 0;
    g->just_verified = 0;
    g->world_result = 0;
    g->run.coins_had = g->playtest ? 0 : g->levels[g->level_sel].coins;
    g->run.eggs_had = g->playtest || g->levels[g->level_sel].builtin < 0 ? 0 : eggs_level_mask(g->levels[g->level_sel].builtin);
    g->run.coins_silver = g->playtest || g->levels[g->level_sel].builtin < 0;
    if (g->playtest != PLAYTEST_LAB)
        g->stats[STAT_ATTEMPTS]++;
    g->song = run_song(g);
    synth_song(g->song);
    synth_pause(0);
    g->run.song_t = 0;
    g->run.bpm = synth_bpm(g->song);
    attempt_begins(g);
    hint_off(g);
    go(g, ST_PLAY);
}

// MODE LAB: mode m's demo as a trial that counts for nothing (PLAYTEST_LAB).
static void start_lab(Game *g, int m)
{
    int i = lab_level(m);
    if (i < 0 || level_parse(&level, LAB_JSON[i], (int)strlen(LAB_JSON[i])) != LEVEL_OK)
        return;
    g->playtest = PLAYTEST_LAB;
    g->verifying = 0;
    g->new_stars = 0;
    begin_run(g, 0);
}

static void start_level(Game *g, int practice)
{
    if (!load_level(g, g->level_sel)) {
        scan_levels(g);
        return;
    }
    g->playtest = 0;
    g->new_stars = 0;
    remember(g, 1);
    g->rec.hash = level_hash(&level);
    ghost_load(&g->ghost, g->levels[g->level_sel].key, g->rec.hash);
    g->ndeaths = deaths_load(g->deaths, g->levels[g->level_sel].key);
    begin_run(g, practice);
}

static void record_attempt(Game *g, int ev)
{
    if (g->playtest == PLAYTEST_LAB)
        return;
    if (ev == EV_COMPLETE)
        g->stats[g->run.practice ? STAT_PRACTICE_WINS : STAT_WINS]++;
    else
        g->stats[STAT_DEATHS]++;
    save_stats(g);
    if (g->verifying && ev == EV_COMPLETE && !g->run.practice) {
        editor_verified(&g->ed, &level);
        g->just_verified = 1;
    }
    if (g->playtest)
        return;
    if (g->world_run && ev == EV_COMPLETE && !g->run.practice)
        g->world_result = world_beaten(&g->world, g->world.sel);
    LevelInfo *li = &g->levels[g->level_sel];
    int *best = g->run.practice ? &li->practice_best : &li->best;
    if (ev == EV_DIED) {
        if (g->ndeaths == MAX_DEATHS)
            memmove(g->deaths, g->deaths + 1, sizeof(g->deaths[0]) * (MAX_DEATHS - 1));
        else
            g->ndeaths++;
        g->deaths[g->ndeaths - 1][0] = (int)g->run.b.x;
        g->deaths[g->ndeaths - 1][1] = (int)g->run.b.y;
        deaths_save((const int (*)[2])g->deaths, g->ndeaths, li->key);
    }
    if (!g->run.practice && g->run.pct == 100 && li->best < 100)
        g->new_stars = li->stars;
    if (ev == EV_COMPLETE && !g->run.practice && li->builtin >= 0 && li->practice_best == 0)
        badge_award(g, B_NO_PRACTICE);
    if (ev == EV_COMPLETE && !g->run.practice && li->attempts >= 49)
        badge_award(g, B_COMEBACK);
    if (ev == EV_COMPLETE && !g->run.practice && (g->run.b.coins & ~li->coins)) {
        g->new_coins = g->run.b.coins & ~li->coins;
        li->coins |= g->run.b.coins;
        save_coins(g);
    }
    if (ghost_mode(g) && g->run.pct > g->ghost.pct && g->rec.frames > 0) {
        g->rec.pct = g->run.pct;
        g->ghost = g->rec;
        ghost_save(&g->ghost, li->key);
    }
    li->attempts++;
    if (g->run.pct > *best)
        *best = g->run.pct;
    save_progress(g);
    badges_update(g);
}

static void sounds_of(const Run *r)
{
    static const int MAP[][2] = { { SFX_JUMP, SND_JUMP }, { SFX_ORB, SND_ORB }, { SFX_PAD, SND_PAD },
                                  { SFX_PORTAL, SND_PORTAL }, { SFX_DEATH, SND_DEATH }, { SFX_DONE, SND_DONE },
                                  { SFX_CHECKPOINT, SND_CHECKPOINT }, { SFX_COIN, SND_COIN }, { SFX_MORPH, SND_MORPH },
                                  { SFX_EGG, SND_EGG } };
    for (int i = 0; i < (int)(sizeof(MAP) / sizeof(MAP[0])); i++)
        if (r->sfx & MAP[i][0])
            synth_sfx(MAP[i][1]);
    static const int MAP_B[][2] = { { SFX_BLAST, SND_BLAST }, { SFX_BREAK, SND_BREAK }, { SFX_BOOM, SND_BOOM },
                                    { SFX_TICK, SND_TICK }, { SFX_SPLAT, SND_SPLAT }, { SFX_SWAP, SND_SWAP },
                                    { SFX_WHOO, SND_WHOO } };
    for (int i = 0; i < (int)(sizeof(MAP_B) / sizeof(MAP_B[0])); i++)
        if (r->sfx & MAP_B[i][0])
            synth_sfx(MAP_B[i][1]);
}

static void run_sounds(Game *g)
{
    sounds_of(&g->run);
    if (g->playtest == PLAYTEST_LAB)
        return;
    if (g->run.sfx & SFX_JUMP)
        g->stats[STAT_JUMPS]++;
    if (g->run.sfx & SFX_ORB)
        g->stats[STAT_ORBS]++;
}

static void leave_run(Game *g)
{
    synth_pause(0);
    g->verifying = 0;
    if (g->playtest == PLAYTEST_LAB) {
        g->playtest = 0;
        music(g, eggs_menu_song());
        go(g, ST_LAB);
    } else if (g->playtest) {
        music(g, -1);
        editor_resume(&g->ed);
        go(g, ST_EDITOR);
    } else if (g->world_run) {
        g->world_run = 0;
        if (g->world_result == 1)
            g->world.sel = g->world.reached;
        music(g, eggs_menu_song());
        go(g, ST_WORLD);
    } else {
        music(g, eggs_menu_song());
        go(g, ST_LEVELS);
    }
}

static void resume(Game *g)
{
    g->run.held_lock = 1;
    synth_pause(0);
    go(g, ST_PLAY);
}

static void restart_song(Game *g)
{
    g->song = run_song(g);
    synth_song(g->song);
    g->run.song_t = 0;
    g->run.bpm = synth_bpm(g->song);
}

static void step_play(Game *g, int input, int edges)
{
    if (g->run.state != RUN_DONE && (edges & IN_START)) {
        g->pause_sel = PAUSE_RESUME;
        synth_pause(1);
        go(g, ST_PAUSE);
        return;
    }
    // Show me how: back to the last checkpoint while the search runs, then
    // the hint plays beside the player (again after each death there).
    if (g->hint.state == HINT_SEARCH) {
        if (hint_step(&g->hint, HINT_STEPS) == HINT_READY) {
            hint_run(&g->hint, &g->run, &g->hint_run);
            g->hint_t = 0;
        } else if (g->hint.state == HINT_FAIL) {
            toast(g, "NO WAY THROUGH - TRIANGLE REMOVES IT");
            hint_off(g);
        }
        return;
    }
    if (g->run.practice && (edges & (IN_TOUCH | IN_L3)) && g->run.state == RUN_PLAY) {
        run_rewind(&g->run);
        hint_start(&g->hint, &g->run);
        g->hint_t = -1;
        g->hint_cp = g->run.ncp;
        return;
    }
    int in = input & (IN_CROSS | IN_SQUARE | IN_TRIANGLE);
    if (ghost_mode(g) && g->run.state == RUN_PLAY && ghost_push(&g->rec, in & IN_CROSS) && (in & IN_SQUARE))
        ghost_mark_square(&g->rec);
    int ev = run_step(&g->run, in);
    if (g->ghost_t >= 0 && g->ghost_t < g->ghost.frames) {
        run_step(&g->ghost_run, (ghost_bit(&g->ghost, g->ghost_t) ? IN_CROSS : 0) |
                                (ghost_square(&g->ghost, g->ghost_t) ? IN_SQUARE : 0));
        g->ghost_t++;
    }
    if (g->hint.state == HINT_READY && g->run.ncp != g->hint_cp)
        hint_off(g);
    if (g->hint_t >= 0) {
        if (g->hint_t < HINT_FRAMES && g->hint_t < g->hint.len)
            run_step(&g->hint_run, g->hint.trace[g->hint_t++]);
        else if (g->run.state == RUN_PLAY)
            hint_off(g);        // made it through with the hint: it is done
    }
    hint_path(g);
    if (ev == EV_RESPAWN && g->hint.state == HINT_READY && !memcmp(&g->run.b, &g->hint.from, sizeof(Body))) {
        hint_run(&g->hint, &g->run, &g->hint_run);
        g->hint_t = 0;
    }
    run_sounds(g);
    if (g->run.sfx & SFX_JUMP)
        rw_event(RW_JUMP, g->run.b.x, g->run.b.y);
    if (ev == EV_DIED || ev == EV_COMPLETE)
        record_attempt(g, ev);
    eggs_after_run(g, ev);
    // Like Geometry Dash, the song stops on a death and starts over with the
    // next attempt; practice keeps it going.
    if (ev == EV_DIED && !g->run.practice)
        music(g, -1);
    if (ev == EV_RESPAWN) {
        if (g->playtest != PLAYTEST_LAB)
            g->stats[STAT_ATTEMPTS]++;
        if (!g->run.practice)
            restart_song(g);
        attempt_begins(g);
    }
    if (g->run.state == RUN_DONE && g->run.state_frames >= 90)
        go(g, ST_COMPLETE);
}

// The world cards: 4 to a row (5 once there are more than 8). X opens the
// world on the level last chosen there.
static int pick_cols(int n)
{
    return n > 8 ? 5 : 4;
}

static void step_picker(Game *g, int edges)
{
    int shown[NUM_LWORLDS], n = 0, at = 0;
    for (int w = 0; w < NUM_LWORLDS; w++)
        if (lworld_shown(g, w)) {
            if (w == g->lworld_sel)
                at = n;
            shown[n++] = w;
        }
    int was = at, cols = pick_cols(n);
    if (edges & IN_LEFT)
        at = (at + n - 1) % n;
    if (edges & IN_RIGHT)
        at = (at + 1) % n;
    if ((edges & IN_UP) && at >= cols)
        at -= cols;
    if ((edges & IN_DOWN) && at / cols < (n - 1) / cols)
        at = at + cols < n ? at + cols : n - 1;
    if (at != was)
        synth_sfx(SND_MOVE);
    g->lworld_sel = shown[at];
    if (edges & IN_CIRCLE) {
        synth_sfx(SND_BACK);
        go(g, ST_TITLE);
    } else if ((edges & IN_CROSS) && g->lworld_sel == LWORLD_VAULT && !eggs_vault_open()) {
        char buf[48];
        snprintf(buf, sizeof(buf), "FIND %d EASTER EGGS TO OPEN IT (%d SO FAR)", VAULT_EGGS, eggs_count());
        toast(g, buf);
        synth_sfx(SND_BACK);
    } else if (edges & IN_CROSS) {
        int i = find_key(g, g->last_key[g->lworld_sel]);
        if (i < 0 || g->levels[i].world != g->lworld_sel)
            for (i = 0; i < g->nlevels && g->levels[i].world != g->lworld_sel; i++)
                ;
        if (i >= g->nlevels) {
            toast(g, "NO LEVELS HERE YET - MAKE ONE IN THE EDITOR!");
            synth_sfx(SND_BACK);
            return;
        }
        g->level_sel = i;
        g->picking = 0;
        remember(g, 1);
        synth_sfx(SND_SELECT);
    }
}

// Remix: the level into his first empty slot, straight into the editor.
static void remix(Game *g)
{
    if (!load_level(g, g->level_sel)) {
        scan_levels(g);
        return;
    }
    editor_open(&g->ed, &level);
    if (!load_level(g, g->level_sel) || editor_remix(&g->ed, &level) < 0) {
        toast(g, "ALL 20 SLOTS ARE FULL - DELETE ONE IN THE EDITOR");
        synth_sfx(SND_BACK);
        return;
    }
    music(g, -1);
    go(g, ST_EDITOR);
}

static int look_locked(const Game *g, const LookOption *o)
{
    return o->stars > game_stars(g) || o->coins > game_coins(g) || (o->badge && !badge_got(g, o->badge - 1));
}

// MY ICON faces wait until that icon has been drawn.
static int icon_locked(int row, int v)
{
    return row == LOOK_FACE && FACE_IS_ICON(v) && icon_empty(v - FACE_ICON0);
}

// ---------------------------------------------------------------------------
// 2 player race: both play the level at once; the camera follows whoever is
// ahead, and a player who drops off the left of the screen joins the
// leader's newest checkpoint. First to the end wins.

#define RACE_COUNT 180      // 3, 2, 1 before GO

static Look p2_look(const Look *p1)
{
    Look l = { { 2, 3, 1, 0, 0, 0 }, 0 };
    if (p1->v[LOOK_COL1] == 2 || p1->v[LOOK_COL1] == 3)
        l.v[LOOK_COL1] = 4, l.v[LOOK_COL2] = 1;
    return l;
}

static void start_race(Game *g)
{
    if (!load_level(g, g->level_sel)) {
        scan_levels(g);
        return;
    }
    g->playtest = 0;
    g->racing = 1;
    remember(g, 1);
    run_start(&g->run, &level, 1, 1);
    run_start(&g->run2, &level, 1, 1);
    g->run.race = g->run2.race = 1;
    g->run.look = g->look;
    g->run2.look = p2_look(&g->look);
    g->race_t = 0;
    g->winner = -1;
    g->race_end = 0;
    g->ghost_t = g->hint_t = -1;
    g->song = level_song();
    synth_song(g->song);
    synth_pause(0);
    g->run.bpm = g->run2.bpm = synth_bpm(g->song);
    go(g, ST_RACE);
}

static const Run *race_leader(const Game *g)
{
    return g->run2.b.x > g->run.b.x ? &g->run2 : &g->run;
}

static void step_race(Game *g, int input, int edges)
{
    if (edges & IN_START) {
        g->pause_sel = PAUSE_RESUME;
        synth_pause(1);
        go(g, ST_PAUSE);
        return;
    }
    if (g->race_t++ < RACE_COUNT) {
        if (g->race_t % 60 == 1)
            synth_sfx(SND_MOVE);
        if (g->race_t == RACE_COUNT)
            synth_sfx(SND_SELECT);
        g->run.song_t = g->run2.song_t = 0;
        return;
    }
    int ea = run_step(&g->run, input & IN_P1_CROSS ? IN_CROSS : 0);
    int eb = run_step(&g->run2, input & IN_P2_CROSS ? IN_CROSS : 0);
    sounds_of(&g->run);
    sounds_of(&g->run2);
    Run *lead = (Run *)race_leader(g), *back = lead == &g->run ? &g->run2 : &g->run;
    if (back->state == RUN_PLAY && back->b.x < lead->b.x - (PLAYER_X + PLAYER_S / 2) && lead->ncp > 0 &&
        lead->cps[lead->ncp - 1].x > back->b.x)
        run_teleport(back, &lead->cps[lead->ncp - 1]);
    if (g->winner < 0 && (ea == EV_COMPLETE || eb == EV_COMPLETE)) {
        g->winner = ea == EV_COMPLETE && eb == EV_COMPLETE ? 0 : ea == EV_COMPLETE ? 1 : 2;
        badge_award(g, B_RACER);
    }
    if (g->winner >= 0 && ++g->race_end >= 100)
        go(g, ST_RACE_DONE);
}

static void step_customize(Game *g, int edges)
{
    int row = g->custom_row, stars = game_stars(g);
    if (edges & IN_CIRCLE) {
        g->browse = g->look;
        synth_sfx(SND_BACK);
        go(g, ST_TITLE);
        return;
    }
    if (edges & IN_TRIANGLE) {
        stockui_open(&g->stock, g->look.stock - 1);
        g->custom_row = LOOK_FACE;
        g->browse = g->look;
        synth_sfx(SND_SELECT);
        go(g, ST_STOCK);
        return;
    }
    if (edges & (IN_UP | IN_DOWN)) {
        g->browse.v[row] = g->look.v[row];
        g->browse.stock = g->look.stock;
        g->custom_row = (row + ((edges & IN_UP) ? LOOK_PARTS - 1 : 1)) % LOOK_PARTS;
        synth_sfx(SND_MOVE);
    }
    int step = (edges & (IN_RIGHT | IN_CROSS)) ? 1 : (edges & IN_LEFT) ? -1 : 0;
    if (!step)
        return;
    row = g->custom_row;
    int n = LOOK_COUNT[row];
    if (row == LOOK_FACE && g->browse.stock)
        g->browse.stock = 0;        // back to the face he wore under the stock icon
    else
        g->browse.v[row] = (g->browse.v[row] + step + n) % n;
    const LookOption *o = &LOOK_OPTIONS[row][g->browse.v[row]];
    if (!look_locked(g, o) && !icon_locked(row, g->browse.v[row])) {
        g->look.v[row] = g->browse.v[row];
        if (row == LOOK_FACE)
            g->look.stock = 0;
        save_look(g);
        synth_sfx(SND_SELECT);
        badge_award(g, B_FASHION);
    } else {
        char buf[48];
        if (icon_locked(row, g->browse.v[row]))
            snprintf(buf, sizeof(buf), "DRAW %s IN THE ICON MAKER", o->name);
        else if (o->badge && !badge_got(g, o->badge - 1))
            snprintf(buf, sizeof(buf), "EARN THE %s BADGE", BADGES[o->badge - 1].name);
        else if (o->coins > game_coins(g))
            snprintf(buf, sizeof(buf), "NEEDS %d COINS - YOU HAVE %d", o->coins, game_coins(g));
        else
            snprintf(buf, sizeof(buf), "NEEDS %d STARS - YOU HAVE %d", o->stars, stars);
        toast(g, buf);
        synth_sfx(SND_BACK);
    }
    if (row == LOOK_DEATH) {
        fx_burst(g->preview, &g->browse, 1440, (float)(GROUND_Y - 470), &g->preview_rng);
        synth_sfx(SND_DEATH);
    }
}

static void step_settings(Game *g, int edges)
{
    int pick = menu_update(&g->settings_sel, SET_COUNT, edges);
    int step = pick >= 0 ? 1 : (edges & IN_RIGHT) ? 1 : (edges & IN_LEFT) ? -1 : 0;
    int changed = 0;
    switch (g->settings_sel) {
    case SET_PROGRESS:
        if (pick >= 0)
            g->show_progress = !g->show_progress, changed = 1;
        break;
    case SET_AUTO_CP:
        if (pick >= 0)
            g->auto_checkpoints = !g->auto_checkpoints, changed = 1;
        break;
    case SET_MUSIC:
        if (step)
            g->music_vol = (g->music_vol + step + 11) % 11, changed = 1;
        break;
    case SET_SFX:
        if (step) {
            g->sfx_vol = (g->sfx_vol + step + 11) % 11, changed = 1;
            synth_sfx(SND_ORB);
        }
        break;
    case SET_CLEAN:
        if (pick >= 0)
            g->clean_hud = !g->clean_hud, changed = 1;
        break;
    case SET_GHOST:
        if (pick >= 0)
            g->show_ghost = !g->show_ghost, changed = 1;
        break;
    case SET_PRACTICE_SONG:
        if (pick >= 0 || step)
            g->practice_song = !g->practice_song, changed = 1;
        break;
    case SET_SECRETS:
        if (pick >= 0) {
            secrets_open_page(g);
            go(g, ST_SECRETS);
            return;
        }
        break;
    }
    if (changed) {
        synth_volume(g->music_vol, g->sfx_vol);
        save_settings(g);
    }
    if (pick == SET_BACK || (edges & IN_CIRCLE))
        go(g, ST_TITLE);
}

void game_step(Game *g, int input)
{
    int edges = input & ~g->prev_input;
    g->prev_input = input;
    g->frame++;
    g->state_frames++;
    g->show_diag = (input & (IN_L1 | IN_R1)) == (IN_L1 | IN_R1);
    if (g->msg_frames > 0)
        g->msg_frames--;
    fx_update(g->preview);
    g->p2_here = (input & IN_P2_HERE) != 0;
    if ((g->state == ST_PLAY || g->state == ST_ENDLESS || g->state == ST_RACE) && g->playtest != PLAYTEST_LAB &&
        ++g->stat_frames >= 60) {
        g->stat_frames = 0;
        g->stats[STAT_SECONDS]++;
    }
    if (g->state == ST_TITLE || g->state == ST_LEVELS || g->state == ST_SETTINGS || g->state == ST_CUSTOMIZE ||
        g->state == ST_STATS || g->state == ST_BADGES || g->state == ST_ICONS || g->state == ST_WORLD ||
        g->state == ST_TAGLINE || g->state == ST_STOCK || g->state == ST_BOOK || g->state == ST_SECRETS ||
        g->state == ST_LAB)
        music(g, g->state != ST_LEVELS || g->picking ? eggs_menu_song()
                 : g->lworld_sel >= 1 && g->lworld_sel < LWORLD_SHAPESHIFT ? SONG_THEME1 + g->lworld_sel - 1
                 : g->lworld_sel == LWORLD_SHAPESHIFT ? SONG_THEME_SHAPESHIFT : eggs_menu_song());
    if (g->frame % 30 == 0)
        badges_update(g);
    eggs_step(g, input, edges, &level);
    rw_tick();
    editor_tick(&g->ed, (g->state == ST_EDITOR && g->ed.screen != ED_SLOTS) ||
                        ((g->state == ST_PLAY || g->state == ST_PAUSE || g->state == ST_COMPLETE) && g->playtest == 1) ? &level : NULL);
    // New badges pop up one at a time; recording mode keeps them out of play.
    if (g->nbadge_toasts && !g->negg_toasts && !(g->clean_hud && g->state == ST_PLAY)) {
        if (g->badge_toast_frames++ == 0)
            synth_sfx(SND_BADGE);
        if (g->badge_toast_frames >= BADGE_TOAST_FRAMES) {
            memmove(g->badge_toasts, g->badge_toasts + 1, sizeof(int) * (size_t)--g->nbadge_toasts);
            g->badge_toast_frames = 0;
        }
    }
    eggs_step_toast(g);

    switch (g->state) {
    case ST_TITLE: {
        if (edges & IN_TRIANGLE) {
            kb_open(&g->kb, g->tagline, TAGLINE_MAX);
            synth_sfx(SND_SELECT);
            go(g, ST_TAGLINE);
            break;
        }
        if (edges & (IN_LEFT | IN_RIGHT)) {
            // Across to the same row of the other column.
            int left = g->title_sel < TITLE_MAKE, row = left ? g->title_sel : g->title_sel - TITLE_MAKE;
            int other = left ? TITLE_MAKE + row : row;
            g->title_sel = left ? (other < TITLE_COUNT ? other : TITLE_COUNT - 1) : other;
            synth_sfx(SND_MOVE);
        }
        int pick = menu_update(&g->title_sel, TITLE_COUNT, edges);
        if (pick == TITLE_LEVELS || pick == TITLE_TWO_PLAYER) {
            scan_levels(g);
            g->two = pick == TITLE_TWO_PLAYER;
            g->picking = 1;
            g->remix_ask = 0;
            go(g, ST_LEVELS);
        } else if (pick == TITLE_WORLD) {
            scan_levels(g);
            world_open(&g->world);
            go(g, ST_WORLD);
        } else if (pick == TITLE_ENDLESS) {
            endless_init(&g->endless, g->endless_best, g->seed + (uint32_t)g->frame * 2654435761u);
            music(g, 0);
            go(g, ST_ENDLESS);
        } else if (pick == TITLE_EDITOR) {
            editor_open(&g->ed, &level);
            music(g, -1);
            go(g, ST_EDITOR);
        } else if (pick == TITLE_ICONS) {
            iconmaker_open(&g->icons, FACE_IS_ICON(g->look.v[LOOK_FACE]) ? g->look.v[LOOK_FACE] - FACE_ICON0 : 0);
            go(g, ST_ICONS);
        } else if (pick == TITLE_SONGS) {
            songmaker_open(&g->sm);
            music(g, -1);
            go(g, ST_SONGS);
        } else if (pick == TITLE_CUSTOMIZE) {
            scan_levels(g);
            g->browse = g->look;
            go(g, ST_CUSTOMIZE);
        } else if (pick == TITLE_BADGES) {
            scan_levels(g);
            badges_update(g);
            go(g, ST_BADGES);
        } else if (pick == TITLE_STATS) {
            scan_levels(g);
            go(g, ST_STATS);
        } else if (pick == TITLE_LAB) {
            go(g, ST_LAB);
        } else if (pick == TITLE_SETTINGS) {
            g->settings_sel = 0;
            go(g, ST_SETTINGS);
        } else if (pick == TITLE_SECRETS) {
            scan_levels(g);
            secrets_open_book(g);
            go(g, ST_BOOK);
        }
        break;
    }
    case ST_LAB: {
        int m = lab_step(g, edges);
        if (m == LAB_BACK)
            go(g, ST_TITLE);
        else if (m >= 0)
            start_lab(g, m);
        break;
    }
    case ST_LEVELS:
        if (g->picking) {
            step_picker(g, edges);
            break;
        }
        if (g->remix_ask) {
            if (edges & IN_CROSS) {
                g->remix_ask = 0;
                remix(g);
            } else if (edges & (IN_CIRCLE | IN_TRIANGLE)) {
                g->remix_ask = 0;
                synth_sfx(SND_BACK);
            }
            break;
        }
        if ((edges & IN_TRIANGLE) && !g->two) {
            g->remix_ask = 1;
            synth_sfx(SND_SELECT);
            break;
        }
        if (edges & (IN_LEFT | IN_RIGHT)) {
            g->level_sel = lworld_next(g, g->level_sel, (edges & IN_LEFT) ? -1 : 1);
            remember(g, 0);
            synth_sfx(SND_MOVE);
        }
        if (edges & IN_CIRCLE) {
            synth_sfx(SND_BACK);
            remember(g, 1);
            g->picking = 1;
        } else if (g->two && (edges & IN_CROSS)) {
            if (g->p2_here) {
                start_race(g);
            } else {
                g->msg_frames = 60;     // the status line flashes
                synth_sfx(SND_BACK);
            }
        } else if (edges & IN_CROSS) {
            g->racing = 0;
            start_level(g, 0);
        } else if ((edges & IN_SQUARE) && !g->two) {
            g->racing = 0;
            start_level(g, 1);
        }
        break;
    case ST_RACE:
        step_race(g, input, edges);
        break;
    case ST_RACE_DONE:
        if (g->state_frames > 30 && (edges & IN_CROSS) && g->p2_here) {
            start_race(g);
        } else if (g->state_frames > 30 && (edges & IN_CIRCLE)) {
            g->racing = 0;
            music(g, eggs_menu_song());
            go(g, ST_LEVELS);
        }
        break;
    case ST_PLAY:
        step_play(g, input, edges);
        break;
    case ST_PAUSE: {
        // The level is frozen; Options, Circle or Resume carry on. A race
        // has no practice entry.
        int pick = menu_update(&g->pause_sel, g->racing ? 3 : PAUSE_COUNT, edges);
        if (g->racing) {
            if ((edges & (IN_START | IN_CIRCLE)) || pick == 0) {
                synth_pause(0);
                go(g, ST_RACE);
            } else if (pick == 1) {
                start_race(g);
            } else if (pick == 2) {
                g->racing = 0;
                synth_pause(0);
                music(g, eggs_menu_song());
                go(g, ST_LEVELS);
            }
            break;
        }
        if ((edges & (IN_START | IN_CIRCLE)) || pick == PAUSE_RESUME) {
            resume(g);
        } else if (pick == PAUSE_RESTART) {
            run_restart(&g->run);
            if (g->playtest != PLAYTEST_LAB)
                g->stats[STAT_ATTEMPTS]++;
            restart_song(g);
            attempt_begins(g);
            hint_off(g);
            resume(g);
        } else if (pick == PAUSE_PRACTICE) {
            run_set_practice(&g->run, !g->run.practice);
            if (!g->run.practice || run_song(g) != g->song)
                restart_song(g);
            attempt_begins(g);
            hint_off(g);
            resume(g);
        } else if (pick == PAUSE_EXIT) {
            save_stats(g);
            leave_run(g);
        }
        break;
    }
    case ST_COMPLETE:
        if (g->state_frames > 30) {
            if (edges & IN_CROSS) {
                g->new_stars = 0;
                begin_run(g, g->run.practice);
            } else if (edges & IN_CIRCLE) {
                leave_run(g);
            }
        }
        break;
    case ST_ENDLESS:
        endless_step(&g->endless, input);
        if (g->endless.state == EN_DYING && g->endless.state_frames == 1)
            synth_sfx(SND_DEATH);
        if (g->endless.save_pending) {
            char buf[16];
            g->endless.save_pending = 0;
            g->endless_best = g->endless.best;
            plat_write_file(BEST_FILE, buf, snprintf(buf, sizeof(buf), "%d\n", g->endless_best));
        }
        if (g->endless.exit) {
            save_stats(g);
            go(g, ST_TITLE);
        }
        break;
    case ST_EDITOR: {
        int r = editor_step(&g->ed, &level, input, edges);
        if (r == EDR_EXIT) {
            synth_sfx(SND_BACK);
            if (g->ed.last_saved) {
                snprintf(g->newest, sizeof(g->newest), "slot%02d.json", g->ed.last_saved);
                save_title(g);
            }
            scan_levels(g);
            go(g, ST_TITLE);
        } else if (r == EDR_PLAYTEST || r == EDR_VERIFY) {
            g->playtest = 1;
            g->verifying = r == EDR_VERIFY;
            g->new_stars = 0;
            begin_run(g, !g->verifying);
        }
        break;
    }
    case ST_CUSTOMIZE:
        step_customize(g, edges);
        break;
    case ST_STOCK: {
        int r = stockui_step(&g->stock, edges, game_stars(g), game_coins(g), g->badges);
        if (r == SBR_WEAR) {
            g->look.stock = 1 + stockui_icon(&g->stock);
            g->browse = g->look;
            save_look(g);
            synth_sfx(SND_SELECT);
            badge_award(g, B_FASHION);
        } else if (r == SBR_BACK) {
            synth_sfx(SND_BACK);
            go(g, ST_CUSTOMIZE);
        }
        break;
    }
    case ST_TAGLINE: {
        int r = kb_step(&g->kb, edges);
        if (r == KB_DONE) {
            snprintf(g->tagline, sizeof(g->tagline), "%s", g->kb.text);
            save_title(g);
            synth_sfx(SND_SELECT);
        }
        if (r != KB_TYPING)
            go(g, ST_TITLE);
        break;
    }
    case ST_WORLD: {
        int r = world_step(g, input, edges);
        if (r == WR_BACK) {
            synth_sfx(SND_BACK);
            go(g, ST_TITLE);
        } else if (r == WR_PLAY || r == WR_PRACTICE) {
            char key[80];
            snprintf(key, sizeof(key), "file:%s", g->world.files[g->world.sel]);
            for (int i = 0; i < g->nlevels; i++)
                if (!strcmp(g->levels[i].key, key))
                    g->level_sel = i;
            g->racing = 0;
            g->world_run = 1;
            start_level(g, r == WR_PRACTICE);
        }
        break;
    }
    case ST_ICONS: {
        int r = iconmaker_step(&g->icons, input, edges);
        if (r == ICR_WEAR) {
            g->look.v[LOOK_FACE] = FACE_ICON0 + g->icons.icon;
            g->look.stock = 0;
            g->browse = g->look;
            save_look(g);
            synth_sfx(SND_SELECT);
            badge_award(g, B_FASHION);
        } else if (r == ICR_BACK) {
            synth_sfx(SND_BACK);
            go(g, ST_TITLE);
        }
        break;
    }
    case ST_SONGS:
        if (songmaker_step(&g->sm, input, edges)) {
            synth_sfx(SND_BACK);
            go(g, ST_TITLE);
        }
        break;
    case ST_STATS:
        if (edges & (IN_CIRCLE | IN_CROSS)) {
            synth_sfx(SND_BACK);
            go(g, ST_TITLE);
        }
        break;
    case ST_SETTINGS:
        step_settings(g, edges);
        break;
    case ST_BADGES:
        badges_step_screen(g, edges);
        if (edges & (IN_CIRCLE | IN_CROSS)) {
            synth_sfx(SND_BACK);
            go(g, ST_TITLE);
        }
        break;
    case ST_BOOK:
        if (secrets_step_book(g, edges)) {
            synth_sfx(SND_BACK);
            go(g, ST_TITLE);
        }
        break;
    case ST_SECRETS:
        if (secrets_step_page(g, edges)) {
            synth_sfx(SND_BACK);
            go(g, ST_SETTINGS);
        }
        break;
    }
}

// ---------------------------------------------------------------------------
// Rendering

static void draw_text_centered_plain(int y, int scale, const char *s, color_t c)
{
    draw_text((SCREEN_W - text_width(s, scale)) / 2, y, scale, s, c);
}

// Draws menu entries centred, one per row; the selected one is boxed and bright.
static void draw_menu(const char *const *items, int count, int sel, int y, int scale, int spacing)
{
    for (int i = 0; i < count; i++) {
        int yy = y + i * spacing;
        if (i == sel) {
            int w = text_width(items[i], scale) + 6 * scale;
            int h = 7 * scale + 4 * scale;
            draw_frame((SCREEN_W - w) / 2, yy - 2 * scale, w, h, 5, COL_CUBE);
            draw_text_glow_centered(yy, scale, items[i], COL_WHITE, COL_CUBE_CORE);
        } else {
            draw_text_centered_plain(yy, scale, items[i], COL_DIM);
        }
    }
}

static void draw_bar(int x, int y, int w, int pct, const char *label, color_t c)
{
    char buf[16];
    draw_text(x, y + 2, 4, label, RGB(220, 220, 255));
    int bx = x + 300;
    draw_rect(bx - 4, y - 4, w + 8, 38, col_scale(c, 1, 2));
    draw_rect(bx, y, w, 30, RGB(4, 2, 12));
    draw_rect(bx, y, w * pct / 100, 30, c);
    snprintf(buf, sizeof(buf), "%d%%", pct);
    draw_text_glow(bx + w + 30, y, 5, buf, COL_WHITE, c);
}

static void draw_star_count(int x, int y, int scale, int n)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", n);
    ui_star((float)x + 3.5f * scale, (float)y + 3.5f * scale, 4.5f * scale, RGB(200, 140, 0));
    ui_star((float)x + 3.5f * scale, (float)y + 3.5f * scale, 3.6f * scale, COL_YELLOW);
    draw_text_glow(x + 9 * scale, y, scale, buf, COL_WHITE, RGB(200, 140, 0));
}

// A coin icon: gold (or silver), or an empty slot.
static void draw_coin_icon(int cx, int cy, int r, int have, int silver)
{
    color_t rim = silver ? RGB(130, 140, 175) : RGB(205, 130, 0), face = silver ? RGB(215, 225, 245) : RGB(255, 212, 40);
    draw_circle(cx, cy, r, have ? rim : RGB(70, 60, 90));
    draw_circle(cx, cy, r * 3 / 4, have ? face : COL_DARK);
    if (have)
        draw_rect(cx - r / 8, cy - r / 2, r / 4 + 1, r, rim);
}

static void draw_coin_count(int x, int y, int scale, int n)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", n);
    draw_coin_icon(x + 7 * scale / 2, y + 7 * scale / 2, 4 * scale, 1, 0);
    draw_text_glow(x + 9 * scale, y, scale, buf, COL_WHITE, RGB(200, 140, 0));
}

static void draw_coin_row(int cx, int cy, int r, int n, int have, int silver)
{
    for (int k = 0; k < n; k++)
        draw_coin_icon(cx + (2 * k - n + 1) * (r * 5 / 4 + 4), cy, r, (have >> k) & 1, silver);
}

typedef struct {
    int stars;
    const char *name;
    color_t col;
} Difficulty;

static const Difficulty DIFFS[] = {
    { 1, "EASY", RGB(0, 200, 255) }, { 2, "NORMAL", RGB(60, 230, 60) }, { 3, "HARD", RGB(255, 200, 0) },
    { 4, "HARDER", RGB(255, 110, 30) }, { 6, "INSANE", RGB(255, 60, 200) }, { 8, "DEMON", RGB(230, 30, 30) },
};

static const Difficulty *difficulty(int stars)
{
    const Difficulty *d = &DIFFS[0];
    for (int i = 0; i < (int)(sizeof(DIFFS) / sizeof(DIFFS[0])); i++)
        if (stars >= DIFFS[i].stars)
            d = &DIFFS[i];
    return d;
}

// rank: 0 EASY .. 5 DEMON (the order of DIFFS and of RATING_), or RATING_UNBEATEN.
static void draw_rank_face(int cx, int cy, int rank)
{
    if (rank == RATING_UNBEATEN) {
        draw_circle(cx, cy, 66, RGB(70, 70, 90));
        draw_circle(cx, cy, 58, RGB(130, 130, 150));
        draw_text(cx - text_width("?", 10) / 2, cy - 35, 10, "?", COL_WHITE);
        return;
    }
    const Difficulty *d = &DIFFS[rank];
    float fx = (float)cx, fy = (float)cy;
    if (rank >= 5) {
        float lx[3] = { fx - 50, fx - 26, fx - 60 }, rx[3] = { fx + 50, fx + 26, fx + 60 };
        float hy[3] = { fy - 40, fy - 54, fy - 84 };
        draw_poly(3, lx, hy, d->col);
        draw_poly(3, rx, hy, d->col);
    }
    draw_circle(cx, cy, 66, col_scale(d->col, 1, 2));
    draw_circle(cx, cy, 58, d->col);
    draw_rect(cx - 30, cy - 22, 18, 22, COL_WHITE);
    draw_rect(cx + 12, cy - 22, 18, 22, COL_WHITE);
    draw_rect(cx - 24, cy - 14, 9, 12, RGB(0, 0, 0));
    draw_rect(cx + 18, cy - 14, 9, 12, RGB(0, 0, 0));
    if (rank >= 3) {
        float lx[4] = { fx - 36, fx - 8, fx - 8, fx - 36 }, rx[4] = { fx + 36, fx + 8, fx + 8, fx + 36 };
        float by[4] = { fy - 40, fy - 28, fy - 22, fy - 32 };
        draw_poly(4, lx, by, RGB(0, 0, 0));
        draw_poly(4, rx, by, RGB(0, 0, 0));
    }
    if (rank <= 1) {
        draw_rect(cx - 22, cy + 22, 44, 8, RGB(0, 0, 0));
        draw_rect(cx - 28, cy + 14, 8, 10, RGB(0, 0, 0));
        draw_rect(cx + 20, cy + 14, 8, 10, RGB(0, 0, 0));
    } else if (rank == 2) {
        draw_rect(cx - 22, cy + 22, 44, 8, RGB(0, 0, 0));
    } else {
        draw_rect(cx - 22, cy + 18, 44, 8, RGB(0, 0, 0));
        draw_rect(cx - 28, cy + 26, 8, 10, RGB(0, 0, 0));
        draw_rect(cx + 20, cy + 26, 8, 10, RGB(0, 0, 0));
    }
}

// The title cube wears his own icon: the one he wears (or his stock icon), or else the first he drew.
static Look title_look(const Game *g)
{
    Look l = g->look;
    for (int k = 0; k < NUM_ICONS && !FACE_IS_ICON(l.v[LOOK_FACE]) && !l.stock; k++)
        if (!icon_empty(k))
            l.v[LOOK_FACE] = FACE_ICON0 + k;
    return l;
}

static void draw_title(const Game *g)
{
    if (g->egg_idle >= EGG_IDLE_FRAMES && g->state == ST_TITLE) {
        rw_screensaver();
        return;
    }
    run_draw_backdrop(TITLE_COLORS, (float)g->frame * 6.0f, 0);
    rw_title_back();

    // Bouncing cube where the player stands, clear of the menu.
    int t = g->frame % 48;
    float hop = (float)(t * (48 - t)) * 0.45f;
    Look l = title_look(g);
    look_set(&l);
    run_draw_cube(PLAYER_X + PLAYER_S / 2.0f - 200, GROUND_Y - PLAYER_S / 2.0f - hop, (float)t * (PI_F / 48.0f));
    look_set(&g->look);

    int scale = 17;
    int wf = text_width("FRITZ", scale), wd = text_width("DASH", scale);
    int gapw = 6 * scale;
    int x = (SCREEN_W - (wf + gapw + wd)) / 2;
    if (!rw_title_logo(x, 34, scale, gapw)) {
        draw_text_glow(x, 34, scale, "FRITZ", RGB(200, 255, 170), COL_CUBE);
        draw_text_glow(x + wf + gapw, 34, scale, "DASH", RGB(255, 190, 240), COL_PINK);
    }
    draw_text_glow(x + wf + gapw + wd + 30, 34 + 7 * scale - 7 * 6, 6, "3.2", COL_YELLOW, RGB(200, 120, 0));
    if (g->tagline[0])
        draw_text_glow_centered(192, ui_fit_scale(g->tagline, SCREEN_W - 200, 6), g->tagline, COL_YELLOW, RGB(200, 120, 0));
    else
        draw_text_centered_plain(195, 3, "TRIANGLE: WRITE YOUR OWN WORDS HERE", RGB(130, 130, 170));
    draw_star_count(60, 40, 6, game_stars(g));
    draw_coin_count(60, 110, 5, game_coins(g));

    // PLAY on the left, MAKE on the right.
    static const char *const HEADS[2] = { "PLAY", "MAKE" };
    for (int c = 0; c < 2; c++) {
        int cx = c ? 1340 : 680, first = c ? TITLE_MAKE : 0, last = c ? TITLE_COUNT : TITLE_MAKE;
        draw_text_glow(cx - text_width(HEADS[c], 5) / 2, 268, 5, HEADS[c], c ? COL_PINK : COL_CUBE_CORE,
                       c ? RGB(120, 20, 90) : RGB(0, 80, 120));
        for (int i = first; i < last; i++) {
            int yy = 340 + (i - first) * 66, s = 6, tw = text_width(TITLE_ITEMS[i], s);
            if (i == g->title_sel) {
                draw_frame(cx - tw / 2 - 18, yy - 12, tw + 36, 7 * s + 24, 5, COL_CUBE);
                draw_text_glow(cx - tw / 2, yy, s, TITLE_ITEMS[i], COL_WHITE, COL_CUBE_CORE);
            } else {
                draw_text(cx - tw / 2, yy, s, TITLE_ITEMS[i], COL_DIM);
            }
        }
    }

    // His newest level and his records.
    char buf[96];
    const LevelInfo *nl = NULL;
    for (int i = 0; i < g->nlevels; i++)
        if (g->newest[0] && !strcmp(g->levels[i].file, g->newest))
            nl = &g->levels[i];
    if (nl) {
        snprintf(buf, sizeof(buf), "NEWEST LEVEL: %s", nl->name);
        draw_text_glow_centered(872, ui_fit_scale(buf, SCREEN_W - 200, 5), buf, COL_WHITE, COL_PINK);
    }
    int made = 0;
    for (int i = 0; i < g->nlevels; i++)
        made += g->levels[i].builtin < 0 && !strncmp(g->levels[i].file, "slot", 4);
    snprintf(buf, sizeof(buf), "LEVELS MADE %d   BADGES %d / %d   WINS %d   ENDLESS BEST %d", made, badges_count(g),
             BADGE_COUNT, g->stats[STAT_WINS], g->endless_best);
    draw_text_glow_centered(936, 4, buf, COL_YELLOW, RGB(200, 120, 0));
    draw_text_glow_centered(SCREEN_H - 70, 4, "D-PAD MOVE   X SELECT   TRIANGLE WRITE YOUR WORDS", RGB(220, 220, 255),
                            RGB(90, 90, 200));

    if (g->show_diag && diag_text[0])
        draw_text(24, SCREEN_H - 40, 3, diag_text, RGB(170, 170, 200));
}

// A crown over the boss: gold once it is beaten.
static void draw_crown(int cx, int cy, int s, int beaten)
{
    color_t c = beaten ? RGB(255, 205, 40) : RGB(110, 100, 140), edge = beaten ? RGB(170, 100, 0) : RGB(50, 44, 70);
    float x = (float)cx, y = (float)cy, f = (float)s;
    float ex[4] = { x - f - 4, x + f + 4, x + f + 4, x - f - 4 }, ey[4] = { y - 4, y - 4, y + f * 0.6f + 4, y + f * 0.6f + 4 };
    draw_poly(4, ex, ey, edge);
    float bx[4] = { x - f, x + f, x + f, x - f }, by[4] = { y, y, y + f * 0.6f, y + f * 0.6f };
    for (int k = 0; k < 3; k++) {
        float px = x + (float)(k - 1) * f * 0.75f, h = k == 1 ? f * 1.25f : f;
        float tx[3] = { px - f * 0.4f, px + f * 0.4f, px }, ty[3] = { y + 1, y + 1, y - h };
        draw_poly(3, tx, ty, c);
        draw_circle((int)px, (int)(y - h), s / 6 + 2, beaten ? RGB(255, 60, 120) : edge);
    }
    draw_poly(4, bx, by, c);
}

static void draw_world_card(const Game *g, int w, int x, int y, int cw, int ch, int on)
{
    LevelColors c = LWORLD_COLORS[w];
    LWorldStats st;
    game_lworld_stats(g, w, &st);
    char buf[48];
    int lift = on ? 10 : 0;
    y -= lift;
    if (on)
        draw_frame(x - 16, y - 16, cw + 32, ch + 32, 8, (g->frame / 10) % 2 ? COL_CUBE : RGB(200, 255, 170));
    draw_rect(x - 6, y - 6, cw + 12, ch + 12, on ? COL_WHITE : col_scale(c.line, 2, 3));
    static const color_t PRISM[6] = { RGB(255, 70, 90), RGB(255, 170, 40), RGB(250, 230, 60), RGB(70, 220, 120),
                                      RGB(50, 170, 255), RGB(170, 90, 255) };
    for (int k = 0; k < 12; k++) {
        color_t band = col_lerp(c.top, c.bottom, k * 256 / 11);
        if (w == LWORLD_SHAPESHIFT) {
            // SHAPESHIFT: a rainbow that runs slowly down the card, a little dark so the name reads.
            int h = (k * 128 + g->frame * 4) % (6 * 256);
            band = col_scale(col_lerp(PRISM[h / 256], PRISM[(h / 256 + 1) % 6], h % 256), 3, 4);
        }
        draw_rect(x, y + 130 * k / 12, cw, 130 * (k + 1) / 12 - 130 * k / 12, band);
    }
    draw_rect(x, y + 130, cw, ch - 130, RGB(10, 6, 26));
    draw_rect(x, y + 126, cw, 6, c.line);
    if (w == LWORLD_VAULT)
        snprintf(buf, sizeof(buf), "SECRET WORLD");
    else if (w < LWORLD_MINE)
        snprintf(buf, sizeof(buf), "WORLD %d", w + 1);
    else
        snprintf(buf, sizeof(buf), "MADE BY YOU");
    draw_text_glow(x + 18, y + 14, 3, buf, COL_WHITE, RGB(0, 0, 0));
    int ns = ui_fit_scale(LWORLD_NAMES[w], cw - 36, 6);
    draw_text_glow(x + (cw - text_width(LWORLD_NAMES[w], ns)) / 2, y + 88 - 7 * ns / 2, ns, LWORLD_NAMES[w], COL_WHITE,
                   col_scale(c.top, 1, 2));
    if (w == LWORLD_VAULT && !eggs_vault_open()) {
        // Locked: a padlock and how many eggs open it.
        int lx = x + 70, ly = y + 150;
        draw_circle(lx, ly + 10, 30, c.line);
        draw_circle(lx, ly + 10, 20, RGB(10, 6, 26));
        draw_rect(lx - 40, ly + 12, 80, 62, c.line);
        draw_rect(lx - 7, ly + 30, 14, 26, RGB(10, 6, 26));
        snprintf(buf, sizeof(buf), "%d EGGS", VAULT_EGGS);
        draw_text_glow(x + 130, y + 150, 4, "FIND", COL_WHITE, c.line);
        draw_text_glow(x + 130, y + 190, 4, buf, COL_YELLOW, c.line);
        int n = eggs_count(), bw = cw - 150;
        draw_rect(x + 24, y + ch - 52, bw, 30, RGB(4, 2, 12));
        draw_rect(x + 24, y + ch - 52, bw * (n < VAULT_EGGS ? n : VAULT_EGGS) / VAULT_EGGS, 30, c.line);
        snprintf(buf, sizeof(buf), "%d", n);
        draw_text_glow(x + 40 + bw, y + ch - 50, 4, buf, COL_WHITE, c.line);
        return;
    }
    if (st.boss >= 0) {
        draw_crown(x + cw - 64, y + 162, 26, g->levels[st.boss].best == 100);
        draw_text(x + cw - 64 - text_width("BOSS", 3) / 2, y + 196, 3, "BOSS",
                  g->levels[st.boss].best == 100 ? COL_YELLOW : COL_DIM);
    }
    if (w < LWORLD_MINE) {
        snprintf(buf, sizeof(buf), "%d/%d", st.stars, st.max_stars);
        ui_star((float)x + 44, (float)y + 168, 24, RGB(200, 140, 0));
        ui_star((float)x + 44, (float)y + 168, 19, COL_YELLOW);
        draw_text_glow(x + 80, y + 152, 5, buf, COL_WHITE, RGB(200, 140, 0));
    } else {
        snprintf(buf, sizeof(buf), "%d LEVEL%s", st.count, st.count == 1 ? "" : "S");
        draw_text_glow(x + 24, y + 152, ui_fit_scale(buf, cw - 150, 5), buf, COL_WHITE, c.line);
    }
    if (st.max_coins) {
        snprintf(buf, sizeof(buf), "%d/%d", st.coins, st.max_coins);
        draw_coin_icon(x + 44, y + 226, 16, 1, w == LWORLD_MINE);
        draw_text(x + 80, y + 212, 4, buf, COL_WHITE);
    }
    int pct = st.count ? st.beaten * 100 / st.count : 0, bw = cw - 150;
    draw_rect(x + 24, y + ch - 52, bw, 30, RGB(4, 2, 12));
    draw_rect(x + 24, y + ch - 52, bw * pct / 100, 30, pct == 100 ? COL_CUBE : c.line);
    draw_frame(x + 22, y + ch - 54, bw + 4, 34, 2, col_scale(c.line, 1, 2));
    snprintf(buf, sizeof(buf), "%d%%", pct);
    draw_text_glow(x + 40 + bw, y + ch - 50, 4, buf, COL_WHITE, c.line);
}

static void draw_picker(const Game *g)
{
    run_draw_backdrop(LWORLD_COLORS[g->lworld_sel], (float)g->frame * 3.0f, 0);
    draw_text_glow_centered(46, 9, g->two ? "2 PLAYER RACE" : "PICK A WORLD", COL_WHITE, LWORLD_COLORS[g->lworld_sel].line);
    draw_star_count(60, 50, 5, game_stars(g));
    draw_coin_count(60, 110, 4, game_coins(g));
    int shown[NUM_LWORLDS], n = 0;
    for (int w = 0; w < NUM_LWORLDS; w++)
        if (lworld_shown(g, w))
            shown[n++] = w;
    int cols = pick_cols(n), cw = cols > 4 ? 340 : 400, ch = 330, gap = cols > 4 ? 30 : 44;
    for (int k = 0; k < n; k++) {
        int row = k / cols, inrow = n - row * cols < cols ? n - row * cols : cols;
        int x0 = (SCREEN_W - (inrow * cw + (inrow - 1) * gap)) / 2;
        draw_world_card(g, shown[k], x0 + (k % cols) * (cw + gap), 196 + row * 384, cw, ch, shown[k] == g->lworld_sel);
    }
    if (g->msg_frames > 0)
        draw_text_glow_centered(SCREEN_H - 140, 5, g->msg, COL_YELLOW, COL_PINK);
    draw_text_glow_centered(SCREEN_H - 80, 4, "D-PAD CHOOSE A WORLD   X GO IN   CIRCLE BACK", RGB(220, 220, 255),
                            RGB(90, 90, 200));
}

static void draw_levels(const Game *g)
{
    if (g->picking) {
        draw_picker(g);
        return;
    }
    const LevelInfo *li = &g->levels[g->level_sel];
    LWorldStats st;
    game_lworld_stats(g, li->world, &st);
    run_draw_backdrop(li->colors, (float)g->frame * 3.0f, 0);
    const char *title = g->two ? "2 PLAYER RACE" : LWORLD_NAMES[li->world];
    draw_text_glow_centered(56, ui_fit_scale(title, SCREEN_W - 700, 9), title, COL_WHITE, li->colors.line);
    if (g->two)
        draw_text_glow_centered(132, 4, LWORLD_NAMES[li->world], COL_YELLOW, RGB(200, 120, 0));
    draw_star_count(60, 50, 5, game_stars(g));
    draw_coin_count(60, 110, 4, game_coins(g));

    int w = 1200, h = 560, x = (SCREEN_W - w) / 2, y = 190;
    ui_panel(x, y, w, h, li->colors.line);
    int face = li->builtin >= 0 || li->rating >= 0;
    int text_x = face ? x + 220 : x;
    int text_w = face ? w - 240 : w;
    int ns = ui_fit_scale(li->name, text_w - (li->verified ? 260 : 60), 11);
    draw_text_glow(text_x + (text_w - text_width(li->name, ns)) / 2, y + 60, ns, li->name, COL_WHITE, li->colors.line);

    char buf[64];
    if (li->builtin >= 0) {
        const Difficulty *d = difficulty(li->stars);
        draw_rank_face(x + 130, y + 130, (int)(d - DIFFS));
        draw_text(x + 130 - text_width(d->name, 4) / 2, y + 215, 4, d->name, d->col);
        draw_star_count(text_x + text_w / 2 - 60, y + 150, 6, li->stars);
        if (li->best == 100)
            draw_text_glow(text_x + text_w / 2 + 70, y + 158, 4, "EARNED!", COL_CUBE, RGB(30, 140, 10));
        draw_coin_row(text_x + text_w / 2, y + 222, 20, li->ncoins, li->coins, 0);
        if (st.boss == g->level_sel) {
            draw_crown(x + w - 110, y + 204, 40, li->best == 100);
            draw_text_glow(x + w - 110 - text_width("BOSS", 4) / 2, y + 246, 4, "BOSS", COL_YELLOW, RGB(200, 120, 0));
        }
    } else {
        snprintf(buf, sizeof(buf), "BY %s", li->author[0] ? li->author : "SOMEONE");
        draw_text_glow(text_x + (text_w - text_width(buf, 5)) / 2, y + 150, 5, buf, COL_YELLOW, RGB(200, 120, 0));
        draw_coin_row(text_x + text_w / 2, y + 222, 20, li->ncoins, li->coins, 1);
        if (li->rating >= 0) {
            const char *rn = RATING_NAMES[li->rating];
            color_t rc = li->rating < RATING_UNBEATEN ? DIFFS[li->rating].col : RGB(170, 170, 190);
            draw_rank_face(x + 130, y + 130, li->rating);
            draw_text(x + 130 - text_width(rn, 4) / 2, y + 215, 4, rn, rc);
        }
        if (li->verified) {
            // A green tick: beaten from the start by whoever made it.
            int tx = x + w - 110, ty = y + 176;
            float ax[4] = { (float)tx - 34, (float)tx - 22, (float)tx - 6, (float)tx - 18 };
            float ay[4] = { (float)ty, (float)ty - 12, (float)ty + 4, (float)ty + 16 };
            float bx[4] = { (float)tx - 18, (float)tx + 26, (float)tx + 38, (float)tx - 6 };
            float by[4] = { (float)ty + 16, (float)ty - 28, (float)ty - 16, (float)ty + 28 };
            draw_circle(tx, ty, 56, RGB(30, 140, 10));
            draw_circle(tx, ty, 48, COL_CUBE);
            draw_poly(4, ax, ay, COL_DARK);
            draw_poly(4, bx, by, COL_DARK);
            draw_text(tx - text_width("VERIFIED", 3) / 2, ty + 62, 3, "VERIFIED", COL_CUBE);
        }
    }

    static const char *const SPEEDS[4] = { "0.5X", "1X", "2X", "3X" };
    snprintf(buf, sizeof(buf), "SPEED %s   %d OBJECTS   %d ATTEMPTS", SPEEDS[li->speed & 3], li->nobj, li->attempts);
    draw_text_centered_plain(y + 260, 4, buf, RGB(200, 200, 235));

    draw_bar(x + 120, y + 330, 560, li->best, "NORMAL", COL_CUBE_CORE);
    draw_bar(x + 120, y + 410, 560, li->practice_best, "PRACTICE", COL_CUBE);

    if (g->nlevels > 1) {
        draw_text_glow(x - 150, y + h / 2 - 50, 14, "<", COL_WHITE, li->colors.line);
        draw_text_glow(x + w + 70, y + h / 2 - 50, 14, ">", COL_WHITE, li->colors.line);
    }
    int at = 1;
    for (int i = 0; i < g->level_sel; i++)
        at += g->levels[i].world == li->world;
    snprintf(buf, sizeof(buf), "%d / %d", at, st.count);
    draw_text_glow_centered(y + h + 40, 5, buf, COL_WHITE, li->colors.line);
    if (g->bad_files) {
        snprintf(buf, sizeof(buf), "%d LEVEL FILE%s COULD NOT BE READ", g->bad_files, g->bad_files > 1 ? "S" : "");
        draw_text_centered_plain(y + h + 100, 3, buf, COL_RED);
    }
    if (g->two) {
        int s = !g->p2_here && g->msg_frames > 0 && (g->msg_frames / 8) % 2 ? 7 : 5;
        draw_text_glow_centered(y + h + 100 - s * 3, s, g->p2_here ? "CONTROLLER 2 IS READY!" : "TURN ON A SECOND CONTROLLER",
                                g->p2_here ? COL_CUBE : COL_YELLOW, g->p2_here ? RGB(30, 140, 10) : RGB(200, 120, 0));
        draw_text_glow_centered(SCREEN_H - 90, 4, "LEFT RIGHT CHOOSE   X RACE   CIRCLE WORLDS", RGB(220, 220, 255),
                                RGB(90, 90, 200));
    } else {
        draw_text_glow_centered(SCREEN_H - 90, 4, "LEFT RIGHT CHOOSE   X PLAY   SQUARE PRACTICE   TRIANGLE REMIX   CIRCLE WORLDS",
                                RGB(220, 220, 255), RGB(90, 90, 200));
    }
    if (g->msg_frames > 0 && !g->two)
        draw_text_glow_centered(SCREEN_H - 160, 5, g->msg, COL_YELLOW, COL_PINK);
    if (g->remix_ask) {
        int pw = 1300, ph = 420, px = (SCREEN_W - pw) / 2, py = 300;
        ui_panel(px, py, pw, ph, COL_PINK);
        draw_text_glow_centered(py + 40, 9, "REMIX IT?", COL_WHITE, COL_PINK);
        snprintf(buf, sizeof(buf), "%s GOES INTO YOUR EDITOR", li->name);
        draw_text_glow_centered(py + 160, ui_fit_scale(buf, pw - 80, 5), buf, COL_YELLOW, RGB(200, 120, 0));
        draw_text_centered_plain(py + 230, 4, "IN AN EMPTY SLOT, SO YOU CAN CHANGE ANYTHING", RGB(200, 200, 235));
        draw_text_glow_centered(py + 320, 5, "X YES, REMIX IT   CIRCLE NO", COL_WHITE, COL_CUBE_CORE);
    }
}

static const char *run_name(const Game *g)
{
    return g->playtest ? level.name : g->levels[g->level_sel].name;
}

// The X button lights up while the hint ghost holds it: press when it does.
static void draw_hint_button(const Game *g)
{
    int on = g->hint_t > 0 && (g->hint.trace[g->hint_t - 1] & IN_CROSS);
    int cx = PLAYER_X + PLAYER_S / 2, cy = 170, r = on ? 50 : 44;
    draw_circle(cx, cy, r + 8, on ? RGB(150, 190, 255) : RGB(40, 50, 90));
    draw_circle(cx, cy, r, on ? RGB(50, 100, 255) : RGB(20, 24, 50));
    color_t c = on ? COL_WHITE : RGB(110, 130, 190);
    float k = (float)r * 0.5f, t = 6.0f, x = (float)cx, y = (float)cy;
    float ax[4] = { x - k - t, x - k + t, x + k + t, x + k - t }, ay[4] = { y - k, y - k, y + k, y + k };
    float bx[4] = { x + k - t, x + k + t, x - k + t, x - k - t };
    draw_poly(4, ax, ay, c);
    draw_poly(4, bx, ay, c);
    draw_text_glow(cx + r + 24, cy - 14, 4, on ? "NOW!" : "WATCH", on ? COL_WHITE : RGB(170, 180, 220),
                   on ? RGB(50, 100, 255) : RGB(40, 50, 90));
}

static void draw_intro(const Game *g)
{
    // "LEVEL NAME" and "BY AUTHOR" slide in on the first attempt, then away.
    int f = g->run.state_frames;
    if (g->run.attempt != 1 || g->run.state != RUN_PLAY || f > 150)
        return;
    int off = f < 20 ? (20 - f) * 90 : f > 130 ? (f - 130) * 90 : 0;
    const char *name = run_name(g);
    char by[48];
    snprintf(by, sizeof(by), "BY %s", level.author[0] ? level.author : "SOMEONE");
    int s = ui_fit_scale(name, SCREEN_W - 160, 12);
    draw_text_glow((SCREEN_W - text_width(name, s)) / 2 - off, 200, s, name, COL_WHITE, level.colors.line);
    draw_text_glow((SCREEN_W - text_width(by, 6)) / 2 + off, 230 + 7 * s, 6, by, COL_YELLOW, RGB(200, 120, 0));
}

static void draw_pause(const Game *g)
{
    int w = 1000, h = 720, x = (SCREEN_W - w) / 2, y = 170;
    ui_panel(x, y, w, h, COL_CUBE_CORE);
    draw_text_glow_centered(y + 40, 12, "PAUSED", RGB(200, 250, 255), COL_CUBE_CORE);
    draw_text_centered_plain(y + 150, 4, run_name(g), COL_YELLOW);

    const char *items[PAUSE_COUNT] = { "RESUME", "RESTART", g->run.practice ? "PRACTICE: ON" : "PRACTICE: OFF",
                                       g->playtest == PLAYTEST_LAB ? "BACK TO MODE LAB" : g->playtest ? "BACK TO EDITOR" : "EXIT" };
    if (g->racing) {
        const char *race_items[3] = { "RESUME", "RESTART RACE", "EXIT" };
        draw_menu(race_items, 3, g->pause_sel, y + 230, 7, 100);
    } else {
        draw_menu(items, PAUSE_COUNT, g->pause_sel, y + 230, 7, 100);
    }

    if (g->verifying)
        draw_text_centered_plain(y + h - 110, 4, g->run.practice ? "PRACTICE IS ON: THIS RUN WILL NOT VERIFY" :
                                 "BEAT IT FROM THE START TO VERIFY IT", COL_YELLOW);
    if (!g->playtest && !g->racing) {
        const LevelInfo *li = &g->levels[g->level_sel];
        char buf[64];
        snprintf(buf, sizeof(buf), "BEST %d%%   PRACTICE %d%%", li->best, li->practice_best);
        draw_text_centered_plain(y + h - 110, 4, buf, RGB(200, 200, 235));
    }
    draw_text_glow_centered(y + h - 55, 4, "D-PAD MOVE   X SELECT   OPTIONS RESUME",
                            RGB(220, 220, 255), RGB(90, 90, 200));
}

static void draw_complete(const Game *g)
{
    int w = 1100, h = 620, x = (SCREEN_W - w) / 2, y = 200;
    ui_panel(x, y, w, h, COL_CUBE);
    draw_text_glow_centered(y + 50, 11, g->just_verified ? "VERIFIED!" : g->run.practice ? "PRACTICE DONE!" : "LEVEL COMPLETE!",
                            RGB(220, 255, 200), COL_CUBE);
    const char *name = run_name(g);
    draw_text_glow_centered(y + 190, ui_fit_scale(name, w - 80, 7), name, COL_WHITE, level.colors.line);

    char buf[48];
    snprintf(buf, sizeof(buf), "ATTEMPTS %d   JUMPS %d", g->run.attempt, g->run.jumps);
    draw_text_glow_centered(y + 280, 6, buf, COL_WHITE, COL_CUBE_CORE);
    if (level.ncoins) {
        int have = g->run.practice ? g->run.b.coins : g->run.b.coins | g->run.coins_had;
        int pop = g->new_coins && (g->frame / 8) % 2 ? 4 : 0;
        draw_coin_row(SCREEN_W / 2, y + 370, 26 + pop, level.ncoins, have, g->run.coins_silver);
    }
    if (g->world_result) {
        int s = 7 + (g->frame / 8) % 2;
        draw_text_glow_centered(y + 420, s, g->world_result == 2 ? "YOU BEAT FRITZ WORLD!" : "NEXT LEVEL OPEN!",
                                COL_YELLOW, RGB(200, 120, 0));
    } else if (g->new_stars > 0) {
        snprintf(buf, sizeof(buf), "+%d STARS!", g->new_stars);
        int s = 8 + (g->frame / 8) % 2;
        int bw = text_width(buf, s) + 12 * s;
        ui_star((float)((SCREEN_W - bw) / 2 + 4 * s), (float)(y + 420 + 3 * s), 5.0f * (float)s, COL_YELLOW);
        draw_text_glow((SCREEN_W - bw) / 2 + 10 * s, y + 420, s, buf, COL_YELLOW, RGB(200, 120, 0));
    } else if (g->new_coins) {
        snprintf(buf, sizeof(buf), "+%d COIN%s!", bits(g->new_coins), bits(g->new_coins) > 1 ? "S" : "");
        draw_text_glow_centered(y + 420, 8, buf, COL_YELLOW, RGB(200, 120, 0));
    }
    if (g->state_frames > 30 && (g->frame / 30) % 2 == 0)
        draw_text_glow_centered(y + 500, 5, "X PLAY AGAIN", COL_WHITE, COL_CUBE_CORE);
    if (g->state_frames > 30)
        draw_text_centered_plain(y + 565, 4, g->playtest == PLAYTEST_LAB ? "CIRCLE FOR MODE LAB" : g->playtest ? "CIRCLE FOR THE EDITOR" : g->world_run ? "CIRCLE FOR FRITZ WORLD" :
                                 "CIRCLE FOR LEVELS",
                                 RGB(170, 170, 210));
}

static void draw_race_bar(int x, int y, const char *who, const Run *r, color_t c)
{
    char buf[32];
    int w = 600, pct = r->state == RUN_DONE ? 100 : r->pct;
    draw_text_glow(x, y - 4, 5, who, COL_WHITE, c);
    draw_rect(x + 95, y - 4, w + 8, 30, col_scale(c, 1, 2));
    draw_rect(x + 99, y, w, 22, COL_DARK);
    draw_rect(x + 99, y, w * pct / 100, 22, c);
    snprintf(buf, sizeof(buf), "FALLS %d", r->attempt - 1);
    draw_text(x + 99, y + 40, 4, buf, RGB(220, 220, 255));
}

static void draw_race(const Game *g)
{
    const Run *lead = race_leader(g), *back = lead == &g->run ? &g->run2 : &g->run;
    look_set(&lead->look);
    run_set_rival(back);
    run_draw(lead, 0);
    run_set_rival(NULL);
    look_set(&g->look);
    int close = g->run.b.x - g->run2.b.x < 70 && g->run2.b.x - g->run.b.x < 70 && g->run.b.y - g->run2.b.y < 70 &&
                g->run2.b.y - g->run.b.y < 70;
    for (int p = 0; p < 2; p++) {
        const Run *r = p ? &g->run2 : &g->run;
        int x, y;
        if (r->state == RUN_DEAD)
            continue;
        run_screen_pos(r->b.x, r->b.y + run_half_h(&r->b), &x, &y);
        x += close ? (p ? 36 : -36) : 0;
        draw_text_glow(x - text_width("P1", 4) / 2, y - 60, 4, p ? "P2" : "P1", COL_WHITE, LOOK_COLORS[r->look.v[LOOK_COL1]]);
    }
    draw_race_bar(60, 40, "P1", &g->run, LOOK_COLORS[g->run.look.v[LOOK_COL1]]);
    draw_race_bar(SCREEN_W - 770, 40, "P2", &g->run2, LOOK_COLORS[g->run2.look.v[LOOK_COL1]]);
    if (g->race_t < RACE_COUNT + 40) {
        static const char *const COUNT[4] = { "3", "2", "1", "GO!" };
        int k = g->race_t < RACE_COUNT ? g->race_t / 60 : 3, f = g->race_t % 60;
        draw_text_glow_centered(300 - (f < 10 ? (10 - f) * 6 : 0), 24, COUNT[k], COL_WHITE, k == 3 ? COL_CUBE : COL_PINK);
    }
    if (g->winner >= 0) {
        const char *msg = g->winner == 0 ? "TIE!" : g->winner == 1 ? "P1 WINS!" : "P2 WINS!";
        draw_text_glow_centered(300, 14, msg, COL_YELLOW, COL_PINK);
    }
}

static void draw_race_done(const Game *g)
{
    int w = 1100, h = 560, x = (SCREEN_W - w) / 2, y = 240;
    const char *msg = g->winner == 0 ? "IT'S A TIE!" : g->winner == 1 ? "PLAYER 1 WINS!" : "PLAYER 2 WINS!";
    color_t c = g->winner == 2 ? LOOK_COLORS[g->run2.look.v[LOOK_COL1]] : LOOK_COLORS[g->run.look.v[LOOK_COL1]];
    ui_panel(x, y, w, h, c);
    draw_text_glow_centered(y + 50, ui_fit_scale(msg, w - 80, 11), msg, COL_WHITE, c);
    draw_text_glow_centered(y + 180, ui_fit_scale(level.name, w - 80, 6), level.name, COL_YELLOW, RGB(200, 120, 0));
    char buf[64];
    snprintf(buf, sizeof(buf), "P1 FELL %d TIMES   P2 FELL %d TIMES", g->run.attempt - 1, g->run2.attempt - 1);
    draw_text_glow_centered(y + 290, 5, buf, COL_WHITE, COL_CUBE_CORE);
    if (g->state_frames > 30 && (g->frame / 30) % 2 == 0)
        draw_text_glow_centered(y + 400, 5, g->p2_here ? "X RACE AGAIN" : "TURN ON CONTROLLER 2", COL_WHITE, COL_CUBE_CORE);
    if (g->state_frames > 30)
        draw_text_centered_plain(y + 470, 4, "CIRCLE FOR LEVELS", RGB(170, 170, 210));
}

static void draw_settings(const Game *g)
{
    run_draw_backdrop(TITLE_COLORS, (float)g->frame * 2.0f, 0);
    int w = 1400, h = 820, x = (SCREEN_W - w) / 2, y = 130;
    ui_panel(x, y, w, h, COL_CUBE_CORE);
    draw_text_glow_centered(y + 36, 11, "SETTINGS", RGB(200, 250, 255), COL_CUBE_CORE);

    char music_item[32], sfx_item[32];
    snprintf(music_item, sizeof(music_item), "MUSIC: < %d >", g->music_vol);
    snprintf(sfx_item, sizeof(sfx_item), "SOUND EFFECTS: < %d >", g->sfx_vol);
    const char *items[SET_COUNT] = {
        g->show_progress ? "PROGRESS BAR: ON" : "PROGRESS BAR: OFF",
        g->auto_checkpoints ? "AUTO CHECKPOINTS: ON" : "AUTO CHECKPOINTS: OFF",
        music_item,
        sfx_item,
        g->clean_hud ? "RECORDING MODE: ON" : "RECORDING MODE: OFF",
        g->show_ghost ? "GHOST: ON" : "GHOST: OFF",
        g->practice_song ? "PRACTICE MUSIC: CHILL" : "PRACTICE MUSIC: LEVEL SONG",
        "SECRETS",
        "BACK",
    };
    draw_menu(items, SET_COUNT, g->settings_sel, y + 140, 6, 60);

    static const char *const HELP[SET_COUNT] = {
        "SHOWS HOW FAR THROUGH THE LEVEL YOU ARE",
        "PRACTICE MODE ADDS A CHECKPOINT EVERY 2 SECONDS",
        "LEFT AND RIGHT CHANGE THE MUSIC VOLUME",
        "LEFT AND RIGHT CHANGE THE SOUND VOLUME",
        "HIDES THE BAR AND TEXT FOR VIDEOS. THE SHARE BUTTON RECORDS",
        "YOUR BEST RUN PLAYS ALONG BESIDE YOU",
        "A CALM SONG FOR PRACTISING, OR THE LEVEL'S OWN SONG",
        "TURN THE EASTER EGG SURPRISES ON AND OFF",
        "",
    };
    draw_text_centered_plain(y + h - 120, 4, HELP[g->settings_sel], RGB(200, 200, 235));
    draw_text_glow_centered(y + h - 60, 4, "X CHANGE   CIRCLE BACK", RGB(220, 220, 255), RGB(90, 90, 200));
}

static void draw_customize(const Game *g)
{
    run_draw_backdrop(LEVEL_PALETTES[7], (float)g->frame * 2.0f, 0);
    draw_text_glow_centered(40, 9, "CUSTOMIZE", COL_WHITE, COL_PINK);
    int stars = game_stars(g);
    draw_star_count(60, 50, 5, stars);
    draw_coin_count(60, 110, 4, game_coins(g));

    int x = 80, y = 180, w = 980;
    ui_panel(x, y, w, 720, COL_PINK);
    for (int r = 0; r < LOOK_PARTS; r++) {
        int yy = y + 40 + r * 112, on = r == g->custom_row;
        const LookOption *o = &LOOK_OPTIONS[r][g->browse.v[r]];
        if (r == LOOK_FACE && g->browse.stock)
            o = &STOCK[g->browse.stock - 1].opt;
        int locked = o == &LOOK_OPTIONS[r][g->browse.v[r]] && (look_locked(g, o) || icon_locked(r, g->browse.v[r]));
        if (on)
            draw_frame(x + 16, yy - 18, w - 32, 96, 5, COL_CUBE);
        draw_text(x + 40, yy + 8, 4, LOOK_ROWS[r], on ? COL_WHITE : COL_DIM);
        char buf[48];
        snprintf(buf, sizeof(buf), "< %s >", o->name);
        draw_text_glow(x + 360, yy, 6, buf, locked ? RGB(120, 110, 140) : COL_WHITE,
                       locked ? RGB(60, 50, 80) : COL_CUBE_CORE);
        if (locked && icon_locked(r, g->browse.v[r])) {
            draw_text(x + 380, yy + 52, 3, "DRAW IT IN THE ICON MAKER FIRST", COL_RED);
        } else if (locked && o->badge && !(g->badges >> (o->badge - 1) & 1)) {
            draw_badge_medal(x + w - 100, yy + 22, 24, o->badge - 1, 0);
        } else if (locked && o->coins > game_coins(g)) {
            snprintf(buf, sizeof(buf), "%d", o->coins);
            draw_coin_icon(x + w - 120, yy + 22, 18, 1, 0);
            draw_text(x + w - 92, yy + 10, 4, buf, COL_RED);
        } else if (locked) {
            snprintf(buf, sizeof(buf), "%d", o->stars);
            ui_star((float)(x + w - 120), (float)(yy + 22), 20, RGB(140, 120, 40));
            draw_text(x + w - 92, yy + 10, 4, buf, COL_RED);
        } else if (r <= LOOK_COL2) {
            draw_rect(x + w - 130, yy - 4, 70, 56, LOOK_COLORS[g->browse.v[r]]);
        }
        if (r == LOOK_FACE && on && !locked) {
            snprintf(buf, sizeof(buf), "TRIANGLE: %d STOCK ICONS", STOCK_COUNT);
            draw_text(x + 380, yy + 50, 3, buf, COL_YELLOW);
        }
    }

    ui_panel(1140, 180, 700, 720, COL_CUBE_CORE);
    look_set(&g->browse);
    look_draw_preview(1440, 470, 760, g->frame);
    fx_draw(g->preview, 0, 0, COL_DARK);
    look_set(&g->look);
    if (g->msg_frames > 0)
        draw_text_glow_centered(SCREEN_H - 150, 5, g->msg, COL_YELLOW, COL_PINK);
    draw_text_glow_centered(SCREEN_H - 90, 4, "UP DOWN PICK   LEFT RIGHT CHANGE   TRIANGLE STOCK ICONS   CIRCLE BACK",
                            RGB(220, 220, 255), RGB(90, 90, 200));
}

static void draw_stats(const Game *g)
{
    run_draw_backdrop(LEVEL_PALETTES[1], (float)g->frame * 2.0f, 0);
    int w = 1300, h = 800, x = (SCREEN_W - w) / 2, y = 140;
    ui_panel(x, y, w, h, COL_CUBE_CORE);
    draw_text_glow_centered(y + 36, 11, "STATS", COL_WHITE, COL_CUBE_CORE);

    int beaten = 0, builtins = 0, made = 0, max_stars = 0, max_coins = 0;
    for (int i = 0; i < g->nlevels; i++) {
        const LevelInfo *li = &g->levels[i];
        if (li->builtin >= 0) {
            builtins++;
            max_stars += li->stars;
            max_coins += li->ncoins;
            beaten += li->best == 100;
        } else if (!strncmp(li->file, "slot", 4)) {
            made++;
        }
    }
    int secs = g->stats[STAT_SECONDS];
    char rows[10][64];
    snprintf(rows[0], 64, "STARS  %d / %d   COINS  %d / %d", game_stars(g), max_stars, game_coins(g), max_coins);
    snprintf(rows[1], 64, "LEVELS BEATEN  %d / %d", beaten, builtins);
    snprintf(rows[2], 64, "LEVELS MADE  %d", made);
    snprintf(rows[3], 64, "ATTEMPTS  %d", g->stats[STAT_ATTEMPTS]);
    snprintf(rows[4], 64, "JUMPS  %d", g->stats[STAT_JUMPS]);
    snprintf(rows[5], 64, "ORBS HIT  %d", g->stats[STAT_ORBS]);
    snprintf(rows[6], 64, "WINS  %d   PRACTICE WINS  %d", g->stats[STAT_WINS], g->stats[STAT_PRACTICE_WINS]);
    snprintf(rows[7], 64, "TIME PLAYED  %dH %02dM", secs / 3600, secs / 60 % 60);
    snprintf(rows[8], 64, "ENDLESS BEST  %d", g->endless_best);
    snprintf(rows[9], 64, "BADGES  %d / %d", badges_count(g), BADGE_COUNT);
    for (int i = 0; i < 10; i++)
        draw_text_glow_centered(y + 150 + i * 58, 5, rows[i], i ? COL_WHITE : COL_YELLOW,
                                i ? COL_CUBE_CORE : RGB(200, 120, 0));
    draw_text_glow_centered(y + h - 50, 4, "CIRCLE BACK", RGB(220, 220, 255), RGB(90, 90, 200));
}

void game_draw(const Game *g)
{
    int hud = g->clean_hud ? 0 : (g->show_progress ? HUD_PROGRESS : 0) | HUD_TEXT;
    draw_set_offset(0, 0);
    look_set(&g->look);
    rewards_frame(g->frame);
    switch (g->state) {
    case ST_TITLE:
        draw_title(g);
        break;
    case ST_TAGLINE:
        draw_title(g);
        kb_draw(&g->kb, "WRITE YOUR WORDS FOR THE TITLE", COL_PINK, g->frame);
        break;
    case ST_LEVELS:
        draw_levels(g);
        break;
    case ST_LAB:
        lab_draw(g);
        break;
    case ST_PLAY:
        run_set_deaths(g->playtest ? NULL : (const int (*)[2])g->deaths, g->ndeaths);
        if (g->hint_t >= 0 && g->hint_t < HINT_FRAMES && g->hint_t < g->hint.len)
            run_set_ghost(&g->hint_run, 1), run_set_hint_path((const float (*)[2])g->hint_path, g->hint_npath);
        else
            run_set_ghost(g->ghost_t >= 0 ? &g->ghost_run : NULL, 0);
        run_draw(&g->run, hud);
        run_set_ghost(NULL, 0);
        run_set_hint_path(NULL, 0);
        draw_intro(g);
        if (g->hint.state == HINT_SEARCH)
            draw_text_glow_centered(320, 8, "THINKING...", COL_WHITE, COL_CUBE_CORE);
        if (g->verifying && !g->run.practice && (hud & HUD_TEXT))
            draw_text_glow(40, 28, 5, "VERIFY RUN", COL_YELLOW, RGB(200, 120, 0));
        if (g->hint_t >= 0 && g->hint_t < HINT_FRAMES && g->hint_t < g->hint.len)
            draw_hint_button(g);
        if (g->msg_frames > 0)
            draw_text_glow_centered(320, 6, g->msg, COL_YELLOW, COL_PINK);
        if (g->run.state == RUN_DONE) {
            int rise = g->run.state_frames < 20 ? (20 - g->run.state_frames) * 12 : 0;
            draw_text_glow_centered(300 + rise, 13, g->run.practice ? "PRACTICE DONE!" : "LEVEL COMPLETE!",
                                    COL_YELLOW, COL_PINK);
        }
        break;
    case ST_PAUSE:
        if (g->racing) {
            draw_race(g);
        } else {
            run_set_deaths(g->playtest ? NULL : (const int (*)[2])g->deaths, g->ndeaths);
            run_set_ghost(g->ghost_t >= 0 ? &g->ghost_run : NULL, 0);
            run_draw(&g->run, hud);
            run_set_ghost(NULL, 0);
        }
        draw_pause(g);
        break;
    case ST_RACE:
        draw_race(g);
        break;
    case ST_RACE_DONE:
        draw_race(g);
        draw_race_done(g);
        break;
    case ST_COMPLETE:
        run_draw(&g->run, hud);
        draw_complete(g);
        break;
    case ST_ENDLESS:
        endless_draw(&g->endless);
        break;
    case ST_EDITOR:
        editor_draw(&g->ed, &level, g->frame);
        break;
    case ST_CUSTOMIZE:
        draw_customize(g);
        break;
    case ST_STOCK:
        stockui_draw(&g->stock, &g->look, game_stars(g), game_coins(g), g->badges, g->frame);
        break;
    case ST_SONGS:
        songmaker_draw(&g->sm, g->frame);
        break;
    case ST_ICONS:
        iconmaker_draw(&g->icons, &g->look, g->frame);
        break;
    case ST_WORLD:
        world_draw(g);
        break;
    case ST_STATS:
        draw_stats(g);
        break;
    case ST_SETTINGS:
        draw_settings(g);
        break;
    case ST_BADGES:
        badges_draw_screen(g);
        break;
    case ST_BOOK:
        secrets_draw_book(g);
        break;
    case ST_SECRETS:
        secrets_draw_page(g);
        break;
    }
    if (!(g->clean_hud && g->state == ST_PLAY)) {
        badges_draw_toast(g);
        eggs_draw_toast(g);
    }
}
