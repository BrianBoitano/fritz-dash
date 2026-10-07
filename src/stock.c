// Fritz Dash - stock icons and the STOCK ICONS browser.
#include <stdio.h>
#include <string.h>

#include "badges.h"
#include "draw.h"
#include "input.h"
#include "stock.h"
#include "synth.h"

#define COL_WHITE  RGB(255, 255, 255)
#define COL_YELLOW RGB(255, 235, 40)
#define COL_GREEN  RGB(90, 255, 40)
#define COL_PINK   RGB(255, 50, 200)
#define COL_CYAN   RGB(0, 230, 255)
#define COL_DIM    RGB(150, 150, 190)
#define COL_RED    RGB(255, 90, 90)
#define COL_SHADE  RGB(20, 14, 40)

// The grid (4 x 4 icons of one set) and the preview panel beside it.
#define COLS  4
#define ROWS  4
#define GX    60
#define GY    220
#define CW    262
#define CH    186
#define GPX   8
#define PX0   1220
#define PW    640
#define PH    (ROWS * CH + 28)

static uint8_t found[STOCK_MAX / 8];

void stock_find(int k)
{
    if (k >= 0 && k < STOCK_MAX)
        found[k / 8] |= (uint8_t)(1 << (k % 8));
}

int stock_found(int k)
{
    return k >= 0 && k < STOCK_MAX && (found[k / 8] >> (k % 8) & 1);
}

void stock_forget(void)
{
    memset(found, 0, sizeof(found));
}

int stock_lock(int k, int stars, int coins, uint32_t badges)
{
    const LookOption *o = &STOCK[k].opt;
    if (STOCK[k].secret && !stock_found(k))
        return STOCK_SECRET;
    if (o->stars > stars)
        return STOCK_STARS;
    if (o->coins > coins)
        return STOCK_COINS;
    if (o->badge && !(badges >> (o->badge - 1) & 1))
        return STOCK_BADGE;
    return STOCK_FREE;
}

color_t stock_color(color_t c, color_t c1, color_t c2)
{
    return c == STOCK_COL1 ? c1 : c == STOCK_COL2 ? c2 : c;
}

// One row of same-coloured pixels at a time; dim (0..256) fades it to the shade.
static void draw_pixels(const char *px, const color_t *pal, int x, int y, int s, color_t c1, color_t c2, int dim)
{
    for (int r = 0; r < STOCK_PX; r++)
        for (int c = 0; c < STOCK_PX;) {
            int v = px[r * STOCK_PX + c] - '0', e = c + 1;
            while (e < STOCK_PX && px[r * STOCK_PX + e] - '0' == v)
                e++;
            if (v)
                draw_rect(x + c * s, y + r * s, (e - c) * s, s, col_lerp(stock_color(pal[v - 1], c1, c2), COL_SHADE, dim));
            c = e;
        }
}

void stock_draw(int k, int x, int y, int px, color_t c1, color_t c2)
{
    if (k >= 0 && k < STOCK_COUNT)
        draw_pixels(STOCK[k].px, STOCK[k].pal, x, y, px, c1, c2, 0);
}

void pet_draw(int p, int f, int x, int y, int px)
{
    if (p >= 0 && p < PET_COUNT)
        draw_pixels(PETS[p].px[f & 1], PETS[p].pal, x, y, px, 0, 0, 0);
}

// ---------------------------------------------------------------------------
// The browser

static int set_list(int set, int *out)
{
    int n = 0;
    for (int k = 0; k < STOCK_COUNT && n < COLS * ROWS; k++)
        if (STOCK[k].set == set)
            out[n++] = k;
    return n;
}

void stockui_open(StockBrowser *b, int k)
{
    int list[COLS * ROWS];
    memset(b, 0, sizeof(*b));
    if (k < 0 || k >= STOCK_COUNT)
        return;
    b->set = STOCK[k].set;
    int n = set_list(b->set, list);
    for (int i = 0; i < n; i++)
        if (list[i] == k)
            b->sel = i;
}

int stockui_icon(const StockBrowser *b)
{
    int list[COLS * ROWS];
    int n = set_list(b->set, list);
    return list[b->sel < n ? b->sel : n - 1];
}

