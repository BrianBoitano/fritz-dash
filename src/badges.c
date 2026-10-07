// Fritz Dash - badges.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "badges.h"
#include "game.h"
#include "platform.h"
#include "synth.h"

#define COL_WHITE RGB(255, 255, 255)
#define COL_DIM   RGB(130, 130, 170)
#define COL_GOLD  RGB(255, 200, 40)

const BadgeDef BADGES[BADGE_COUNT] = {
    [B_FIRST_WIN] = { "first_win", "FIRST WIN", "BEAT ANY LEVEL", "1ST", RGB(255, 200, 40) },
    [B_STUDENT] = { "student", "STUDENT", "FINISH HOW TO DASH", "ABC", RGB(90, 255, 40) },
    [B_ATTEMPTS100] = { "attempts100", "NEVER GIVE UP", "MAKE 100 ATTEMPTS", "100", RGB(0, 230, 255) },
    [B_ATTEMPTS1000] = { "attempts1000", "UNSTOPPABLE", "MAKE 1000 ATTEMPTS", "1000", RGB(60, 110, 255) },
    [B_JUMPS] = { "jumps1000", "BOING BOING", "JUMP 1000 TIMES", "UP", RGB(90, 255, 40) },
    [B_ORBS] = { "orbs100", "ORB MASTER", "HIT 100 ORBS", "O", RGB(255, 235, 40) },
    [B_NO_PRACTICE] = { "no_practice", "NO PRACTICE", "BEAT A LEVEL BEFORE PRACTISING IT", "!", RGB(255, 60, 60) },
    [B_FIRST_COIN] = { "first_coin", "SHINY", "GET A SECRET COIN", "C", RGB(255, 200, 40) },
    [B_LEVEL_COINS] = { "level_coins", "COIN HUNTER", "GET ALL 3 COINS IN A LEVEL", "3C", RGB(255, 170, 30) },
    [B_ALL_COINS] = { "all_coins", "COIN KING", "GET EVERY SECRET COIN", "ALL", RGB(255, 220, 90) },
    [B_STAR_STORM] = { "star_storm", "STORM CHASER", "BEAT STAR STORM", "S", RGB(255, 50, 200) },
    [B_ALL_LEVELS] = { "all_levels", "CHAMPION", "BEAT EVERY BUILT-IN LEVEL", "WIN", RGB(255, 200, 40) },
    [B_STARS10] = { "stars10", "STAR COLLECTOR", "EARN 10 STARS", "10", RGB(255, 235, 40) },
    [B_BUILDER] = { "builder", "BUILDER", "MAKE A LEVEL IN THE EDITOR", "+", RGB(170, 70, 255) },
    [B_MASTER_BUILDER] = { "master_builder", "MASTER BUILDER", "MAKE 5 LEVELS", "5", RGB(200, 100, 255) },
    [B_CREATOR] = { "creator", "CREATOR", "BEAT A LEVEL YOU MADE", "ME", RGB(255, 120, 255) },
    [B_HOUR] = { "hour", "DEDICATED", "PLAY FOR 1 HOUR", "1H", RGB(0, 230, 255) },
    [B_PRACTICE_WIN] = { "practice_win", "PRACTICE PRO", "FINISH A LEVEL IN PRACTICE", "P", RGB(90, 255, 40) },
    [B_ENDLESS] = { "endless50", "ENDLESS RUNNER", "SCORE 50 IN ENDLESS", "50", RGB(255, 140, 20) },
    [B_COMEBACK] = { "comeback", "COMEBACK", "BEAT A LEVEL AFTER 50 TRIES", "50X", RGB(255, 90, 90) },
    [B_RACER] = { "racer", "RACER", "FINISH A 2 PLAYER RACE", "2P", RGB(0, 230, 255) },
    [B_FASHION] = { "fashion", "FASHION STAR", "CHANGE YOUR LOOK", "?", RGB(255, 50, 200) },
    [B_WAVE] = { "wave", "WAVE RIDER", "BEAT ZIGZAG ZONE", "/", RGB(40, 210, 255) },
    [B_ISLES] = { "world_isles", "ISLAND HOPPER", "BEAT EVERY SUNRISE ISLES LEVEL", "W2", RGB(255, 140, 70) },
    [B_CORAL] = { "world_coral", "DEEP DIVER", "BEAT EVERY CORAL DEEP LEVEL", "W3", RGB(0, 170, 200) },
    [B_FORGE] = { "world_forge", "FORGE MASTER", "BEAT EVERY LAVA FORGE LEVEL", "W4", RGB(255, 90, 20) },
    [B_NEON] = { "world_neon", "CITY LIGHTS", "BEAT EVERY NEON CITY LEVEL", "W5", RGB(210, 50, 255) },
    [B_COSMIC] = { "world_cosmic", "SPACE ACE", "BEAT EVERY COSMIC CORE LEVEL", "W6", RGB(120, 100, 255) },
    [B_WORLD_COINS] = { "world_coins", "TREASURE HUNTER", "GET EVERY COIN IN ONE WORLD", "GOLD", RGB(255, 210, 60) },
    [B_STARS100] = { "stars100", "SUPER STAR", "EARN 100 STARS", "100", RGB(255, 235, 40) },
    [B_STARS200] = { "stars200", "MEGA STAR", "EARN 200 STARS", "200", RGB(255, 170, 40) },
    [B_CORE] = { "core_breaker", "CORE BREAKER", "BEAT THE LAST COSMIC CORE LEVEL", "END", RGB(255, 60, 140) },
    [B_EGG_FIRST] = { "egg_first", "EGG HUNTER", "FIND AN EASTER EGG", "EGG", RGB(255, 150, 220) },
    [B_EGG50] = { "egg50", "EGG COLLECTOR", "FIND 50 EASTER EGGS", "50", RGB(120, 230, 70) },
    [B_EGG100] = { "egg100", "VAULT KEY", "FIND 100 EASTER EGGS", "100", RGB(255, 200, 40) },
    [B_EGG200] = { "egg200", "GOLDEN EGG", "FIND 200 EASTER EGGS", "200", RGB(255, 170, 30) },
    [B_EGG_GAME] = { "egg_game", "RIDDLE MASTER", "FIND EVERY EGG AROUND THE GAME", "?!", RGB(170, 110, 255) },
    [B_SHAPESHIFT] = { "world_shapeshift", "SHAPE MASTER", "BEAT EVERY SHAPESHIFT LEVEL", "W7", RGB(255, 120, 230) },
};

