// Fritz Dash - easter eggs: the registry of level eggs, the game eggs and
// what finds them, the rewards and what unlocks them, eggs.txt and the
// cracking-egg toast. The SECRETS book and page are secrets.c; the rewards'
// drawing is rewards.c.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "badges.h"
#include "eggs.h"
#include "game.h"
#include "mode.h"
#include "platform.h"
#include "synth.h"

const char *const REWARD_KIND_NAMES[RK_COUNT] = { "BACKGROUND", "TRAIL", "DEATH", "CUBE FINISH", "SCREEN FILTER",
                                                  "PET", "SOUNDS", "MENU MUSIC", "TITLE", "PARTY", "SECRET ICON",
                                                  "WORLD" };

// Every reward. The keys are saved: never change or reuse one.
const Reward REWARDS[] = {
    { "bg_warp", "STARFIELD WARP", RK_BG, BGX_WARP },
    { "bg_aurora", "AURORA SKY", RK_BG, BGX_AURORA },
    { "bg_bubbles", "UNDERWATER BUBBLES", RK_BG, BGX_BUBBLES },
    { "bg_code", "CODE RAIN", RK_BG, BGX_CODE },
    { "bg_snow", "SNOWFALL", RK_BG, BGX_SNOW },
    { "bg_fireworks", "FIREWORKS SKY", RK_BG, BGX_FIREWORKS },
    { "bg_retro", "RETRO GRID", RK_BG, BGX_RETRO },
    { "bg_clouds", "CANDY CLOUDS", RK_BG, BGX_CLOUDS },
    { "bg_planets", "PLANETS", RK_BG, BGX_PLANETS },
    { "tr_rainbow", "RAINBOW RIBBON", RK_TRAIL, TRX_RAINBOW },
    { "tr_hearts", "HEARTS", RK_TRAIL, TRX_HEARTS },
    { "tr_notes", "MUSIC NOTES", RK_TRAIL, TRX_NOTES },
    { "tr_lightning", "LIGHTNING", RK_TRAIL, TRX_LIGHTNING },
    { "tr_dust", "PIXEL DUST", RK_TRAIL, TRX_DUST },
    { "tr_snow", "SNOWFLAKES", RK_TRAIL, TRX_SNOW },
    { "tr_eggs", "LITTLE EGGS", RK_TRAIL, TRX_EGGS },
    { "tr_confetti", "CONFETTI TRAIL", RK_TRAIL, TRX_CONFETTI },
    { "de_poof", "CARTOON POOF", RK_DEATH, DEX_POOF },
    { "de_ghost", "GHOST FLOAT", RK_DEATH, DEX_GHOST },
    { "de_pixels", "PIXEL SHATTER", RK_DEATH, DEX_PIXELS },
    { "de_jelly", "JELLY SPLAT", RK_DEATH, DEX_JELLY },
    { "de_egg", "EGG CRACK", RK_DEATH, DEX_EGG },
    { "de_rainbow", "RAINBOW SPLASH", RK_DEATH, DEX_RAINBOW },
    { "fi_chrome", "CHROME", RK_FINISH, FIX_CHROME },
    { "fi_glitch", "GLITCH", RK_FINISH, FIX_GLITCH },
    { "fi_rainbow", "RAINBOW CYCLE", RK_FINISH, FIX_RAINBOW },
    { "fi_galaxy", "GALAXY", RK_FINISH, FIX_GALAXY },
    { "fi_neon", "NEON OUTLINE", RK_FINISH, FIX_NEON },
    { "fi_candy", "CANDY STRIPES", RK_FINISH, FIX_CANDY },
    { "fi_golden", "GOLDEN CUBE", RK_FINISH, FIX_GOLDEN },
    { "sf_crt", "OLD TV LINES", RK_FILTER, FILTER_CRT },
    { "sf_handheld", "HANDHELD GREEN", RK_FILTER, FILTER_HANDHELD },
    { "sf_movie", "OLD MOVIE", RK_FILTER, FILTER_SEPIA },
    { "sf_neon", "NEON GLOW", RK_FILTER, FILTER_NEON },
    { "sf_thermal", "THERMAL VISION", RK_FILTER, FILTER_THERMAL },
    { "sf_pastel", "PASTEL", RK_FILTER, FILTER_PASTEL },
    { "pet_robot", "ROBO BUDDY", RK_PET, 1 },
    { "pet_bird", "BIRDY", RK_PET, 2 },
    { "pet_ghost", "BOO", RK_PET, 3 },
    { "pet_dragon", "DRAGGY", RK_PET, 4 },
    { "pet_ufo", "MINI UFO", RK_PET, 5 },
    { "pet_cat", "KITTY", RK_PET, 6 },
    { "pet_slime", "SLIMEY", RK_PET, 7 },
    { "pet_star", "STARRY", RK_PET, 8 },
    { "so_boing", "BOING SOUNDS", RK_SOUNDS, SFXPACK_BOING },
    { "so_quack", "QUACK SOUNDS", RK_SOUNDS, SFXPACK_QUACK },
    { "so_chip", "CHIPTUNE SOUNDS", RK_SOUNDS, SFXPACK_CHIP },
    { "so_robot", "ROBOT SOUNDS", RK_SOUNDS, SFXPACK_ROBOT },
    { "song_1", "SECRET SONG: EGG HUNT", RK_SONG, 1 },
    { "song_2", "SECRET SONG: MOON BOUNCE", RK_SONG, 2 },
    { "song_3", "SECRET SONG: GOLDEN", RK_SONG, 3 },
    { "ti_night", "NIGHT TITLE", RK_TITLE, TIX_NIGHT },
    { "ti_rainbow", "RAINBOW LETTERS", RK_TITLE, TIX_RAINBOW },
    { "ti_eggs", "EGG HUNT TITLE", RK_TITLE, TIX_EGGS },
    { "ti_party", "PARTY TITLE", RK_TITLE, TIX_PARTY },
    { "pa_disco", "DISCO LIGHTS", RK_PARTY, PA_DISCO },
    { "pa_bighead", "BIG HEAD", RK_PARTY, PA_BIGHEAD },
    { "pa_tiny", "TINY WORLD", RK_PARTY, PA_TINY },
    { "pa_confetti", "CONFETTI JUMPS", RK_PARTY, PA_CONFETTI },
    { "pa_slowmo", "SLOW-MO CRASHES", RK_PARTY, PA_SLOWMO },
    { "pa_fireworks", "FIREWORKS FINISH", RK_PARTY, PA_FIREWORKS },
    { "pa_hat", "PARTY HAT", RK_PARTY, PA_HAT },
    { "pa_googly", "GOOGLY EYES", RK_PARTY, PA_GOOGLY },
    { "pa_floor", "RAINBOW FLOOR", RK_PARTY, PA_FLOOR },
    { "ic_king", "SECRET ICON: KING", RK_ICON, 0 },
    { "ic_rainbow", "SECRET ICON: RAINBOW", RK_ICON, 1 },
    { "ic_diamond", "SECRET ICON: DIAMOND", RK_ICON, 2 },
    { "ic_mew", "SECRET ICON: MEW", RK_ICON, 3 },
    { "ic_dragon", "SECRET ICON: DRAGON", RK_ICON, 4 },
    { "ic_unicorn", "SECRET ICON: UNICORN", RK_ICON, 5 },
    { "ic_trex", "SECRET ICON: T-REX", RK_ICON, 6 },
    { "ic_invader", "SECRET ICON: INVADER", RK_ICON, 7 },
    { "ic_chomper", "SECRET ICON: CHOMPER", RK_ICON, 8 },
    { "ic_galaxy", "SECRET ICON: GALAXY", RK_ICON, 9 },
    { "ic_skull", "SECRET ICON: FIRE SKULL", RK_ICON, 10 },
    { "ic_lava", "SECRET ICON: LAVA BLOB", RK_ICON, 11 },
    { "ic_demon", "SECRET ICON: DEMON KING", RK_ICON, 12 },
    { "ic_fritz", "SECRET ICON: FRITZ", RK_ICON, 13 },
    { "ic_omega", "SECRET ICON: OMEGA BOT", RK_ICON, 14 },
    { "ic_glitch", "SECRET ICON: GLITCH", RK_ICON, 15 },
    { "vault", "THE VAULT WORLD", RK_VAULT, 1 },
};
const int REWARD_COUNT = (int)(sizeof(REWARDS) / sizeof(REWARDS[0]));