static void lock_text(char *buf, size_t n, int k, int why, int stars, int coins)
{
    const LookOption *o = &STOCK[k].opt;
    switch (why) {
    case STOCK_SECRET: snprintf(buf, n, "A SECRET! FIND IT IN AN EASTER EGG"); break;
    case STOCK_STARS: snprintf(buf, n, "NEEDS %d STARS - YOU HAVE %d", o->stars, stars); break;
    case STOCK_COINS: snprintf(buf, n, "NEEDS %d COINS - YOU HAVE %d", o->coins, coins); break;
    case STOCK_BADGE: snprintf(buf, n, "EARN THE %s BADGE", BADGES[o->badge - 1].name); break;
    default: snprintf(buf, n, "X TO WEAR IT"); break;
    }
}

int stockui_step(StockBrowser *b, int edges, int stars, int coins, uint32_t badges)
{
    int list[COLS * ROWS];
    int n = set_list(b->set, list), set = b->set, sel = b->sel;
    if (b->msg_frames > 0)
        b->msg_frames--;
    if (edges & IN_CIRCLE)
        return SBR_BACK;
    // Left and Right off the edge of the grid go on to the next set.
    if (edges & (IN_L1 | IN_R1))
        set = (set + ((edges & IN_R1) ? 1 : STOCK_SET_COUNT - 1)) % STOCK_SET_COUNT, sel = 0;
    else if ((edges & IN_LEFT) && sel % COLS == 0)
        set = (set + STOCK_SET_COUNT - 1) % STOCK_SET_COUNT, sel += COLS - 1;
    else if ((edges & IN_RIGHT) && (sel % COLS == COLS - 1 || sel == n - 1))
        set = (set + 1) % STOCK_SET_COUNT, sel -= sel % COLS;
    else if (edges & IN_LEFT)
        sel--;
    else if (edges & IN_RIGHT)
        sel++;
    else if ((edges & IN_UP) && sel >= COLS)
        sel -= COLS;
    else if ((edges & IN_DOWN) && sel / COLS < (n - 1) / COLS)
        sel = sel + COLS < n ? sel + COLS : n - 1;
    if (set != b->set || sel != b->sel) {
        n = set_list(set, list);
        b->set = set;
        b->sel = sel < n ? sel : n - 1;
        synth_sfx(SND_MOVE);
        return SBR_NONE;
    }
    if (edges & IN_CROSS) {
        int k = list[b->sel], why = stock_lock(k, stars, coins, badges);
        if (why == STOCK_FREE)
            return SBR_WEAR;
        lock_text(b->msg, sizeof(b->msg), k, why, stars, coins);
        b->msg_frames = 120;
        synth_sfx(SND_BACK);
    }
    return SBR_NONE;
}

static void text_in(int x, int w, int y, int scale, const char *s, color_t core, color_t glow)
{
    draw_text_glow(x + (w - text_width(s, scale)) / 2, y, scale, s, core, glow);
}

// The little sign on a locked icon: what it needs.
static void draw_need(int x, int y, int k, int why)
{
    const LookOption *o = &STOCK[k].opt;
    char buf[8];
    if (why == STOCK_BADGE) {
        draw_badge_medal(x, y, 22, o->badge - 1, 0);
        return;
    }
    snprintf(buf, sizeof(buf), "%d", why == STOCK_STARS ? o->stars : o->coins);
    draw_circle(x, y, 24, COL_SHADE);
    if (why == STOCK_STARS)
        ui_star((float)x, (float)y - 2, 17, COL_YELLOW);
    else
        draw_circle(x, y, 14, RGB(255, 212, 40));
    draw_text_glow(x - text_width(buf, 3) / 2, y + 14, 3, buf, COL_WHITE, COL_RED);
}