int badge_got(const Game *g, int badge)
{
    if (badge < 0 || badge >= BADGE_COUNT)
        return 0;
    return badge < 32 ? (g->badges >> badge & 1) : (g->badges2 >> (badge - 32) & 1);
}

static void set_got(Game *g, int badge)
{
    if (badge < 32)
        g->badges |= 1u << badge;
    else
        g->badges2 |= 1u << (badge - 32);
}

static char buf[2048];

void badges_load(Game *g)
{
    int n = plat_read_file(BADGES_FILE, buf, (int)sizeof(buf) - 1);
    buf[n > 0 ? n : 0] = 0;
    g->badges = g->badges2 = 0;
    for (int i = 0; i < BADGE_COUNT; i++) {
        size_t k = strlen(BADGES[i].key);
        for (const char *p = buf; p; p = strchr(p, '\n')) {
            if (*p == '\n')
                p++;
            if (!strncmp(p, BADGES[i].key, k) && p[k] == ' ' && atoi(p + k + 1))
                set_got(g, i);
        }
    }
}

static void save(const Game *g)
{
    int n = 0;
    for (int i = 0; i < BADGE_COUNT; i++)
        if (badge_got(g, i))
            n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s 1\n", BADGES[i].key);
    plat_write_file(BADGES_FILE, buf, n);
}

void badge_award(Game *g, int badge)
{
    if (badge < 0 || badge >= BADGE_COUNT || badge_got(g, badge))
        return;
    set_got(g, badge);
    save(g);
    if (g->nbadge_toasts < 8)
        g->badge_toasts[g->nbadge_toasts++] = badge;
}

int badges_count(const Game *g)
{
    int n = 0;
    for (int i = 0; i < BADGE_COUNT; i++)
        n += badge_got(g, i);
    return n;
}

// The first built-in level whose id starts with id is beaten.
static int beaten(const Game *g, const char *id)
{
    for (int i = 0; i < g->nlevels; i++)
        if (g->levels[i].builtin >= 0 && !strncmp(BUILTIN_IDS[g->levels[i].builtin], id, strlen(id)))
            return g->levels[i].best == 100;
    return 0;
}

