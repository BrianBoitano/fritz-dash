// Fritz Dash - the SECRETS book (from the title): a page of eggs found around
// the game, then a page per world with a row of egg slots for each level.
// Found eggs are painted and say what they unlocked; the rest are "?" with
// their hint once the level is beaten (game eggs always show their riddle).
// And the SECRETS page (Settings), which turns each reward on and off.
#include <stdio.h>
#include <string.h>

#include "draw.h"
#include "eggs.h"
#include "game.h"
#include "input.h"
#include "mode.h"
#include "synth.h"

#define COL_WHITE RGB(255, 255, 255)
#define COL_DIM   RGB(130, 130, 170)
#define COL_GOLD  RGB(255, 200, 40)
#define COL_PINK  RGB(255, 120, 220)
#define COL_PAPER RGB(30, 16, 54)
#define BOOK_BG   ((LevelColors){ RGB(40, 14, 60), RGB(110, 40, 120), RGB(30, 10, 40), RGB(255, 150, 220) })

#define GAME_COLS 11
#define REWARD_KIND_MAX 24
#define MAX_ROWS  12

// A page: the game eggs (world -1) or one world's levels with eggs.
typedef struct {
    int world;
    int nrows;
    int builtin[MAX_ROWS];
    int first[MAX_ROWS], count[MAX_ROWS];   // registry entries of each row's eggs
} Page;

static int world_of(int builtin)
{
    char c = BUILTIN_IDS[builtin][0];
    return c >= '0' && c < '0' + LWORLD_MINE ? c - '0' : 0;
}

static int make_pages(Page *pages, int max)
{
    int n = 0;
    pages[n++].world = -1;
    for (int w = 0; w < LWORLD_MINE && n < max; w++) {
        if (w == LWORLD_VAULT && !eggs_vault_open())
            continue;
        Page *p = &pages[n];
        p->world = w;
        p->nrows = 0;
        for (int i = 0; i < eggs_reg_count(); i++) {
            const RegEgg *e = eggs_reg(i);
            if (world_of(e->builtin) != w)
                continue;
            int r = 0;
            while (r < p->nrows && p->builtin[r] != e->builtin)
                r++;
            if (r == p->nrows) {
                if (r == MAX_ROWS)
                    continue;
                p->builtin[r] = e->builtin;
                p->first[r] = i;
                p->count[r] = 0;
                p->nrows++;
            }
            p->count[r]++;
        }
        n += p->nrows > 0;
    }
    return n;
}

static int page_slots(const Page *p)
{
    if (p->world < 0)
        return GAME_EGG_COUNT;
    int n = 0;
    for (int r = 0; r < p->nrows; r++)
        n += p->count[r];
    return n;
}

// Row and column of slot k on a world page.
static void slot_rc(const Page *p, int k, int *row, int *col)
{
    int r = 0;
    while (r < p->nrows - 1 && k >= p->count[r])
        k -= p->count[r++];
    *row = r;
    *col = k;
}

static int slot_at(const Page *p, int row, int col)
{
    int k = 0;
    for (int r = 0; r < row; r++)
        k += p->count[r];
    return k + (col < p->count[row] ? col : p->count[row] - 1);
}

void secrets_open_book(Game *g)
{
    g->book_page = 0;
    g->book_sel = 0;
}

int secrets_step_book(Game *g, int edges)
{
    static Page pages[NUM_LWORLDS + 1];
    int np = make_pages(pages, NUM_LWORLDS + 1);
    if (edges & (IN_CIRCLE | IN_TRIANGLE))
        return 1;
    int was = g->book_page * 1000 + g->book_sel;
    if (edges & (IN_L1 | IN_R1)) {
        g->book_page = (g->book_page + ((edges & IN_R1) ? 1 : np - 1)) % np;
        g->book_sel = 0;
    }
    if (g->book_page >= np)
        g->book_page = 0;
    const Page *p = &pages[g->book_page];
    int n = page_slots(p), s = g->book_sel;
    if (p->world < 0) {
        if (edges & IN_LEFT) s = (s + n - 1) % n;
        if (edges & IN_RIGHT) s = (s + 1) % n;
        if ((edges & IN_UP) && s >= GAME_COLS) s -= GAME_COLS;
        if ((edges & IN_DOWN) && s + GAME_COLS < n) s += GAME_COLS;
    } else if (n) {
        int row, col;
        slot_rc(p, s, &row, &col);
        if (edges & IN_LEFT) s = (s + n - 1) % n;
        if (edges & IN_RIGHT) s = (s + 1) % n;
        if (edges & IN_UP) s = slot_at(p, (row + p->nrows - 1) % p->nrows, col);
        if (edges & IN_DOWN) s = slot_at(p, (row + 1) % p->nrows, col);
    }
    g->book_sel = n ? (s % n + n) % n : 0;
    if (g->book_page * 1000 + g->book_sel != was)
        synth_sfx(SND_MOVE);
    return 0;
}