const GameEgg GAME_EGGS[] = {
    { "konami", "THE CODE", "UP UP, DOWN DOWN, LEFT RIGHT, LEFT RIGHT ON THE TITLE", "bg_retro" },
    { "logo", "LOGO TAPPER", "THE TITLE LIKES TAPS: TOUCHPAD 10 TIMES", "ti_rainbow" },
    { "rainbow", "RAINBOW MAKER", "NAME A LEVEL AFTER WHAT SHINES AFTER RAIN", "tr_rainbow" },
    { "disco", "DISCO FEVER", "NAME A LEVEL AFTER A DANCE PARTY WITH A SHINY BALL", "pa_disco" },
    { "snow", "SNOW DAY", "NAME A LEVEL AFTER WHAT FALLS IN WINTER", "bg_snow" },
    { "tag_egg", "EGG WORDS", "WHAT DO HENS LAY? WRITE IT UNDER THE LOGO", "tr_eggs" },
    { "tag_fritz", "IT IS ME", "WRITE YOUR OWN NAME UNDER THE LOGO", "ic_fritz" },
    { "smiley", "HAPPY FACE", "DRAW A SMILEY FACE IN THE ICON MAKER", "pa_googly" },
    { "drums", "DRUM ROLL", "MAKE A SONG WITH A DRUM ON EVERY STEP", "so_chip" },
    { "idle", "SLEEPY TITLE", "PUT THE CONTROLLER DOWN ON THE TITLE FOR 2 MINUTES", "bg_warp" },
    { "halloween", "SPOOKY NIGHT", "PLAY ON THE SPOOKIEST NIGHT OF THE YEAR", "de_ghost" },
    { "christmas", "PRESENT DAY", "PLAY ON THE DAY OF PRESENTS UNDER THE TREE", "tr_snow" },
    { "newyear", "HAPPY NEW YEAR", "PLAY ON THE VERY FIRST DAY OF THE YEAR", "bg_fireworks" },
    { "jumps", "BOING X 1000", "JUMP ONE THOUSAND TIMES", "so_boing" },
    { "never_give_up", "NEVER GIVE UP", "TRY ONE LEVEL 100 TIMES. KEEP GOING!", "pa_slowmo" },
    { "silent_win", "SILENT WIN", "BEAT A LEVEL WITH THE MUSIC TURNED ALL THE WAY DOWN", "song_1" },
    { "fashion", "FASHION SHOW", "CHANGE EVERY ROW IN CUSTOMIZE", "fi_candy" },
    { "so_close", "SO CLOSE", "CRASH AT 99 PERCENT. SO CLOSE!", "fi_glitch" },
    { "twins", "TWIN COLOURS", "MAKE COLOUR 1 AND COLOUR 2 THE SAME", "sf_handheld" },
    { "coin_run", "COIN SWEEP", "GET EVERY COIN OF A LEVEL IN ONE RUN", "pa_fireworks" },
    { "endless", "ENDLESS 25", "SCORE 25 IN ENDLESS", "so_robot" },
    { "player2", "PLAYER 2 WINS", "LET PLAYER 2 WIN A RACE", "pa_confetti" },
    { "world_path", "WORLD BUILDER", "BUILD A FRITZ WORLD PATH OF 5 LEVELS", "bg_clouds" },
    { "checkpoints", "CHECKPOINT KING", "PUT DOWN 20 CHECKPOINTS IN ONE PRACTICE RUN", "sf_movie" },
    { "show_me", "SHOW ME HOW", "ASK PRACTICE MODE TO SHOW YOU HOW", "tr_lightning" },
    { "shoulders", "SHOULDER PRESS", "HOLD L2 AND R2 TOGETHER ON THE TITLE", "sf_thermal" },
    { "melody", "MELODY MAKER", "MAKE A SONG WITH A LEAD NOTE ON EVERY STEP", "song_2" },
    { "full_icon", "FULL ICON", "FILL A WHOLE ICON WITH ONE COLOUR", "sf_pastel" },
    { "badge_tour", "BADGE TOUR", "LOOK AT EVERY BADGE ON THE BADGE SCREEN", "ti_party" },
    { "night_owl", "NIGHT OWL", "PLAY WHEN THE OWLS ARE AWAKE: AFTER 8 AT NIGHT", "ti_night" },
    { "early_bird", "EARLY BIRD", "PLAY BEFORE 7 IN THE MORNING", "bg_aurora" },
    { "editor_100", "BIG BUILDER", "PUT 100 THINGS IN ONE LEVEL IN THE EDITOR", "fi_neon" },
    { "stock_tour", "ICON TOUR", "VISIT EVERY SET IN STOCK ICONS", "so_quack" },
};
const int GAME_EGG_COUNT = (int)(sizeof(GAME_EGGS) / sizeof(GAME_EGGS[0]));