void badges_update(Game *g)
{
    int builtins = 0, won = 0, made = 0, own_win = 0, full_coins = 0, max_coins = 0;
    for (int i = 0; i < g->nlevels; i++) {
        const LevelInfo *li = &g->levels[i];
        if (li->builtin >= 0) {
            builtins++;
            won += li->best == 100;
            max_coins += li->ncoins;
            full_coins |= li->ncoins && li->coins == (1 << li->ncoins) - 1;
        } else if (!strncmp(li->file, "slot", 4)) {
            made++;
            own_win |= li->best == 100;
        }
    }
    int slots = 0;
    for (int i = 0; i < EDIT_SLOTS; i++)
        slots += g->ed.slot_used[i];
    made = made > slots ? made : slots;
    const int *st = g->stats;
    int coins = game_coins(g);
    if (st[STAT_WINS] >= 1) badge_award(g, B_FIRST_WIN);
    if (beaten(g, "00_how_to_dash")) badge_award(g, B_STUDENT);
    if (st[STAT_ATTEMPTS] >= 100) badge_award(g, B_ATTEMPTS100);
    if (st[STAT_ATTEMPTS] >= 1000) badge_award(g, B_ATTEMPTS1000);
    if (st[STAT_JUMPS] >= 1000) badge_award(g, B_JUMPS);
    if (st[STAT_ORBS] >= 100) badge_award(g, B_ORBS);
    if (coins >= 1) badge_award(g, B_FIRST_COIN);
    if (full_coins) badge_award(g, B_LEVEL_COINS);
    if (max_coins && coins == max_coins) badge_award(g, B_ALL_COINS);
    if (beaten(g, "05_star_storm")) badge_award(g, B_STAR_STORM);
    if (builtins && won == builtins) badge_award(g, B_ALL_LEVELS);
    if (game_stars(g) >= 10) badge_award(g, B_STARS10);
    if (made >= 1) badge_award(g, B_BUILDER);
    if (made >= 5) badge_award(g, B_MASTER_BUILDER);
    if (own_win) badge_award(g, B_CREATOR);
    if (st[STAT_SECONDS] >= 3600) badge_award(g, B_HOUR);
    if (st[STAT_PRACTICE_WINS] >= 1) badge_award(g, B_PRACTICE_WIN);
    if (g->endless_best >= 50) badge_award(g, B_ENDLESS);
    if (beaten(g, "08_zigzag_zone")) badge_award(g, B_WAVE);
    for (int w = 0; w < LWORLD_MINE; w++) {
        LWorldStats ws;
        game_lworld_stats(g, w, &ws);
        if (w > 0 && w <= B_COSMIC - B_ISLES + 1 && ws.count && ws.beaten == ws.count) badge_award(g, B_ISLES + w - 1);
        if (ws.max_coins && ws.coins == ws.max_coins) badge_award(g, B_WORLD_COINS);
        if (w == LWORLD_SHAPESHIFT && ws.count && ws.beaten == ws.count) badge_award(g, B_SHAPESHIFT);
    }
    if (game_stars(g) >= 100) badge_award(g, B_STARS100);
    if (game_stars(g) >= 200) badge_award(g, B_STARS200);
    if (beaten(g, "59")) badge_award(g, B_CORE);
    int eggs = eggs_count();
    if (eggs >= 1) badge_award(g, B_EGG_FIRST);
    if (eggs >= 50) badge_award(g, B_EGG50);
    if (eggs >= VAULT_EGGS) badge_award(g, B_EGG100);
    if (eggs >= GOLDEN_EGGS) badge_award(g, B_EGG200);
    if (eggs_game_found() == GAME_EGG_COUNT) badge_award(g, B_EGG_GAME);
}

// ---------------------------------------------------------------------------
// The badge screen: a 10 x 4 grid, details of the chosen one underneath.

#define COLS 10
#define CW   180

void badges_step_screen(Game *g, int edges)
{
    int s = g->badge_sel, was = s;
    if (edges & IN_LEFT) s = (s + BADGE_COUNT - 1) % BADGE_COUNT;
    if (edges & IN_RIGHT) s = (s + 1) % BADGE_COUNT;
    if ((edges & IN_UP) && s >= COLS) s -= COLS;
    if ((edges & IN_DOWN) && s + COLS < BADGE_COUNT) s += COLS;
    if (s != was)
        synth_sfx(SND_MOVE);
    g->badge_sel = s;
}

void draw_badge_medal(int cx, int cy, int r, int badge, int got)
{
    const BadgeDef *b = &BADGES[badge];
    color_t c = got ? b->color : RGB(60, 54, 84);
    float fx = (float)cx, fy = (float)cy;
    float lx[4] = { fx - r * 0.55f, fx - r * 0.15f, fx - r * 0.3f, fx - r * 0.7f };
    float rx[4] = { fx + r * 0.15f, fx + r * 0.55f, fx + r * 0.7f, fx + r * 0.3f };
    float ry[4] = { fy, fy, fy + r * 1.35f, fy + r * 1.2f };
    draw_poly(4, lx, ry, got ? RGB(220, 40, 70) : RGB(50, 40, 60));
    draw_poly(4, rx, ry, got ? RGB(40, 110, 255) : RGB(50, 40, 60));
    draw_circle(cx, cy, r + 6, col_scale(c, 1, 2));
    draw_circle(cx, cy, r, c);
    draw_circle(cx, cy, r * 4 / 5, got ? col_scale(c, 2, 3) : RGB(30, 26, 46));
    const char *label = got ? b->label : "?";
    int s = ui_fit_scale(label, r * 3 / 2, r / 9 > 3 ? r / 9 : 3);
    draw_text(cx - text_width(label, s) / 2, cy - 7 * s / 2, s, label, got ? COL_WHITE : COL_DIM);
}