static const LevelInfo *level_of(const Game *g, int builtin)
{
    for (int i = 0; i < g->nlevels; i++)
        if (g->levels[i].builtin == builtin)
            return &g->levels[i];
    return NULL;
}

static void slot_box(int x, int y, int w, int h, int on, int got)
{
    draw_rect(x, y, w, h, on ? COL_GOLD : got ? RGB(120, 60, 140) : RGB(60, 40, 80));
    draw_rect(x + 4, y + 4, w - 8, h - 8, on ? RGB(70, 40, 20) : COL_PAPER);
}

static void unknown_egg(float x, float y, float w)
{
    draw_egg(x, y, w, 0, 0, 200, RGB(40, 30, 60));
    int s = w > 60 ? 4 : 3;
    draw_text((int)x - text_width("?", s) / 2, (int)y - 7 * s / 2, s, "?", RGB(200, 190, 230));
}

// What finding id unlocked: its own reward (a game egg's) and the count's.
static void unlocked(const char *id, char *buf, int n)
{
    int rank = eggs_found_rank(id), own = -1;
    if (!strncmp(id, "g:", 2))
        for (int i = 0; i < GAME_EGG_COUNT; i++)
            if (!strcmp(GAME_EGGS[i].key, id + 2))
                for (int r = 0; r < REWARD_COUNT; r++)
                    if (!strcmp(REWARDS[r].key, GAME_EGGS[i].reward))
                        own = r;
    int at = reward_at_count(rank);
    if (own >= 0 && at >= 0)
        snprintf(buf, (size_t)n, "UNLOCKED: %s AND %s", REWARDS[own].name, REWARDS[at].name);
    else if (own >= 0 || at >= 0)
        snprintf(buf, (size_t)n, "UNLOCKED: %s", REWARDS[own >= 0 ? own : at].name);
    else
        snprintf(buf, (size_t)n, "A PAINTED EGG FOR YOUR BOOK (EGG NUMBER %d)", rank);
}

static void draw_detail(const char *title, const char *line1, const char *line2, color_t edge)
{
    ui_panel(60, 846, SCREEN_W - 120, 160, edge);
    draw_text_glow(96, 866, ui_fit_scale(title, SCREEN_W - 200, 6), title, COL_WHITE, edge);
    draw_text(96, 922, ui_fit_scale(line1, SCREEN_W - 200, 4), line1, RGB(255, 230, 140));
    if (line2[0])
        draw_text(96, 962, ui_fit_scale(line2, SCREEN_W - 200, 3), line2, RGB(200, 200, 240));
}