// The rest unlock as eggs are counted: one at each count here, and every
// tenth egg a bigger one (a pet, the VAULT, the GOLDEN cube).
static const struct {
    int count;
    const char *key;
} LADDER[] = {
    { 1, "tr_hearts" }, { 2, "pa_hat" }, { 3, "bg_bubbles" }, { 4, "ic_king" }, { 5, "de_poof" },
    { 6, "fi_rainbow" }, { 7, "ic_rainbow" }, { 8, "tr_notes" }, { 9, "ic_diamond" }, { 10, "pet_robot" },
    { 12, "de_egg" }, { 14, "ic_mew" }, { 16, "pa_bighead" }, { 18, "ic_dragon" }, { 20, "pet_bird" },
    { 22, "tr_dust" }, { 25, "ic_unicorn" }, { 28, "bg_code" }, { 30, "pet_ghost" }, { 32, "de_pixels" },
    { 35, "ic_trex" }, { 38, "sf_crt" }, { 40, "pet_dragon" }, { 42, "pa_tiny" }, { 45, "ic_invader" },
    { 48, "de_jelly" }, { 50, "pet_ufo" }, { 52, "tr_confetti" }, { 55, "ic_chomper" }, { 58, "fi_chrome" },
    { 60, "pet_cat" }, { 64, "ic_galaxy" }, { 68, "pa_floor" }, { 70, "pet_slime" }, { 75, "ic_skull" },
    { 80, "pet_star" }, { 90, "ti_eggs" }, { VAULT_EGGS, "vault" }, { 110, "ic_lava" }, { 120, "bg_planets" },
    { 130, "ic_demon" }, { 140, "de_rainbow" }, { 150, "song_3" }, { 160, "ic_omega" }, { 170, "sf_neon" },
    { 180, "fi_galaxy" }, { 190, "ic_glitch" }, { GOLDEN_EGGS, "fi_golden" },
};
#define NLADDER ((int)(sizeof(LADDER) / sizeof(LADDER[0])))