void stockui_draw(const StockBrowser *b, const Look *look, int stars, int coins, uint32_t badges, int frame)
{
    int list[COLS * ROWS];
    int n = set_list(b->set, list), k = stockui_icon(b);
    color_t c1 = LOOK_COLORS[look->v[LOOK_COL1] % LOOK_COUNT[LOOK_COL1]];
    color_t c2 = LOOK_COLORS[look->v[LOOK_COL2] % LOOK_COUNT[LOOK_COL2]];
    draw_set_offset(0, 0);
    run_draw_backdrop(LEVEL_PALETTES[5], (float)frame * 2.0f, 0);
    draw_text_glow_centered(30, 8, "STOCK ICONS", COL_WHITE, COL_PINK);

    // Set tabs.
    int tw = (SCREEN_W - 2 * GX) / STOCK_SET_COUNT;
    for (int s = 0; s < STOCK_SET_COUNT; s++) {
        int x = GX + s * tw, on = s == b->set;
        ui_panel(x + 4, 124, tw - 8, 70, on ? COL_GREEN : RGB(80, 60, 140));
        text_in(x, tw, 147, 3, STOCK_SET_NAMES[s], on ? COL_WHITE : COL_DIM, on ? COL_GREEN : RGB(40, 30, 70));
    }

    // The grid: each icon on a tile of his cube's colour, as it looks worn.
    ui_panel(GX, GY, COLS * CW + 32, PH, COL_CYAN);
    for (int i = 0; i < n; i++) {
        int cx = GX + 16 + (i % COLS) * CW, cy = GY + 14 + (i / COLS) * CH;
        int tx = cx + (CW - STOCK_PX * GPX) / 2, ty = cy + (CH - STOCK_PX * GPX) / 2;
        int why = stock_lock(list[i], stars, coins, badges);
        if (i == b->sel)
            draw_frame(cx + 6, cy + 4, CW - 12, CH - 8, 6, (frame / 8) % 2 ? COL_YELLOW : COL_WHITE);
        if (look->stock == list[i] + 1)
            draw_frame(tx - 16, ty - 16, STOCK_PX * GPX + 32, STOCK_PX * GPX + 32, 5, COL_GREEN);
        if (why == STOCK_SECRET) {
            draw_rect(tx - 8, ty - 8, STOCK_PX * GPX + 16, STOCK_PX * GPX + 16, RGB(40, 30, 76));
            draw_text_glow(tx + 34, ty + 22, 12, "?", COL_YELLOW, COL_PINK);
            continue;
        }
        draw_rect(tx - 8, ty - 8, STOCK_PX * GPX + 16, STOCK_PX * GPX + 16, col_lerp(c1, COL_SHADE, why ? 150 : 0));
        draw_pixels(STOCK[list[i]].px, STOCK[list[i]].pal, tx, ty, GPX, c1, c2, why ? 150 : 0);
        if (why)
            draw_need(tx + STOCK_PX * GPX - 6, ty + STOCK_PX * GPX - 20, list[i], why);
    }

    // The preview: its name, a big spinning cube wearing it, and the ship.
    char buf[48];
    int why = stock_lock(k, stars, coins, badges), secret = why == STOCK_SECRET;
    ui_panel(PX0, GY, PW, PH, COL_PINK);
    const char *name = secret ? "? ? ?" : STOCK[k].opt.name;
    text_in(PX0, PW, GY + 30, ui_fit_scale(name, PW - 60, 7), name, COL_WHITE, COL_PINK);
    if (b->set == STOCK[k].set && STOCK[k].secret) {
        int got = 0, all = 0;
        for (int j = 0; j < STOCK_COUNT; j++)
            if (STOCK[j].secret)
                all++, got += stock_found(j);
        snprintf(buf, sizeof(buf), "%d OF %d SECRETS FOUND", got, all);
        text_in(PX0, PW, GY + 90, 3, buf, COL_YELLOW, RGB(120, 60, 0));
    }
    if (secret) {
        text_in(PX0, PW, GY + 200, 24, "?", COL_YELLOW, COL_PINK);
    } else {
        Look wear = *look;
        wear.stock = k + 1;
        look_set(&wear);
        int t = frame % 60, air = t < 40;
        run_draw_pilot((float)(PX0 + PW / 2), (float)(GY + 320) - (air ? (float)(t * (40 - t)) * 0.15f : 0.0f), 2.4f,
                       air ? (float)t * (1.5707963f / 40.0f) : 0.0f);
        look_draw_ship((float)(PX0 + PW / 2), (float)(GY + 570) + fsin((float)frame * 0.08f) * 14.0f, 1.7f,
                       fsin((float)frame * 0.08f + 1.2f) * 0.2f);
        look_set(look);
    }
    if (look->stock == k + 1)
        snprintf(buf, sizeof(buf), "YOU ARE WEARING IT");
    else
        lock_text(buf, sizeof(buf), k, why, stars, coins);
    text_in(PX0, PW, GY + PH - 70, ui_fit_scale(buf, PW - 40, 4), buf, why ? COL_RED : COL_GREEN, COL_SHADE);

    if (b->msg_frames > 0)
        draw_text_glow_centered(GY + PH + 8, 4, b->msg, COL_YELLOW, COL_PINK);
    draw_text_glow_centered(SCREEN_H - 44, 3, "L1 R1 SETS   D-PAD PICK   X WEAR IT   CIRCLE BACK",
                            RGB(220, 220, 255), RGB(90, 90, 200));
}