static void draw_game_page(const Game *g)
{
    char id[EGG_ID + 8], line[160];
    int found = 0;
    for (int i = 0; i < GAME_EGG_COUNT; i++) {
        snprintf(id, sizeof(id), "g:%s", GAME_EGGS[i].key);
        int got = eggs_found(id), on = i == g->book_sel;
        found += got;
        int x = 60 + (i % GAME_COLS) * 164, y = 190 + (i / GAME_COLS) * 214;
        slot_box(x, y, 154, 200, on, got);
        uint32_t paint;
        eggs_name(id, &paint);
        float wob = on ? fsin((float)g->frame * 0.15f) * 6 : 0;
        if (got) {
            draw_egg((float)x + 77, (float)y + 82, 76, wob, paint, 0, COL_PAPER);
            const char *nm = GAME_EGGS[i].name, *sp = strchr(nm, ' ');
            if (text_width(nm, 2) <= 140 || !sp) {
                int sc = ui_fit_scale(nm, 140, 2);
                draw_text(x + 77 - text_width(nm, sc) / 2, y + 162, sc, nm, COL_WHITE);
            } else {
                char a[24];
                snprintf(a, sizeof(a), "%.*s", (int)(sp - nm), nm);
                draw_text(x + 77 - text_width(a, 2) / 2, y + 154, 2, a, COL_WHITE);
                draw_text(x + 77 - text_width(sp + 1, 2) / 2, y + 174, 2, sp + 1, COL_WHITE);
            }
        } else {
            unknown_egg((float)x + 77, (float)y + 82, 76);
        }
    }
    snprintf(line, sizeof(line), "AROUND THE GAME  %d / %d", found, GAME_EGG_COUNT);
    draw_text_glow(60, 128, 5, line, COL_GOLD, RGB(140, 60, 0));
    const GameEgg *e = &GAME_EGGS[g->book_sel % GAME_EGG_COUNT];
    snprintf(id, sizeof(id), "g:%s", e->key);
    if (eggs_found(id)) {
        unlocked(id, line, sizeof(line));
        draw_detail(e->name, line, e->riddle, COL_GOLD);
    } else {
        draw_detail("? A SECRET ?", e->riddle, "", COL_PINK);
    }
}

static void draw_world_page(const Game *g, const Page *p)
{
    char id[EGG_ID], line[160];
    int found = 0, total = 0, row, col;
    slot_rc(p, g->book_sel, &row, &col);
    int rh = p->nrows > 10 ? 54 : p->nrows > 6 ? 64 : 92;
    for (int r = 0; r < p->nrows; r++) {
        int y = 186 + r * rh;
        const LevelInfo *li = level_of(g, p->builtin[r]);
        const char *name = li ? li->name : BUILTIN_IDS[p->builtin[r]];
        draw_rect(60, y, 700, rh - 8, r == row ? RGB(80, 40, 100) : RGB(36, 18, 56));
        draw_text(78, y + (rh - 8) / 2 - 10, ui_fit_scale(name, 600, 3), name, li && li->best == 100 ? COL_WHITE : COL_DIM);
        if (li && li->best == 100)
            ui_star(735, (float)(y + (rh - 8) / 2), 16, COL_GOLD);
        for (int k = 0; k < p->count[r]; k++) {
            const RegEgg *e = eggs_reg(p->first[r] + k);
            int x = 790 + k * (rh + 40), on = r == row && k == col;
            eggs_level_id(e->builtin, e->e.n, id);
            int got = eggs_found(id);
            found += got;
            total++;
            slot_box(x, y, rh + 30, rh - 8, on, got);
            float wob = on ? fsin((float)g->frame * 0.15f) * 4 : 0, ex = (float)x + (float)(rh + 30) / 2;
            if (got)
                draw_egg(ex, (float)y + (float)(rh - 8) / 2, (float)(rh - 8) * 0.62f, wob, e->paint, 0, COL_PAPER);
            else
                unknown_egg(ex, (float)y + (float)(rh - 8) / 2, (float)(rh - 8) * 0.62f);
        }
    }
    if (p->world == LWORLD_VAULT)
        snprintf(line, sizeof(line), "SECRET WORLD: %s  %d / %d", LWORLD_NAMES[p->world], found, total);
    else
        snprintf(line, sizeof(line), "WORLD %d: %s  %d / %d", p->world + 1, LWORLD_NAMES[p->world], found, total);
    draw_text_glow(60, 128, 5, line, COL_GOLD, RGB(140, 60, 0));
    if (!p->nrows)
        return;
    const RegEgg *e = eggs_reg(p->first[row] + col);
    const LevelInfo *li = level_of(g, e->builtin);
    eggs_level_id(e->builtin, e->e.n, id);
    if (eggs_found(id)) {
        unlocked(id, line, sizeof(line));
        draw_detail(e->e.name, line, li ? li->name : "", COL_GOLD);
    } else if (li && li->best == 100) {
        snprintf(line, sizeof(line), "IN %s", li->name);
        draw_detail(e->e.hint, line, "HINT", COL_PINK);
    } else {
        draw_detail("? A SECRET ?", "BEAT THIS LEVEL TO SEE A HINT", "", COL_PINK);
    }
}