static int reward_index(const char *key)
{
    for (int r = 0; r < REWARD_COUNT; r++)
        if (!strcmp(REWARDS[r].key, key))
            return r;
    return -1;
}

// What unlocks each reward: a game egg (index) or a count.
static int bound_egg[sizeof(REWARDS) / sizeof(REWARDS[0])], need_count[sizeof(REWARDS) / sizeof(REWARDS[0])];
static int at_count[GOLDEN_EGGS + 1];

static void tables_init(void)
{
    static int done;
    if (done)
        return;
    done = 1;
    for (int r = 0; r < REWARD_COUNT; r++)
        bound_egg[r] = -1;
    for (int c = 0; c <= GOLDEN_EGGS; c++)
        at_count[c] = -1;
    for (int i = 0; i < GAME_EGG_COUNT; i++)
        if (reward_index(GAME_EGGS[i].reward) >= 0)
            bound_egg[reward_index(GAME_EGGS[i].reward)] = i;
    for (int i = 0; i < NLADDER; i++) {
        int r = reward_index(LADDER[i].key);
        if (r >= 0) {
            need_count[r] = LADDER[i].count;
            at_count[LADDER[i].count] = r;
        }
    }
}

// --- The registry of level eggs ---------------------------------------------------

static RegEgg reg[MAX_REG_EGGS];
static int nreg;
static int counted = -1;        // eggs_count, worked out again after any change

void eggs_register(int builtin, const Level *lv)
{
    for (int i = 0; i < lv->neggs && nreg < MAX_REG_EGGS; i++) {
        int dup = 0;
        for (int k = 0; k < nreg; k++)
            dup |= reg[k].builtin == builtin && reg[k].e.n == lv->eggs[i].n;
        if (dup)
            continue;
        reg[nreg].builtin = builtin;
        reg[nreg].e = lv->eggs[i];
        reg[nreg].paint = egg_paint(lv->name, lv->eggs[i].n);
        eggs_level_id(builtin, lv->eggs[i].n, reg[nreg].id);
        nreg++;
        counted = -1;
    }
}

int eggs_reg_count(void) { return nreg; }
const RegEgg *eggs_reg(int i) { return &reg[i]; }

void eggs_level_id(int builtin, int n, char *out)
{
    snprintf(out, EGG_ID, "%.2s:%d", BUILTIN_IDS[builtin], n);
}

// --- The save ---------------------------------------------------------------------

static char found[MAX_FOUND][EGG_ID];
static int nfound;
static int worn[RK_PICKED], party;
static int clock_month, clock_day, clock_hour;
static char save_buf[MAX_FOUND * (EGG_ID + 1) + 2048];

static int known(const char *id)
{
    if (!strncmp(id, "g:", 2)) {
        for (int i = 0; i < GAME_EGG_COUNT; i++)
            if (!strcmp(GAME_EGGS[i].key, id + 2))
                return 1;
        return 0;
    }
    for (int i = 0; i < nreg; i++)
        if (!strcmp(reg[i].id, id))
            return 1;
    return 0;
}

int eggs_found(const char *id)
{
    for (int i = 0; i < nfound; i++)
        if (!strcmp(found[i], id))
            return 1;
    return 0;
}