void badges_draw_screen(const Game *g)
{
    run_draw_backdrop(LEVEL_PALETTES[11], (float)g->frame * 2.0f, 0);
    char line[80];
    draw_text_glow_centered(36, 9, "BADGES", COL_WHITE, COL_GOLD);
    snprintf(line, sizeof(line), "%d / %d", badges_count(g), BADGE_COUNT);
    draw_text_glow(SCREEN_W - 80 - text_width(line, 5), 50, 5, line, COL_GOLD, RGB(200, 120, 0));
    for (int i = 0; i < BADGE_COUNT; i++) {
        int x = 66 + (i % COLS) * CW, y = 140 + (i / COLS) * 178;
        int got = badge_got(g, i), on = i == g->badge_sel;
        ui_panel(x, y, CW - 10, 160, on ? RGB(90, 255, 40) : got ? col_scale(BADGES[i].color, 2, 3) : RGB(60, 50, 90));
        draw_badge_medal(x + (CW - 10) / 2, y + 50, 36, i, got);
        // A long name goes on two lines rather than shrinking.
        const char *name = got ? BADGES[i].name : "???", *sp = strchr(name, ' ');
        color_t c = got ? COL_WHITE : COL_DIM;
        if (text_width(name, 3) <= CW - 20 || !sp) {
            int s = ui_fit_scale(name, CW - 20, 3);
            draw_text(x + (CW - 10 - text_width(name, s)) / 2, y + 112, s, name, c);
        } else {
            char a[24];
            snprintf(a, sizeof(a), "%.*s", (int)(sp - name), name);
            draw_text(x + (CW - 10 - text_width(a, 3)) / 2, y + 104, 3, a, c);
            draw_text(x + (CW - 10 - text_width(sp + 1, 3)) / 2, y + 130, 3, sp + 1, c);
        }
    }
    const BadgeDef *b = &BADGES[g->badge_sel];
    int got = badge_got(g, g->badge_sel);
    ui_panel(66, 860, 1806, 130, got ? b->color : RGB(80, 70, 120));
    draw_text_glow(100, 880, 6, got ? b->name : "LOCKED BADGE", COL_WHITE, got ? b->color : RGB(80, 70, 120));
    snprintf(line, sizeof(line), "%s%s", got ? "EARNED: " : "HOW: ", b->how);
    draw_text(100, 940, 4, line, got ? COL_GOLD : RGB(220, 220, 255));
    for (int p = 0; p < LOOK_PARTS; p++)
        for (int k = 0; k < LOOK_COUNT[p]; k++)
            if (LOOK_OPTIONS[p][k].badge == g->badge_sel + 1) {
                snprintf(line, sizeof(line), "UNLOCKS %s", LOOK_OPTIONS[p][k].name);
                draw_text_glow(1840 - text_width(line, 4), 890, 4, line, COL_GOLD, RGB(200, 120, 0));
            }
    draw_text_glow_centered(SCREEN_H - 62, 4, "D-PAD CHOOSE   CIRCLE BACK", RGB(220, 220, 255), RGB(90, 90, 200));
}

void badges_draw_toast(const Game *g)
{
    if (!g->nbadge_toasts || g->negg_toasts)
        return;
    int f = g->badge_toast_frames, badge = g->badge_toasts[0];
    int slide = f < 16 ? (16 - f) * 12 : f > BADGE_TOAST_FRAMES - 16 ? (f - (BADGE_TOAST_FRAMES - 16)) * 12 : 0;
    int w = 860, h = 130, x = (SCREEN_W - w) / 2, y = 24 - slide;
    ui_panel(x, y, w, h, BADGES[badge].color);
    draw_badge_medal(x + 80, y + 58, 44, badge, 1);
    draw_text_glow(x + 160, y + 18, 5, "NEW BADGE!", COL_GOLD, RGB(200, 120, 0));
    int s = ui_fit_scale(BADGES[badge].name, w - 190, 7);
    draw_text_glow(x + 160, y + 66, s, BADGES[badge].name, COL_WHITE, BADGES[badge].color);
}