void secrets_draw_book(const Game *g)
{
    static Page pages[NUM_LWORLDS + 1];
    int np = make_pages(pages, NUM_LWORLDS + 1);
    run_draw_backdrop(BOOK_BG, (float)g->frame * 1.5f, 0);
    draw_text_glow_centered(24, 9, "SECRETS", COL_WHITE, COL_PINK);
    char buf[80];
    snprintf(buf, sizeof(buf), "EGGS %d / %d", eggs_count(), eggs_total());
    draw_text_glow(SCREEN_W - 70 - text_width(buf, 5), 44, 5, buf, COL_GOLD, RGB(140, 60, 0));
    // The next reward a count brings.
    for (int c = eggs_count() + 1; c <= GOLDEN_EGGS; c++)
        if (reward_at_count(c) >= 0) {
            snprintf(buf, sizeof(buf), "NEXT AT %d: %s", c, REWARDS[reward_at_count(c)].name);
            draw_text(SCREEN_W - 70 - text_width(buf, 3), 96, 3, buf, RGB(220, 200, 255));
            break;
        }
    draw_egg(110, 70, 70, fsin((float)g->frame * 0.08f) * 6, (uint32_t)g->frame / 90 * 31 + 5, 0, BOOK_BG.top);
    const Page *p = &pages[g->book_page < np ? g->book_page : 0];
    if (p->world < 0)
        draw_game_page(g);
    else
        draw_world_page(g, p);
    snprintf(buf, sizeof(buf), "PAGE %d / %d", (g->book_page < np ? g->book_page : 0) + 1, np);
    draw_text(SCREEN_W - 70 - text_width(buf, 3), 140, 3, buf, COL_DIM);
    draw_text_glow_centered(SCREEN_H - 58, 4, "L1 R1 TURN THE PAGE   D-PAD CHOOSE   CIRCLE BACK", RGB(220, 220, 255),
                            RGB(90, 90, 200));
}

// --- The SECRETS page ------------------------------------------------------------------

#define NKINDS RK_PICKED
#define NPARTY 9
#define NROWS (NKINDS + NPARTY)

static int party_reward(int k)
{
    for (int r = 0, n = 0; r < REWARD_COUNT; r++)
        if (REWARDS[r].kind == RK_PARTY && n++ == k)
            return r;
    return -1;
}

// Unlocked rewards of a kind in order; returns how many.
static int options(int kind, int *out)
{
    int n = 0;
    for (int r = 0; r < REWARD_COUNT; r++)
        if (REWARDS[r].kind == kind && reward_unlocked(r))
            out[n++] = r;
    return n;
}

static int worn_reward(int kind)
{
    for (int r = 0; r < REWARD_COUNT; r++)
        if (REWARDS[r].kind == kind && reward_is_on(r))
            return r;
    return -1;
}

void secrets_open_page(Game *g)
{
    g->secret_row = 0;
}

int secrets_step_page(Game *g, int edges)
{
    if (edges & IN_CIRCLE)
        return 1;
    int was = g->secret_row;
    if (edges & IN_UP)
        g->secret_row = (g->secret_row + NROWS - 1) % NROWS;
    if (edges & IN_DOWN)
        g->secret_row = (g->secret_row + 1) % NROWS;
    if (g->secret_row != was)
        synth_sfx(SND_MOVE);
    int step = (edges & (IN_RIGHT | IN_CROSS)) ? 1 : (edges & IN_LEFT) ? -1 : 0;
    if (!step)
        return 0;
    int row = g->secret_row;
    if (row < NKINDS) {
        int opt[REWARD_KIND_MAX], n = options(row, opt), at = -1, cur = worn_reward(row);
        for (int i = 0; i < n; i++)
            if (opt[i] == cur)
                at = i;
        if (!n) {
            synth_sfx(SND_BACK);
            return 0;
        }
        // OFF, then each unlocked one.
        at = ((at + 1 + step) % (n + 1) + n + 1) % (n + 1) - 1;
        if (at < 0)
            reward_wear(cur, 0);
        else
            reward_wear(opt[at], 1);
        synth_sfx(row == RK_SOUNDS ? SND_JUMP : SND_SELECT);
        if (row == RK_DEATH)
            fx_burst(g->preview, &g->look, 1560, (float)(GROUND_Y - 470), &g->preview_rng);
    } else {
        int r = party_reward(row - NKINDS);
        if (r < 0 || !reward_unlocked(r)) {
            synth_sfx(SND_BACK);
            return 0;
        }
        reward_wear(r, !reward_is_on(r));
        synth_sfx(SND_SELECT);
    }
    return 0;
}