int eggs_found_rank(const char *id)
{
    for (int i = 0, n = 0; i < nfound; i++)
        if (known(found[i])) {
            n++;
            if (!strcmp(found[i], id))
                return n;
        }
    return 0;
}

int eggs_count(void)
{
    if (counted < 0) {
        counted = 0;
        for (int i = 0; i < nfound; i++)
            counted += known(found[i]);
    }
    return counted;
}

int eggs_total(void)
{
    return nreg + GAME_EGG_COUNT;
}

int eggs_level_mask(int builtin)
{
    int m = 0;
    char id[EGG_ID];
    for (int n = 1; n <= MAX_EGGS; n++) {
        eggs_level_id(builtin, n, id);
        if (eggs_found(id))
            m |= 1 << (n - 1);
    }
    return m;
}

int eggs_game_found(void)
{
    char id[EGG_ID + 8];
    int n = 0;
    for (int i = 0; i < GAME_EGG_COUNT; i++) {
        snprintf(id, sizeof(id), "g:%s", GAME_EGGS[i].key);
        n += eggs_found(id);
    }
    return n;
}

int reward_unlocked(int r)
{
    tables_init();
    if (r < 0 || r >= REWARD_COUNT)
        return 0;
    if (bound_egg[r] >= 0) {
        char id[EGG_ID + 8];
        snprintf(id, sizeof(id), "g:%s", GAME_EGGS[bound_egg[r]].key);
        return eggs_found(id);
    }
    return need_count[r] > 0 && eggs_count() >= need_count[r];
}

int reward_at_count(int count)
{
    tables_init();
    return count >= 1 && count <= GOLDEN_EGGS ? at_count[count] : -1;
}

int reward_needs(int r)
{
    tables_init();
    return r >= 0 && r < REWARD_COUNT ? need_count[r] : 0;
}

int reward_worn(int kind)
{
    return kind >= 0 && kind < RK_PICKED ? worn[kind] : 0;
}

int reward_party(void)
{
    return party;
}

int reward_is_on(int r)
{
    const Reward *w = &REWARDS[r];
    if (w->kind == RK_PARTY)
        return (party & w->value) != 0;
    return w->kind < RK_PICKED && worn[w->kind] == w->value;
}

int eggs_vault_open(void)
{
    return reward_unlocked(reward_index("vault"));
}

int eggs_menu_song(void)
{
    return worn[RK_SONG] ? SONG_SECRET + worn[RK_SONG] - 1 : SONG_MENU;
}

static int secret_icon(int n)
{
    for (int k = 0; k < STOCK_COUNT; k++)
        if (STOCK[k].secret && n-- == 0)
            return k;
    return -1;
}

// Sounds and secret icons take effect at once; the rest are read as drawn.
static void apply(void)
{
    synth_sfx_pack(worn[RK_SOUNDS]);
    stock_forget();
    for (int r = 0; r < REWARD_COUNT; r++)
        if (REWARDS[r].kind == RK_ICON && reward_unlocked(r))
            stock_find(secret_icon(REWARDS[r].value));
}

static void save(void)
{
    int n = 0;
    for (int i = 0; i < nfound; i++)
        n += snprintf(save_buf + n, sizeof(save_buf) - (size_t)n, "%s\n", found[i]);
    for (int r = 0; r < REWARD_COUNT; r++)
        if (REWARDS[r].kind <= RK_PARTY && reward_is_on(r))
            n += snprintf(save_buf + n, sizeof(save_buf) - (size_t)n, "+%s\n", REWARDS[r].key);
    plat_write_file(EGGS_FILE, save_buf, n);
}

void reward_wear(int r, int on)
{
    const Reward *w = &REWARDS[r];
    if (w->kind == RK_PARTY)
        party = on ? party | w->value : party & ~w->value;
    else if (w->kind < RK_PICKED)
        worn[w->kind] = on ? w->value : 0;
    apply();
    save();
}

void eggs_load(void)
{
    tables_init();
    nfound = party = 0;
    counted = -1;
    memset(worn, 0, sizeof(worn));
    int n = plat_read_file(EGGS_FILE, save_buf, (int)sizeof(save_buf) - 1);
    save_buf[n > 0 ? n : 0] = 0;
    for (char *line = save_buf; line && *line;) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = 0;
        size_t len = strlen(line);
        if (len && line[len - 1] == '\r')
            line[--len] = 0;
        if (line[0] == '+') {
            int r = reward_index(line + 1);
            if (r >= 0 && reward_unlocked(r)) {
                if (REWARDS[r].kind == RK_PARTY)
                    party |= REWARDS[r].value;
                else if (REWARDS[r].kind < RK_PICKED)
                    worn[REWARDS[r].kind] = REWARDS[r].value;
            }
        } else if (len > 0 && len < EGG_ID && nfound < MAX_FOUND && !eggs_found(line)) {
            snprintf(found[nfound++], EGG_ID, "%s", line);
        }
        line = nl ? nl + 1 : NULL;
    }
    apply();
}

void eggs_set_clock(int month, int day, int hour)
{
    clock_month = month;
    clock_day = day;
    clock_hour = hour;
}

// --- Finding one ---------------------------------------------------------------------

int eggs_find(Game *g, const char *id)
{
    if (!id[0] || eggs_found(id) || nfound >= MAX_FOUND || !known(id))
        return 0;
    int was[sizeof(REWARDS) / sizeof(REWARDS[0])];
    for (int r = 0; r < REWARD_COUNT; r++)
        was[r] = reward_unlocked(r);
    snprintf(found[nfound++], EGG_ID, "%s", id);
    counted = -1;
    int got[2] = { -1, -1 }, ngot = 0;
    for (int r = 0; r < REWARD_COUNT; r++)
        if (!was[r] && reward_unlocked(r)) {
            if (ngot < 2)
                got[ngot++] = r;
            if (REWARDS[r].kind == RK_PARTY)
                party |= REWARDS[r].value;
            else if (REWARDS[r].kind < RK_PICKED)
                worn[REWARDS[r].kind] = REWARDS[r].value;
        }
    apply();
    save();
    if (g->negg_toasts < EGG_TOASTS) {
        EggToast *t = &g->egg_toast[g->negg_toasts++];
        snprintf(t->id, sizeof(t->id), "%s", id);
        t->reward = got[0];
        t->reward2 = got[1];
        t->count = eggs_count();
    }
    badges_update(g);
    return 1;
}

static void find_game(Game *g, const char *key)
{
    char id[EGG_ID + 8];
    snprintf(id, sizeof(id), "g:%s", key);
    eggs_find(g, id);
}

static int has_word(const char *s, const char *w)
{
    return strstr(s, w) != NULL;
}

// A smiley in an 8x8 icon: two eyes side by side in the top half, and below
// them a mouth whose ends sit a row higher than its middle.
static int smiley(const uint8_t *px)
{
    for (int ey = 0; ey < 4; ey++)
        for (int a = 0; a < 6; a++)
            for (int b = a + 2; b < 8; b++) {
                int c = px[ey * 8 + a];
                if (c == PX_CUBE || px[ey * 8 + b] != c || px[ey * 8 + a + 1] == c)
                    continue;
                for (int my = ey + 2; my < 7; my++)
                    for (int ma = 0; ma <= a + 1; ma++)
                        for (int mb = b - 1 > ma + 3 ? b - 1 : ma + 3; mb < 8; mb++) {
                            int m = px[my * 8 + ma], ok = m != PX_CUBE && px[my * 8 + mb] == m;
                            for (int x = ma + 1; x < mb && ok; x++)
                                ok = px[(my + 1) * 8 + x] == m && px[my * 8 + x] != m;
                            if (ok)
                                return 1;
                        }
            }
    return 0;
}

static int all_one(const uint8_t *px)
{
    for (int i = 1; i < ICON_N * ICON_N; i++)
        if (px[i] != px[0])
            return 0;
    return px[0] != PX_CUBE;
}