void secrets_draw_page(const Game *g)
{
    run_draw_backdrop(BOOK_BG, (float)g->frame * 2.0f, 0);
    // The preview on the right: his cube hopping over the worn background.
    int px0 = 1220;
    draw_set_clip(px0, SCREEN_W);
    draw_set_filter(reward_worn(RK_FILTER));
    LevelColors c = LEVEL_PALETTES[(g->frame / 600) % NUM_PALETTES];
    run_draw_backdrop(c, (float)g->frame * 4.0f, 0);
    rw_backdrop(c, (float)g->frame * 4.0f, 0);
    look_set(&g->look);
    look_draw_preview(1560, 540, 360, g->frame);
    fx_draw(g->preview, 0, 0, c.bottom);
    if (reward_worn(RK_FILTER) == FILTER_CRT)
        for (int y = 0; y < SCREEN_H; y += 4)
            draw_rect(px0, y, SCREEN_W - px0, 1, RGB(0, 0, 0));
    draw_set_filter(0);
    draw_set_clip(0, SCREEN_W);
    draw_rect(px0 - 8, 0, 8, SCREEN_H, COL_PINK);

    draw_text_glow(60, 24, 9, "SECRETS", COL_WHITE, COL_PINK);
    char buf[96];
    snprintf(buf, sizeof(buf), "%d EGGS FOUND", eggs_count());
    draw_text_glow(60, 104, 4, buf, COL_GOLD, RGB(140, 60, 0));
    for (int row = 0; row < NROWS; row++) {
        int left = row < NKINDS, x = left ? 60 : 640, y = 152 + (left ? row : row - NKINDS) * 88;
        int w = left ? 560 : 540, on = row == g->secret_row;
        draw_rect(x, y, w, 80, on ? COL_GOLD : RGB(60, 30, 80));
        draw_rect(x + 4, y + 4, w - 8, 72, on ? RGB(70, 40, 20) : COL_PAPER);
        if (left) {
            int opt[REWARD_KIND_MAX], n = options(row, opt), cur = worn_reward(row);
            draw_text(x + 18, y + 10, 3, REWARD_KIND_NAMES[row], on ? COL_GOLD : COL_DIM);
            const char *v = !n ? "FIND EGGS!" : cur >= 0 ? REWARDS[cur].name : "OFF";
            snprintf(buf, sizeof(buf), n ? "< %s >" : "%s", v);
            draw_text(x + 18, y + 38, ui_fit_scale(buf, w - 36, 4), buf, n ? COL_WHITE : RGB(110, 90, 140));
        } else {
            int r = party_reward(row - NKINDS), got = r >= 0 && reward_unlocked(r);
            const char *nm = got ? REWARDS[r].name : "? ? ?";
            draw_text(x + 18, y + 26, ui_fit_scale(nm, w - 150, 4), nm, got ? COL_WHITE : RGB(110, 90, 140));
            if (got) {
                int lit = reward_is_on(r);
                draw_rect(x + w - 124, y + 18, 100, 44, lit ? RGB(90, 255, 40) : RGB(80, 60, 100));
                draw_text(x + w - 74 - text_width(lit ? "ON" : "OFF", 4) / 2, y + 26, 4, lit ? "ON" : "OFF",
                          lit ? RGB(10, 40, 10) : COL_WHITE);
            }
        }
    }
    draw_text(640, 104, 4, "PARTY", COL_PINK);
    draw_text_glow(60, SCREEN_H - 60, 3, "UP DOWN PICK   LEFT RIGHT OR X CHANGE   CIRCLE BACK", RGB(220, 220, 255),
                   RGB(90, 90, 200));
}