void eggs_step(Game *g, int input, int edges, const Level *edit)
{
    static const int CODE[8] = { IN_UP, IN_UP, IN_DOWN, IN_DOWN, IN_LEFT, IN_RIGHT, IN_LEFT, IN_RIGHT };
    int dirs = edges & (IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT), held = input & ~(IN_P2_HERE | IN_P1_CROSS | IN_P2_CROSS);
    if (g->state == ST_TITLE) {
        if (dirs) {
            memmove(g->egg_keys, g->egg_keys + 1, sizeof(int) * 7);
            g->egg_keys[7] = dirs;
            if (!memcmp(g->egg_keys, CODE, sizeof(CODE)))
                find_game(g, "konami");
        }
        if ((edges & IN_TOUCH) && ++g->egg_taps >= 10)
            find_game(g, "logo");
        g->egg_idle = held ? 0 : g->egg_idle + 1;
        if (g->egg_idle == EGG_IDLE_FRAMES)
            find_game(g, "idle");
        g->egg_hold = (held & (IN_L2 | IN_R2)) == (IN_L2 | IN_R2) ? g->egg_hold + 1 : 0;
        if (g->egg_hold == 60)
            find_game(g, "shoulders");
    } else {
        g->egg_idle = 0;
    }
    if (g->state == ST_EDITOR && g->ed.screen != ED_SLOTS && edit) {
        if (!strcmp(edit->name, "RAINBOW"))
            find_game(g, "rainbow");
        if (!strcmp(edit->name, "DISCO"))
            find_game(g, "disco");
        if (!strcmp(edit->name, "SNOW"))
            find_game(g, "snow");
        if (edit->nobj >= 100)
            find_game(g, "editor_100");
    }
    if (g->state == ST_ICONS && g->icons.icon < NUM_ICONS && g->frame % 10 == 0) {
        if (smiley(icon_pixels(g->icons.icon)))
            find_game(g, "smiley");
        if (all_one(icon_pixels(g->icons.icon)))
            find_game(g, "full_icon");
    }
    if (g->state == ST_SONGS && g->frame % 10 == 0) {
        int drums = 1, lead = 1;
        for (int s = 0; s < USER_STEPS; s++) {
            drums &= g->sm.song.notes[TRACK_DRUMS][s] != 0;
            lead &= g->sm.song.notes[TRACK_LEAD][s] != 0;
        }
        if (drums)
            find_game(g, "drums");
        if (lead)
            find_game(g, "melody");
    }
    if (g->state == ST_BADGES) {
        g->egg_badge_seen |= 1ull << g->badge_sel;
        if (g->egg_badge_seen == (1ull << BADGE_COUNT) - 1)
            find_game(g, "badge_tour");
    }
    if (g->state == ST_STOCK) {
        g->egg_sets |= 1 << g->stock.set;
        if (g->egg_sets == (1 << STOCK_SET_COUNT) - 1)
            find_game(g, "stock_tour");
    }
    if (g->state == ST_CUSTOMIZE || g->state == ST_STOCK || g->state == ST_ICONS) {
        for (int i = 0; i < LOOK_PARTS; i++)
            if (g->look.v[i] != g->egg_look.v[i] || (i == LOOK_FACE && g->look.stock != g->egg_look.stock))
                g->egg_rows |= 1 << i;
        if (g->egg_rows == (1 << LOOK_PARTS) - 1)
            find_game(g, "fashion");
    }
    g->egg_look = g->look;
    if (g->look.v[LOOK_COL1] == g->look.v[LOOK_COL2])
        find_game(g, "twins");
    if (g->tagline[0] && has_word(g->tagline, "EGG"))
        find_game(g, "tag_egg");
    if (g->tagline[0] && has_word(g->tagline, "FRITZ"))
        find_game(g, "tag_fritz");
    if (g->stats[STAT_JUMPS] >= 1000)
        find_game(g, "jumps");
    if (g->state == ST_ENDLESS && g->endless.score >= 25)
        find_game(g, "endless");
    if (g->state == ST_RACE_DONE && g->winner == 2)
        find_game(g, "player2");
    if (g->state == ST_WORLD && g->world.n >= 5)
        find_game(g, "world_path");
    if (g->state == ST_PLAY && g->hint.state == HINT_SEARCH)
        find_game(g, "show_me");
    if (g->state == ST_PLAY && g->run.practice && g->run.ncp >= 20)
        find_game(g, "checkpoints");
    if (clock_month == 10 && clock_day == 31)
        find_game(g, "halloween");
    if (clock_month == 12 && clock_day == 25)
        find_game(g, "christmas");
    if (clock_month == 1 && clock_day == 1)
        find_game(g, "newyear");
    if (clock_month && clock_hour >= 20)
        find_game(g, "night_owl");
    if (clock_month && clock_hour >= 4 && clock_hour < 7)
        find_game(g, "early_bird");
}

// After each frame of a level: eggs taken in it (built-in levels, practice
// too), and the game eggs a run can find.
void eggs_after_run(Game *g, int ev)
{
    const LevelInfo *li = &g->levels[g->level_sel];
    if (g->run.egg_new && !g->playtest && li->builtin >= 0)
        for (int n = 1; n <= MAX_EGGS; n++)
            if (g->run.egg_new >> (n - 1) & 1) {
                char id[EGG_ID];
                eggs_level_id(li->builtin, n, id);
                eggs_find(g, id);
                g->run.eggs_had |= 1 << (n - 1);
            }
    if (g->playtest)
        return;
    if (ev == EV_DIED && g->run.pct >= 99)
        find_game(g, "so_close");
    if (ev == EV_DIED && li->attempts >= 100)
        find_game(g, "never_give_up");
    if (ev == EV_COMPLETE && g->music_vol == 0)
        find_game(g, "silent_win");
    if (ev == EV_COMPLETE && li->ncoins && g->run.b.coins == (1 << li->ncoins) - 1)
        find_game(g, "coin_run");
}

// --- The toast ---------------------------------------------------------------------

void eggs_step_toast(Game *g)
{
    if (!g->negg_toasts || (g->clean_hud && g->state == ST_PLAY))
        return;
    if (g->egg_toast_frames++ == 0)
        synth_sfx(SND_EGG);
    if (g->egg_toast_frames >= EGG_TOAST_FRAMES) {
        memmove(g->egg_toast, g->egg_toast + 1, sizeof(EggToast) * (size_t)--g->negg_toasts);
        g->egg_toast_frames = 0;
    }
}

// The name of the egg called id, and its paint.
static const char *egg_name(const char *id, uint32_t *paint)
{
    *paint = 0;
    if (!strncmp(id, "g:", 2)) {
        for (int i = 0; i < GAME_EGG_COUNT; i++)
            if (!strcmp(GAME_EGGS[i].key, id + 2)) {
                *paint = (uint32_t)i * 7919u + 13u;
                return GAME_EGGS[i].name;
            }
        return "?";
    }
    for (int i = 0; i < nreg; i++)
        if (!strcmp(reg[i].id, id)) {
            *paint = reg[i].paint;
            return reg[i].e.name;
        }
    return "?";
}

const char *eggs_name(const char *id, uint32_t *paint)
{
    return egg_name(id, paint);
}

void eggs_draw_toast(const Game *g)
{
    if (!g->negg_toasts || !g->egg_toast_frames)
        return;
    const EggToast *t = &g->egg_toast[0];
    int f = g->egg_toast_frames, milestone = t->count % 10 == 0;
    int slide = f < 16 ? (16 - f) * 14 : f > EGG_TOAST_FRAMES - 16 ? (f - (EGG_TOAST_FRAMES - 16)) * 14 : 0;
    int w = 1060, h = t->reward >= 0 ? 196 : 150, x = (SCREEN_W - w) / 2, y = 24 - slide;
    color_t edge = milestone ? RGB(255, 200, 40) : RGB(255, 120, 220);
    ui_panel(x, y, w, h, edge);
    uint32_t paint;
    const char *name = egg_name(t->id, &paint);
    // The egg wobbles, cracks along a zigzag and its top flies off with a burst.
    float ex = (float)(x + 92), ey = (float)(y + h / 2 + 6), crack = f < 30 ? 0 : (float)(f - 30) / 20.0f;
    crack = crack > 1 ? 1 : crack;
    float lean = f < 30 ? fsin((float)f * 0.9f) * 8.0f : 0;
    if (crack > 0) {
        for (int k = 0; k < 8; k++) {
            float a = (float)k * 0.785f + (float)f * 0.05f, d = 30 + crack * 40;
            ui_star(ex + fcos(a) * d, ey - 10 + fsin(a) * d, 9 - crack * 4, k % 2 ? RGB(255, 240, 120) : RGB(255, 255, 255));
        }
    }
    draw_egg_rows(ex, ey, 92, lean, paint, 0, RGB(20, 8, 40), 15, 32);
    draw_egg_rows(ex - crack * 22, ey - crack * 40, 92, lean - crack * 18, paint, 0, RGB(20, 8, 40), 0, 15);
    if (f >= 26 && f < 34)
        for (int k = 0; k < 6; k++) {
            float zx = ex - 42 + (float)k * 14, zy = ey - 14 + (k % 2 ? 8 : -2);
            draw_rect((int)zx, (int)zy, 14, 4, RGB(255, 255, 255));
        }
    char line[96];
    snprintf(line, sizeof(line), milestone ? "%d EGGS! MILESTONE!" : "EGG FOUND!", t->count);
    draw_text_glow(x + 190, y + 18, 5, line, RGB(255, 230, 120), edge);
    int s = ui_fit_scale(name, w - 230, 7);
    draw_text_glow(x + 190, y + 62, s, name, RGB(255, 255, 255), edge);
    if (t->reward >= 0) {
        snprintf(line, sizeof(line), "UNLOCKED: %s%s", REWARDS[t->reward].name, t->reward2 >= 0 ? " +" : "");
        draw_text_glow(x + 190, y + 128, ui_fit_scale(line, w - 230, 4), line, RGB(120, 255, 200), RGB(0, 120, 90));
    }
    snprintf(line, sizeof(line), "%d / %d", t->count, eggs_total());
    draw_text(x + w - 30 - text_width(line, 3), y + h - 34, 3, line, RGB(220, 200, 255));
}
