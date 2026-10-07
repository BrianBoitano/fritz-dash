// Fritz Dash - drawing a level: backdrop, objects, player, practice markers, HUD.
#include <stdio.h>
#include <string.h>

#include "badges.h"
#include "draw.h"
#include "eggs.h"
#include "icons.h"
#include "mode.h"
#include "play.h"
#include "stock.h"
#include "transform.h"

#define COL_CUBE      RGB(90, 255, 40)
#define COL_CUBE_CORE RGB(0, 230, 255)
#define COL_PINK      RGB(255, 50, 200)
#define COL_YELLOW    RGB(255, 235, 40)
#define COL_BLUE      RGB(40, 150, 255)
#define COL_ORANGE    RGB(255, 150, 30)
#define COL_WHITE     RGB(255, 255, 255)
#define COL_DARK      RGB(10, 4, 22)
#define COL_RED       RGB(255, 70, 50)
#define COL_WAVE      RGB(40, 210, 255)

// Camera of the frame being drawn: world x at screen x 0, and the world y
// shown on the floor line GROUND_Y. cur is the level's colours this frame.
static float cam_x, cam_y;
static LevelColors cur;
static const Body *moved;      // the body whose groups have moved; NULL draws every object at home
static float beat;             // run_beat of the frame: pulses throb with it
static int coins_had, coins_silver;
static const Run *ghost, *rival;
static const int (*deaths)[2];
static int ndeaths;
static int ghost_hint;
static const float (*hint_path)[2];
static int hint_npath;
static int faded;              // drawing a ghost: colours fade this much (of 256) into the background
static const Run *drawing;     // the run being drawn, for objects that show its state
static float view_mx, view_my; // zoomed out (or mid flip): world drawn this far past the screen's edges
static int deco_theme;         // THEMES decoration behind the grid (0 none)

static float sx(float wx) { return wx - cam_x; }
static float sy(float wy) { return (float)GROUND_Y - (wy - cam_y); }

static int wrap(int v, int m)
{
    return ((v % m) + m) % m;
}

static void draw_theme_deco(int theme, LevelColors c, float cx, int frame);

void run_draw_backdrop(LevelColors c, float cx, float cy)
{
    int mx = (int)view_mx, my = (int)view_my, x0 = -mx, x1 = SCREEN_W + mx, y0 = -my, y1 = SCREEN_H + my;
    draw_gradient(c.top, c.bottom);
    if (deco_theme)
        draw_theme_deco(deco_theme, c, cx, drawing ? drawing->frame : 0);
    if (drawing)
        rw_backdrop(c, cx, cy);

    color_t grid = col_lerp(c.bottom, c.line, 40);
    for (int x = -wrap((int)(cx * 0.25f), 192) - (mx / 192 + 1) * 192; x < x1; x += 192)
        draw_rect(x, y0, 3, y1 - y0, grid);
    for (int y = 120 + wrap((int)(cy * 0.25f), 192) - 192 - (my / 192 + 1) * 192; y < y1; y += 192)
        draw_rect(x0, y, x1 - x0, 3, grid);

    // Ground at world height 0, with a neon edge and scrolling tiles.
    int gy = (int)((float)GROUND_Y + cy);
    if (gy >= y1 + 6)
        return;
    draw_rect(x0, gy, x1 - x0, y1 - gy, c.ground);
    color_t tile = col_lerp(c.ground, c.line, 50);
    for (int x = -wrap((int)cx, 160) - (mx / 160 + 1) * 160; x < x1; x += 160)
        draw_rect(x, gy + 6, 4, y1 - gy - 6, tile);
    draw_rect(x0, gy + 110, x1 - x0, 4, tile);
    draw_rect(x0, gy - 6, x1 - x0, 6, col_lerp(c.bottom, c.line, 90));
    if (!drawing || !rw_floor(gy, x0, x1, cx))
        draw_rect(x0, gy, x1 - x0, 6, c.line);
    draw_rect(x0, gy + 6, x1 - x0, 4, col_lerp(c.ground, c.line, 110));
}

// Polygon given in cell-local points (u, v), turned by rot.
static void cell_poly(const LevelObj *o, int rot, int n, const float *uv, color_t c)
{
    float xs[8], ys[8];
    for (int i = 0; i < n; i++) {
        float u, v;
        cell_rotate(rot, uv[2 * i], uv[2 * i + 1], &u, &v);
        xs[i] = sx(((float)o->x + u) * TILE);
        ys[i] = sy(((float)o->y + v) * TILE);
    }
    draw_poly(n, xs, ys, c);
}

static void draw_block(const LevelObj *o, color_t line)
{
    int x = (int)sx((float)o->x * TILE), y = (int)sy((float)(o->y + 1) * TILE);
    draw_rect(x, y, TILE, TILE, line);
    draw_rect(x + 6, y + 6, TILE - 12, TILE - 12, RGB(8, 4, 20));
    draw_frame(x + 22, y + 22, TILE - 44, TILE - 44, 4, col_scale(line, 2, 3));
}

static void draw_platform(const LevelObj *o, color_t line)
{
    static const float OUT[] = { 0, 0.5f, 1, 0.5f, 1, 1, 0, 1 };
    static const float IN[] = { 0.07f, 0.58f, 0.93f, 0.58f, 0.93f, 0.92f, 0.07f, 0.92f };
    cell_poly(o, o->rot, 4, OUT, line);
    cell_poly(o, o->rot, 4, IN, RGB(8, 4, 20));
}

void draw_cell_block(const LevelObj *o, int platform)
{
    if (platform)
        draw_platform(o, cur.line);
    else
        draw_block(o, cur.line);
}

static void draw_spike(const LevelObj *o, color_t glow, float h)
{
    float g[] = { -0.075f, 0, 0.5f, 1.175f * h, 1.075f, 0 };
    float s[] = { 0, 0, 0.5f, h, 1, 0 };
    float in[] = { 0.14f, 0.05f, 0.5f, 0.79f * h, 0.86f, 0.05f };
    cell_poly(o, o->rot, 3, g, col_scale(glow, 1, 2));
    cell_poly(o, o->rot, 3, s, COL_WHITE);
    cell_poly(o, o->rot, 3, in, COL_DARK);
}

static void draw_pad(const LevelObj *o, color_t c)
{
    static const float GLOW[] = { 0.02f, 0, 0.98f, 0, 0.84f, 0.3f, 0.16f, 0.3f };
    static const float CORE[] = { 0.1f, 0, 0.9f, 0, 0.76f, 0.2f, 0.24f, 0.2f };
    cell_poly(o, o->rot, 4, GLOW, col_scale(c, 1, 3));
    cell_poly(o, o->rot, 4, CORE, c);
}

static void draw_orb(const LevelObj *o, color_t c, int frame)
{
    int x = (int)sx(((float)o->x + 0.5f) * TILE), y = (int)sy(((float)o->y + 0.5f) * TILE);
    int pulse = (frame / 3) % 8;
    draw_circle(x, y, 40 + pulse, col_scale(c, 1, 3));
    draw_circle(x, y, 34 + pulse, COL_DARK);
    draw_circle(x, y, 27, c);
    draw_circle(x, y, 13, col_lerp(c, COL_WHITE, 170));
}

static void draw_oval8(float cx, float cy, float rx, float ry, color_t c)
{
    float k = 0.414f;
    float xs[8] = { cx - rx * k, cx + rx * k, cx + rx, cx + rx, cx + rx * k, cx - rx * k, cx - rx, cx - rx };
    float ys[8] = { cy - ry, cy - ry, cy - ry * k, cy + ry * k, cy + ry, cy + ry, cy + ry * k, cy - ry * k };
    draw_poly(8, xs, ys, c);
}

static void draw_portal(const LevelObj *o, int frame)
{
    float x = sx(((float)o->x + 0.5f) * TILE), y = sy(((float)o->y + 0.5f) * TILE);
    color_t c = COL_CUBE;
    if (o->type == OBJ_GRAVITY_UP) c = COL_YELLOW;
    else if (o->type == OBJ_GRAVITY_DOWN) c = COL_BLUE;
    else if (o->type == OBJ_SHIP_PORTAL) c = COL_PINK;
    else if (o->type == OBJ_BALL_PORTAL) c = COL_RED;
    else if (o->type == OBJ_UFO_PORTAL) c = COL_ORANGE;
    else if (o->type == OBJ_WAVE_PORTAL) c = COL_WAVE;
    int glow = 6 + (frame / 4) % 5;
    draw_oval8(x, y, 30 + glow, 118 + glow, col_scale(c, 1, 3));
    draw_oval8(x, y, 30, 118, c);
    draw_oval8(x, y, 19, 103, COL_DARK);

    if (o->type == OBJ_GRAVITY_UP || o->type == OBJ_GRAVITY_DOWN) {
        float d = o->type == OBJ_GRAVITY_UP ? -1.0f : 1.0f;
        float xs[3] = { x - 12, x + 12, x }, ys[3] = { y - 10 * d, y - 10 * d, y + 14 * d };
        draw_poly(3, xs, ys, c);
    } else if (o->type == OBJ_SHIP_PORTAL) {
        float xs[3] = { x - 11, x + 13, x - 11 }, ys[3] = { y - 12, y, y + 12 };
        draw_poly(3, xs, ys, c);
    } else if (o->type == OBJ_BALL_PORTAL) {
        draw_circle((int)x, (int)y, 13, c);
        draw_rect((int)x - 13, (int)y - 2, 26, 4, COL_DARK);
    } else if (o->type == OBJ_UFO_PORTAL) {
        draw_oval8(x, y + 4, 15, 6, c);
        draw_rect((int)x - 6, (int)y - 9, 12, 9, c);
    } else if (o->type == OBJ_WAVE_PORTAL) {
        float xs[3] = { x - 11, x + 13, x - 4 }, ys[3] = { y + 12, y - 12, y + 4 };
        draw_poly(3, xs, ys, c);
    } else {
        draw_rect((int)x - 10, (int)y - 10, 20, 20, c);
    }
}

// Size portals are rounder: arrows point in for mini, out for big.
static void draw_size_portal(const LevelObj *o, int frame)
{
    float x = sx(((float)o->x + 0.5f) * TILE), y = sy(((float)o->y + 0.5f) * TILE);
    int mini = o->type == OBJ_MINI_PORTAL;
    color_t c = mini ? COL_PINK : COL_CUBE;
    int glow = 5 + (frame / 4) % 5;
    draw_oval8(x, y, 36 + glow, 104 + glow, col_scale(c, 1, 3));
    draw_oval8(x, y, 36, 104, c);
    draw_oval8(x, y, 24, 90, COL_DARK);
    for (int k = -1; k <= 1; k += 2) {
        float tip = mini ? y - 10.0f * (float)k : y - 42.0f * (float)k, base = mini ? y - 42.0f * (float)k : y - 10.0f * (float)k;
        float xs[3] = { x - 11, x + 11, x }, ys[3] = { base, base, tip };
        draw_poly(3, xs, ys, c);
    }
}

static void draw_speed_portal(const LevelObj *o, int frame)
{
    static const color_t COLS[4] = { COL_ORANGE, COL_BLUE, RGB(60, 255, 90), COL_PINK };
    int n = o->type - OBJ_SPEED_05;
    color_t c = COLS[n];
    float cx = sx(((float)o->x + 0.5f) * TILE), cy = sy(((float)o->y + 0.5f) * TILE);
    int glow = (frame / 4) % 5;
    draw_oval8(cx, cy, 40 + glow, 66 + glow, col_scale(c, 1, 4));
    float w = 34.0f + 16.0f * (float)n;
    for (int i = 0; i <= n; i++) {
        float x = cx - w / 2 + 16.0f * (float)i;
        float ux[4] = { x, x + 12, x + 34, x + 22 }, uy[4] = { cy - 36, cy - 36, cy, cy };
        float lx[4] = { x + 22, x + 34, x + 12, x }, ly[4] = { cy, cy, cy + 36, cy + 36 };
        draw_poly(4, ux, uy, c);
        draw_poly(4, lx, ly, c);
    }
}

static void draw_saw(const LevelObj *o, float r, int frame)
{
    float cx = sx(((float)o->x + 0.5f) * TILE), cy = sy(((float)o->y + 0.5f) * TILE);
    float a = (float)frame * (r > 60 ? 0.07f : 0.13f);
    int teeth = r > 60 ? 18 : 10;
    float half = 3.14159265f / (float)teeth * 0.75f;
    color_t tooth = RGB(225, 225, 240);
    for (int k = 0; k < teeth; k++) {
        float t = a + (float)k * 6.2831853f / (float)teeth;
        float xs[3] = { cx + fcos(t - half) * r * 0.74f, cx + fcos(t) * r, cx + fcos(t + half) * r * 0.74f };
        float ys[3] = { cy + fsin(t - half) * r * 0.74f, cy + fsin(t) * r, cy + fsin(t + half) * r * 0.74f };
        draw_poly(3, xs, ys, tooth);
    }
    draw_circle((int)cx, (int)cy, (int)(r * 0.78f), col_scale(cur.line, 1, 2));
    draw_circle((int)cx, (int)cy, (int)(r * 0.68f), COL_DARK);
    for (int k = 0; k < 3; k++) {
        float t = -a * 1.5f + (float)k * 2.0943951f, c = fcos(t), sn = fsin(t), w = r * 0.07f, l = r * 0.6f;
        float xs[4] = { cx - sn * w, cx + c * l - sn * w, cx + c * l + sn * w, cx + sn * w };
        float ys[4] = { cy + c * w, cy + sn * l + c * w, cy + sn * l - c * w, cy - c * w };
        draw_poly(4, xs, ys, col_scale(cur.line, 3, 4));
    }
    draw_circle((int)cx, (int)cy, (int)(r * 0.2f), cur.line);
    draw_circle((int)cx, (int)cy, (int)(r * 0.09f), COL_WHITE);
}

// A glowing ball that swells on every beat of the song.
static void draw_pulse(const LevelObj *o)
{
    int cx = (int)sx(((float)o->x + 0.5f) * TILE), cy = (int)sy(((float)o->y + 0.5f) * TILE);
    int e = (int)(beat * 256), r = 14 + (int)(beat * 16);
    draw_circle(cx, cy, r + 16, col_lerp(cur.bottom, cur.line, 50 + e / 4));
    draw_circle(cx, cy, r + 7, col_lerp(cur.bottom, cur.line, 120 + e / 3));
    draw_circle(cx, cy, r, col_lerp(cur.line, COL_WHITE, 60 + e * 3 / 4));
}

// Three equaliser bars jumping to the beat.
static void draw_pulse_bars(const LevelObj *o)
{
    static const float AMP[3] = { 0.55f, 1.0f, 0.75f };
    int x = (int)sx((float)o->x * TILE), y = (int)sy((float)o->y * TILE);
    for (int k = 0; k < 3; k++) {
        int h = (int)((0.18f + 0.72f * AMP[k] * beat) * TILE);
        color_t c = k == 1 ? COL_PINK : cur.line;
        draw_rect(x + 8 + k * 24, y - h, 18, h, col_lerp(c, cur.bottom, 60));
        draw_rect(x + 11 + k * 24, y - h + 3, 12, h - 3, c);
    }
}

static void draw_deco(const LevelObj *o, int frame)
{
    color_t dim = col_lerp(cur.bottom, cur.line, 110);
    switch (o->type) {
    case OBJ_DECO_GLOW: {
        static const float GLOW[8] = { 0, 0.4f, 1, 0.4f, 1, 0.6f, 0, 0.6f };
        static const float CORE[8] = { 0, 0.46f, 1, 0.46f, 1, 0.54f, 0, 0.54f };
        cell_poly(o, o->rot, 4, GLOW, col_lerp(cur.bottom, cur.line, 90));
        cell_poly(o, o->rot, 4, CORE, cur.line);
        break;
    }
    case OBJ_DECO_STAR: {
        float cx = sx(((float)o->x + 0.5f) * TILE), cy = sy(((float)o->y + 0.5f) * TILE);
        int tw = (frame / 5 + o->x * 3 + o->y) % 8;
        float s = 10.0f + (float)(tw < 4 ? tw : 8 - tw) * 3.0f;
        draw_rot_square(cx, cy, s * 1.6f, PI_F / 4, col_lerp(cur.bottom, COL_YELLOW, 120));
        draw_rect((int)cx - 2, (int)(cy - s * 1.4f), 4, (int)(s * 2.8f), COL_WHITE);
        draw_rect((int)(cx - s * 1.4f), (int)cy - 2, (int)(s * 2.8f), 4, COL_WHITE);
        draw_rot_square(cx, cy, s * 0.7f, PI_F / 4, COL_YELLOW);
        break;
    }
    case OBJ_DECO_ARROW: {
        float slide = (float)((frame / 3) % 10) * 0.02f;
        for (int k = 0; k < 2; k++) {
            float u = 0.18f + 0.34f * (float)k + slide;
            float up[8] = { u, 0.82f, u + 0.12f, 0.82f, u + 0.34f, 0.5f, u + 0.22f, 0.5f };
            float dn[8] = { u + 0.22f, 0.5f, u + 0.34f, 0.5f, u + 0.12f, 0.18f, u, 0.18f };
            cell_poly(o, o->rot, 4, up, k ? cur.line : dim);
            cell_poly(o, o->rot, 4, dn, k ? cur.line : dim);
        }
        break;
    }
    case OBJ_DECO_BLOCK: {
        int x = (int)sx((float)o->x * TILE), y = (int)sy((float)(o->y + 1) * TILE);
        draw_rect(x, y, TILE, TILE, col_lerp(cur.bottom, cur.line, 45));
        draw_rect(x + 3, y + 3, TILE - 6, TILE - 6, col_lerp(cur.bottom, COL_DARK, 150));
        draw_rect(x + 30, y + 30, TILE - 60, TILE - 60, col_lerp(cur.bottom, cur.line, 45));
        break;
    }
    case OBJ_DECO_SPIKES:
        // Thin leaning blades, so grass never reads as a row of spikes.
        for (int k = 0; k < 6; k++) {
            float u = 0.05f + 0.16f * (float)k, h = 0.16f + 0.06f * (float)((k * 5) % 3);
            float sp[6] = { u, 0, u + 0.07f + ((k & 1) ? 0.05f : -0.03f), h, u + 0.05f, 0 };
            cell_poly(o, o->rot, 3, sp, dim);
        }
        break;
    }
}

// Coin colours: gold, silver for player-made levels, faded when saved already.
static void coin_colors(int had, color_t *rim, color_t *face)
{
    *rim = coins_silver ? RGB(130, 140, 175) : RGB(205, 130, 0);
    *face = coins_silver ? RGB(215, 225, 245) : RGB(255, 212, 40);
    if (had) {
        *rim = col_lerp(*rim, cur.bottom, 150);
        *face = col_lerp(*face, cur.bottom, 150);
    }
}

static void draw_coin_at(float cx, float cy, float r, float squash, int had)
{
    color_t rim, face;
    coin_colors(had, &rim, &face);
    draw_oval8(cx, cy, r * squash + 7, r + 7, col_lerp(cur.bottom, face, 90));
    draw_oval8(cx, cy, r * squash, r, rim);
    draw_oval8(cx, cy, r * 0.78f * squash, r * 0.78f, face);
    if (squash > 0.55f) {
        draw_rect((int)(cx - r * 0.12f * squash), (int)(cy - r * 0.5f), (int)(r * 0.24f * squash) + 1, (int)r, rim);
        draw_rect((int)(cx - r * 0.42f * squash), (int)(cy - r * 0.12f), (int)(r * 0.84f * squash) + 1, (int)(r * 0.24f), rim);
    }
}

static void draw_coin(const LevelObj *o, int frame)
{
    int bit = o->param < MAX_COINS ? 1 << o->param : 0;
    if (moved && (moved->coins & bit))
        return;
    float spin = fcos((float)frame * 0.06f + (float)o->x * 0.7f);
    float cx = sx(((float)o->x + 0.5f) * TILE);
    float cy = sy(((float)o->y + 0.5f) * TILE) + fsin((float)frame * 0.08f + (float)o->x) * 5.0f;
    draw_coin_at(cx, cy, 34, 0.3f + 0.7f * (spin < 0 ? -spin : spin), (coins_had & bit) != 0);
}

// A signpost with words from SIGN_TEXTS, big enough to read from the sofa.
static void draw_sign(const LevelObj *o)
{
    const char *t = SIGN_TEXTS[o->param % NUM_SIGNS];
    int tw = text_width(t, 4), cx = (int)sx(((float)o->x + 0.5f) * TILE), cy = (int)sy(((float)o->y + 0.5f) * TILE);
    int foot = (int)sy((float)(o->y > 1 ? o->y - 1 : 0) * TILE);
    draw_rect(cx - 5, cy, 10, foot - cy, col_lerp(cur.bottom, cur.line, 90));
    draw_rect(cx - tw / 2 - 24, cy - 34, tw + 48, 68, cur.line);
    draw_rect(cx - tw / 2 - 18, cy - 28, tw + 36, 56, COL_DARK);
    draw_text(cx - tw / 2, cy - 14, 4, t, COL_YELLOW);
}

static void draw_end(int end_x)
{
    int x = (int)sx((float)end_x);
    int bottom = (int)sy(0);
    if (x < -120 - view_mx || x > SCREEN_W + 40 + view_mx)
        return;
    draw_rect(x - 14, -(int)view_my, 108, bottom + (int)view_my, col_scale(cur.line, 1, 3));
    for (int y = bottom - 40, row = 0; y > -40 - view_my; y -= 40, row++)
        for (int col = 0; col < 2; col++)
            draw_rect(x + col * 40, y, 40, 40, (row + col) % 2 ? COL_WHITE : cur.line);
}

void draw_portal_frame(float x, float y, color_t c, int frame)
{
    int glow = 6 + (frame / 4) % 5;
    draw_oval8(x, y, 34 + glow, 118 + glow, col_scale(c, 1, 3));
    draw_oval8(x, y, 34, 118, c);
    draw_oval8(x, y, 23, 103, COL_DARK);
}

// A 3.2 mode's portal: its colour, with its vehicle small inside.
static void draw_mode_portal(const LevelObj *o, int mode, int frame)
{
    float x = sx(((float)o->x + 0.5f) * TILE), y = sy(((float)o->y + 0.5f) * TILE);
    draw_portal_frame(x, y, mode_def(mode)->color, frame);
    draw_mode_icon(mode, x, y, 0.38f, frame);
}

static void draw_object(const LevelObj *o, int frame)
{
    switch (o->type) {
    case OBJ_BLOCK: draw_block(o, cur.line); break;
    case OBJ_PLATFORM: draw_platform(o, cur.line); break;
    case OBJ_SPIKE: draw_spike(o, cur.line, 1.0f); break;
    case OBJ_HALF_SPIKE: draw_spike(o, cur.line, 0.5f); break;
    case OBJ_PAD_YELLOW: draw_pad(o, COL_YELLOW); break;
    case OBJ_PAD_PINK: draw_pad(o, COL_PINK); break;
    case OBJ_ORB_YELLOW: draw_orb(o, COL_YELLOW, frame); break;
    case OBJ_ORB_PINK: draw_orb(o, COL_PINK, frame); break;
    case OBJ_SPEED_05: case OBJ_SPEED_1: case OBJ_SPEED_2: case OBJ_SPEED_3:
        draw_speed_portal(o, frame);
        break;
    case OBJ_GRAVITY_UP: case OBJ_GRAVITY_DOWN: case OBJ_CUBE_PORTAL: case OBJ_SHIP_PORTAL:
    case OBJ_BALL_PORTAL: case OBJ_UFO_PORTAL: case OBJ_WAVE_PORTAL:
        draw_portal(o, frame);
        break;
    case OBJ_MINI_PORTAL: case OBJ_BIG_PORTAL:
        draw_size_portal(o, frame);
        break;
    case OBJ_SAW: draw_saw(o, 0.5f * TILE, frame); break;
    case OBJ_SAW_BIG: draw_saw(o, 1.42f * TILE, frame); break;
    case OBJ_PULSE: draw_pulse(o); break;
    case OBJ_COIN: draw_coin(o, frame); break;
    case OBJ_SIGN: draw_sign(o); break;
    case OBJ_PULSE_BARS: draw_pulse_bars(o); break;
    case OBJ_DECO_GLOW: case OBJ_DECO_STAR: case OBJ_DECO_ARROW: case OBJ_DECO_BLOCK: case OBJ_DECO_SPIKES:
        draw_deco(o, frame);
        break;
    default: {
        const ObjDef *d = obj_def(o->type);
        int m = run_portal_mode(o->type);
        if (m >= 0)
            draw_mode_portal(o, m, frame);
        else if (d && d->draw)
            d->draw(o, sx(((float)o->x + 0.5f) * TILE), sy(((float)o->y + 0.5f) * TILE), frame);
        break;
    }
    }
}

// Where a block has morphed into air: a faint dashed outline.
static void draw_air(const LevelObj *o)
{
    int x = (int)sx((float)o->x * TILE), y = (int)sy((float)(o->y + 1) * TILE);
    color_t c = col_lerp(cur.bottom, cur.line, 90);
    for (int k = 0; k < TILE; k += 20) {
        draw_rect(x + k, y, 10, 3, c);
        draw_rect(x + k, y + TILE - 3, 10, 3, c);
        draw_rect(x, y + k, 3, 10, c);
        draw_rect(x + TILE - 3, y + k, 3, 10, c);
    }
}

// A grouped object as morph blocks have it now, blinking toward what it
// turns into next during the warning beat.
static void draw_moving(const LevelObj *o, int frame)
{
    LevelObj tmp = *o;
    int next;
    if (drawing && moved && o->group && moved->mb_to[(o->group - 1) % MAX_GROUPS]) {
        tmp.type = (uint8_t)morph_type(drawing, moved, o);
        if (morph_view(drawing, moved, o, &next) && (frame / 5) % 2)
            tmp.type = (uint8_t)next;
    }
    if (tmp.type == OBJ_NONE)
        draw_air(&tmp);
    else
        draw_object(&tmp, frame);
}

static void draw_objects(const Level *lv, int frame)
{
    int last = (int)((cam_x + SCREEN_W + view_mx) / TILE) + 2;
    for (int i = level_first_at(lv, (int)((cam_x - view_mx) / TILE) - 2); i < lv->nobj && lv->objs[i].x <= last; i++) {
        const LevelObj *o = &lv->objs[i];
        float top = sy((float)(o->y + 1) * TILE);
        if (top > SCREEN_H + 3 * TILE + view_my || top < -4 * TILE - view_my || (moved && obj_grouped(o)))
            continue;
        draw_object(o, frame);
    }
    // Moving objects: drawn at home with the camera shifted back by their offset.
    float cx = cam_x, cy = cam_y;
    for (int k = 0; moved && k < lv->nmoving; k++) {
        const LevelObj *o = &lv->objs[lv->moving[k]];
        float ox, oy;
        run_group_offset(moved, o->group, &ox, &oy);
        cam_x = cx - ox;
        cam_y = cy - oy;
        float x = sx((float)o->x * TILE), top = sy((float)(o->y + 1) * TILE);
        if (x > -3 * TILE - view_mx && x < SCREEN_W + 2 * TILE + view_mx && top < SCREEN_H + 3 * TILE + view_my &&
            top > -4 * TILE - view_my)
            draw_moving(o, frame);
    }
    cam_x = cx;
    cam_y = cy;
    draw_end(lv->end_x);
}

void level_draw(const Level *lv, LevelColors c, float cx, float cy, int frame)
{
    cam_x = cx;
    cam_y = cy;
    cur = c;
    moved = NULL;
    drawing = NULL;
    coins_had = coins_silver = 0;
    beat = (float)(30 - frame % 30) / 30.0f;
    beat *= beat;
    draw_set_offset(0, 0);
    run_draw_backdrop(c, cam_x, cam_y);
    draw_objects(lv, frame);
}

void level_draw_object(const LevelObj *o, int frame)
{
    if (o->type == OBJ_END)
        draw_end(o->x * TILE);
    else
        draw_object(o, frame);
}

static void corridor_span(float x0, float x1, float lo, float hi)
{
    color_t wall = col_scale(cur.top, 1, 2), line = cur.line;
    int a = x0 < -view_mx ? (int)-view_mx : (int)x0, b = x1 > SCREEN_W + view_mx ? (int)(SCREEN_W + view_mx) : (int)x1;
    int hy = (int)sy(hi), ly = (int)sy(lo), top = (int)-view_my;
    if (a >= b)
        return;
    draw_rect(a, top, b - a, hy - 6 - top, wall);
    draw_rect(a, hy - 6, b - a, 6, line);
    if (lo > 0) {
        draw_rect(a, ly, b - a, 6, line);
        draw_rect(a, ly + 6, b - a, (int)sy(0) - ly - 6, wall);
    }
}

// Flying corridors start at their portal: the one the player flies in from
// the portal that began it, and the ones ahead from their own portals, so the
// ceiling of a coming tunnel never hangs in the air above a cube.
static void draw_bounds(const Run *r)
{
    const Level *lv = r->lv;
    const Body *b = &r->b;
    float left = cam_x - TILE - view_mx, right = cam_x + SCREEN_W + TILE + view_mx, start = left, end = right;
    float front = b->x + run_half_w(b);     // portals from here on are not taken yet
    int ahead = -1;
    for (int i = level_first_at(lv, (int)(left / TILE)); i < lv->nobj && lv->objs[i].x * TILE <= right; i++) {
        const LevelObj *o = &lv->objs[i];
        int m = run_portal_mode(o->type);
        if (m < 0 || obj_grouped(o))
            continue;
        float x = ((float)o->x + 0.5f) * TILE, lo, hi;
        run_corridor(m, o, &lo, &hi);
        if (((float)o->x + 0.1f) * TILE <= front) {
            if (m == b->mode && lo == b->lo && hi == b->hi)
                start = x;
        } else if (ahead < 0) {
            ahead = i;
            end = x;
        }
    }
    if (mode_def(b->mode)->corridor)
        corridor_span(sx(start), sx(end), b->lo, b->hi);
    for (int i = ahead; i >= 0 && i < lv->nobj && lv->objs[i].x * TILE <= right; i++) {
        const LevelObj *o = &lv->objs[i];
        int m = run_portal_mode(o->type);
        if (m < 0 || obj_grouped(o))
            continue;
        float x = ((float)o->x + 0.5f) * TILE, x1 = right, lo, hi;
        for (int k = i + 1; k < lv->nobj && lv->objs[k].x * TILE <= right; k++)
            if (run_portal_mode(lv->objs[k].type) >= 0 && !obj_grouped(&lv->objs[k])) {
                x1 = ((float)lv->objs[k].x + 0.5f) * TILE;
                break;
            }
        run_corridor(m, o, &lo, &hi);
        if (mode_def(m)->corridor)
            corridor_span(sx(x), sx(x1), lo, hi);
    }
}

// New options go at the end of each list: custom.txt saves their positions.
static const LookOption COLOR_OPTS[] = {
    { "GREEN", 0, 0, 0 }, { "CYAN", 0, 0, 0 }, { "PINK", 0, 0, 0 }, { "YELLOW", 0, 0, 0 }, { "ORANGE", 1, 0, 0 },
    { "RED", 2, 0, 0 }, { "BLUE", 3, 0, 0 }, { "PURPLE", 5, 0, 0 }, { "WHITE", 8, 0, 0 }, { "BLACK", 12, 0, 0 },
    { "GOLD", 0, 3, 0 }, { "MINT", 0, 0, B_BUILDER + 1 }, { "CORAL", 40, 0, 0 }, { "TEAL", 0, 45, 0 },
};
const color_t LOOK_COLORS[] = {
    RGB(90, 255, 40), RGB(0, 230, 255), RGB(255, 50, 200), RGB(255, 235, 40), RGB(255, 140, 20),
    RGB(255, 50, 50), RGB(60, 110, 255), RGB(170, 70, 255), RGB(240, 240, 240), RGB(40, 40, 52),
    RGB(255, 196, 30), RGB(120, 255, 200), RGB(255, 118, 104), RGB(0, 168, 150),
};
static const LookOption FACE_OPTS[] = {
    { "CLASSIC", 0, 0, 0 }, { "SMILE", 0, 0, 0 }, { "COOL", 1, 0, 0 }, { "ANGRY", 3, 0, 0 },
    { "ROBOT", 5, 0, 0 }, { "CAT", 8, 0, 0 }, { "CYCLOPS", 11, 0, 0 }, { "STAR", 15, 0, 0 },
    { "ALIEN", 0, 6, 0 }, { "WINK", 0, 0, B_FIRST_WIN + 1 },
    { "MY ICON 1", 0, 0, 0 }, { "MY ICON 2", 0, 0, 0 }, { "MY ICON 3", 0, 0, 0 }, { "MY ICON 4", 0, 0, 0 },
    { "MY ICON 5", 0, 0, 0 }, { "MY ICON 6", 0, 0, 0 }, { "NINJA", 60, 0, 0 }, { "DEMON", 220, 0, 0 },
};
static const LookOption SHIP_OPTS[] = {
    { "ROCKET", 0, 0, 0 }, { "JET", 3, 0, 0 }, { "SAUCER", 6, 0, 0 }, { "SHARK", 10, 0, 0 }, { "STARSHIP", 120, 0, 0 },
};
static const LookOption TRAIL_OPTS[] = {
    { "SQUARES", 0, 0, 0 }, { "NONE", 0, 0, 0 }, { "LINE", 2, 0, 0 }, { "SPARKS", 5, 0, 0 }, { "RAINBOW", 9, 0, 0 },
    { "STARS", 0, 9, 0 }, { "BUBBLES", 0, 0, B_ORBS + 1 }, { "FIRE", 90, 0, 0 }, { "COMET", 250, 0, 0 },
};
static const LookOption DEATH_OPTS[] = {
    { "BURST", 0, 0, 0 }, { "RING", 2, 0, 0 }, { "SHATTER", 4, 0, 0 }, { "FIREWORKS", 7, 0, 0 },
    { "COIN SHOWER", 0, 12, 0 }, { "CONFETTI", 0, 0, B_ATTEMPTS100 + 1 }, { "SUPERNOVA", 160, 0, 0 },
    { "LAVA SPLASH", 0, 90, 0 },
};

const int LOOK_COUNT[LOOK_PARTS] = { 14, 14, 18, 5, 9, 8 };
const LookOption *const LOOK_OPTIONS[LOOK_PARTS] = { COLOR_OPTS, COLOR_OPTS, FACE_OPTS, SHIP_OPTS, TRAIL_OPTS, DEATH_OPTS };

static Look look = { { 0, 1, 0, 0, 0, 0 }, 0 };
static float pscale = 1.0f;     // 0.5 while drawing a mini player

void look_set(const Look *l)
{
    look = *l;
}

static color_t look_col(int part)
{
    color_t c = LOOK_COLORS[look.v[part] % LOOK_COUNT[LOOK_COL1]];
    return faded ? col_lerp(c, cur.bottom, faded) : c;
}

void run_set_ghost(const Run *g, int hint)
{
    ghost = g;
    ghost_hint = hint;
}

void run_set_hint_path(const float (*xy)[2], int n)
{
    hint_path = xy;
    hint_npath = xy ? n : 0;
}

// The hint ghost stands out even right on top of the player: dots where it
// goes next, a pulsing ring, and a FOLLOW ME tag with a question mark.
static void draw_hint_path(int frame)
{
    for (int i = 0; i < hint_npath; i++) {
        int x = (int)sx(hint_path[i][0]), y = (int)sy(hint_path[i][1]), s = 18 - i;
        color_t c = (i + frame / 6) % 2 ? HINT_RING_A : HINT_RING_B;
        draw_rect(x - s / 2 - 3, y - s / 2 - 3, s + 6, s + 6, RGB(20, 60, 160));
        draw_rect(x - s / 2, y - s / 2, s, s, c);
    }
}

static void draw_hint_marks(const Run *g, int frame)
{
    int x = (int)sx(g->b.x), y = (int)sy(g->b.y);
    int half = (int)run_half_w(&g->b) + 16 + (frame / 4) % 4 * 2;
    color_t ring = (frame / 8) % 2 ? HINT_RING_A : HINT_RING_B;
    draw_frame(x - half - 4, y - half - 4, 2 * half + 8, 2 * half + 8, 4, RGB(20, 60, 160));
    draw_frame(x - half, y - half, 2 * half, 2 * half, 6, ring);
    const char *t = "FOLLOW ME";
    int tw = text_width(t, 4), bw = tw + 96, bx = x - bw / 2, by = y - half - 110;
    float px[3] = { (float)x - 16, (float)x + 16, (float)x }, py[3] = { (float)(by + 70), (float)(by + 70), (float)(by + 92) };
    draw_rect(bx - 4, by - 4, bw + 8, 78, RGB(20, 60, 160));
    draw_poly(3, px, py, RGB(20, 60, 160));
    draw_rect(bx, by, bw, 70, COL_BLUE);
    draw_circle(bx + 36, by + 35, 24, COL_WHITE);
    draw_text(bx + 36 - text_width("?", 5) / 2, by + 18, 5, "?", COL_BLUE);
    draw_text(bx + 72, by + 21, 4, t, COL_WHITE);
}

void run_set_deaths(const int (*xy)[2], int n)
{
    deaths = xy;
    ndeaths = xy ? n : 0;
}

static void draw_death_marks(void)
{
    for (int i = 0; i < ndeaths; i++) {
        float x = sx((float)deaths[i][0]), y = sy((float)deaths[i][1]);
        if (x < -20 || x > SCREEN_W + 20 || y < -20 || y > SCREEN_H + 20)
            continue;
        color_t c = i >= ndeaths - 10 ? RGB(255, 70, 70) : RGB(190, 40, 60);
        float a[4] = { x - 18, x - 9, x + 18, x + 9 }, b[4] = { y - 18, y - 18, y + 18, y + 18 };
        float a2[4] = { x + 9, x + 18, x - 9, x - 18 };
        draw_poly(4, a, b, c);
        draw_poly(4, a2, b, c);
    }
}

// Deaths along the level, in 60 slices under the progress bar: the more,
// the hotter.
static void draw_heat(int x, int y, int w, int end_x)
{
    int count[60] = { 0 };
    for (int i = 0; i < ndeaths; i++) {
        int k = end_x > 0 ? deaths[i][0] * 60 / end_x : 0;
        count[k < 0 ? 0 : k > 59 ? 59 : k]++;
    }
    draw_rect(x, y, w, 14, COL_DARK);
    for (int k = 0; k < 60; k++)
        if (count[k])
            draw_rect(x + w * k / 60, y, w / 60 + 1, 14,
                      col_lerp(RGB(150, 20, 40), RGB(255, 230, 60), count[k] * 40 > 256 ? 256 : count[k] * 40));
}

void run_set_rival(const Run *r)
{
    rival = r;
}

void run_screen_pos(float x, float y, int *px, int *py)
{
    *px = (int)sx(x);
    *py = (int)sy(y);
}

// Polygon in the player's own coordinates (pixels from its centre, y down),
// turned by rot and moved to (cx, cy).
static void local_poly(float cx, float cy, float rot, int n, const float *pts, color_t c)
{
    float cs = fcos(rot) * pscale, sn = fsin(rot) * pscale, xs[8], ys[8];
    for (int i = 0; i < n; i++) {
        xs[i] = cx + pts[2 * i] * cs - pts[2 * i + 1] * sn;
        ys[i] = cy + pts[2 * i] * sn + pts[2 * i + 1] * cs;
    }
    draw_poly(n, xs, ys, c);
}

static void local_rect(float cx, float cy, float rot, float x0, float y0, float x1, float y1, color_t c)
{
    float pts[8] = { x0, y0, x1, y0, x1, y1, x0, y1 };
    local_poly(cx, cy, rot, 4, pts, c);
}

static void local_star(float cx, float cy, float rot, float r, color_t c)
{
    float outer[10], inner[10];
    for (int i = 0; i < 5; i++) {
        float a = -1.5707963f + (float)i * 1.2566371f;
        outer[2 * i] = fcos(a) * r;
        outer[2 * i + 1] = fsin(a) * r;
        inner[2 * i] = fcos(a + 0.6283185f) * r * 0.42f;
        inner[2 * i + 1] = fsin(a + 0.6283185f) * r * 0.42f;
    }
    local_poly(cx, cy, rot, 5, inner, c);
    for (int i = 0; i < 5; i++) {
        int j = (i + 4) % 5;
        float tri[6] = { outer[2 * i], outer[2 * i + 1], inner[2 * i], inner[2 * i + 1], inner[2 * j], inner[2 * j + 1] };
        local_poly(cx, cy, rot, 3, tri, c);
    }
}

// --- Vehicle art for the modes (mode.h) -------------------------------------

color_t veh_color(int k)
{
    return k == VC_1 ? look_col(LOOK_COL1) : k == VC_2 ? look_col(LOOK_COL2) : k == VC_WHITE ? COL_WHITE : COL_DARK;
}

int veh_look(int part)
{
    return look.v[part];
}

float veh_scale(void)
{
    return pscale;
}

void veh_set_scale(float s)
{
    pscale = s;
}

void veh_poly(float cx, float cy, float rot, int flip, int n, const float *pts, color_t c)
{
    float m[16];
    for (int i = 0; i < n && i < 8; i++) {
        m[2 * i] = pts[2 * i];
        m[2 * i + 1] = pts[2 * i + 1] * (flip ? -1.0f : 1.0f);
    }
    local_poly(cx, cy, rot, n, m, c);
}

void veh_rect(float cx, float cy, float rot, int flip, float x0, float y0, float x1, float y1, color_t c)
{
    float pts[8] = { x0, y0, x1, y0, x1, y1, x0, y1 };
    veh_poly(cx, cy, rot, flip, 4, pts, c);
}

void veh_square(float cx, float cy, float rot, int flip, float x, float y, float size, color_t c)
{
    float h = size / 2;
    veh_rect(cx, cy, rot, flip, x - h, y - h, x + h, y + h, c);
}

// A circle at a point of the vehicle (round, so only its centre turns).
void veh_circle(float cx, float cy, float rot, int flip, float x, float y, float r, color_t c)
{
    float py = flip ? -y : y, cs = fcos(rot) * pscale, sn = fsin(rot) * pscale;
    draw_circle((int)(cx + x * cs - py * sn), (int)(cy + x * sn + py * cs), (int)(r * pscale), c);
}

float veh_sx(float wx)
{
    return sx(wx);
}

float veh_sy(float wy)
{
    return sy(wy);
}

color_t veh_bg(void)
{
    return cur.bottom;
}

const Run *draw_run(void)
{
    return drawing;
}

// His icon fills the cube's face, one row of same-coloured pixels at a time.
static void draw_icon_face(float cx, float cy, float rot, int k, color_t c2)
{
    const uint8_t *p = icon_pixels(k);
    color_t cols[PX_COLOURS] = { 0, c2, COL_WHITE, RGB(0, 0, 0) };
    if (faded)
        for (int i = 2; i < PX_COLOURS; i++)
            cols[i] = col_lerp(cols[i], cur.bottom, faded);
    float s = 8.0f, o = -s * ICON_N / 2;
    for (int y = 0; y < ICON_N; y++)
        for (int x = 0; x < ICON_N;) {
            int v = p[y * ICON_N + x], x1 = x + 1;
            while (x1 < ICON_N && p[y * ICON_N + x1] == v)
                x1++;
            if (v)
                local_rect(cx, cy, rot, o + (float)x * s, o + (float)y * s, o + (float)x1 * s, o + (float)(y + 1) * s, cols[v]);
            x = x1;
        }
}

// A stock icon fills the face the same way, 16 pixels across.
static void draw_stock_face(float cx, float cy, float rot, int k)
{
    const StockIcon *ic = &STOCK[k];
    color_t cols[9];
    for (int i = 0; i < 8; i++) {
        cols[i + 1] = stock_color(ic->pal[i], look_col(LOOK_COL1), look_col(LOOK_COL2));
        if (faded && ic->pal[i] != STOCK_COL1 && ic->pal[i] != STOCK_COL2)
            cols[i + 1] = col_lerp(cols[i + 1], cur.bottom, faded);
    }
    float s = 4.0f, o = -s * STOCK_PX / 2;
    for (int y = 0; y < STOCK_PX; y++)
        for (int x = 0; x < STOCK_PX;) {
            int v = ic->px[y * STOCK_PX + x] - '0', x1 = x + 1;
            while (x1 < STOCK_PX && ic->px[y * STOCK_PX + x1] - '0' == v)
                x1++;
            if (v)
                local_rect(cx, cy, rot, o + (float)x * s, o + (float)y * s, o + (float)x1 * s, o + (float)(y + 1) * s, cols[v]);
            x = x1;
        }
}

static void draw_face(float cx, float cy, float rot, int face, color_t c2)
{
    color_t w = faded ? col_lerp(COL_WHITE, cur.bottom, faded) : COL_WHITE;
    if (face >= FACE_STOCK0 && face < FACE_STOCK0 + STOCK_COUNT) {
        draw_stock_face(cx, cy, rot, face - FACE_STOCK0);
        return;
    }
    if (FACE_IS_ICON(face)) {
        draw_icon_face(cx, cy, rot, face - FACE_ICON0, c2);
        return;
    }
    switch (face) {
    case 1: // smile
        local_rect(cx, cy, rot, -18, -16, -7, -4, c2);
        local_rect(cx, cy, rot, 7, -16, 18, -4, c2);
        local_rect(cx, cy, rot, -15, 10, 15, 16, c2);
        local_rect(cx, cy, rot, -21, 3, -15, 12, c2);
        local_rect(cx, cy, rot, 15, 3, 21, 12, c2);
        break;
    case 2: // cool
        local_rect(cx, cy, rot, -25, -15, 25, -10, c2);
        local_rect(cx, cy, rot, -23, -15, -4, -2, c2);
        local_rect(cx, cy, rot, 4, -15, 23, -2, c2);
        local_rect(cx, cy, rot, -19, -13, -14, -9, w);
        local_rect(cx, cy, rot, -6, 10, 17, 15, c2);
        break;
    case 3: { // angry
        float le[6] = { -23, -18, -5, -9, -23, -3 }, re[6] = { 23, -18, 5, -9, 23, -3 };
        local_poly(cx, cy, rot, 3, le, c2);
        local_poly(cx, cy, rot, 3, re, c2);
        local_rect(cx, cy, rot, -15, 10, 15, 16, c2);
        break;
    }
    case 4: // robot
        local_rect(cx, cy, rot, -25, -17, 25, -3, c2);
        local_rect(cx, cy, rot, -6, -13, 6, -7, w);
        for (int i = 0; i < 3; i++)
            local_rect(cx, cy, rot, -16 + i * 12, 7, -10 + i * 12, 18, c2);
        break;
    case 5: { // cat
        float nose[6] = { -5, 2, 5, 2, 0, 8 };
        local_rect(cx, cy, rot, -18, -14, -8, -4, c2);
        local_rect(cx, cy, rot, 8, -14, 18, -4, c2);
        local_rect(cx, cy, rot, -15, -12, -12, -6, w);
        local_rect(cx, cy, rot, 11, -12, 14, -6, w);
        local_poly(cx, cy, rot, 3, nose, c2);
        local_rect(cx, cy, rot, -27, 6, -9, 8, c2);
        local_rect(cx, cy, rot, 9, 6, 27, 8, c2);
        local_rect(cx, cy, rot, -27, 12, -9, 14, c2);
        local_rect(cx, cy, rot, 9, 12, 27, 14, c2);
        break;
    }
    case 6: // cyclops
        local_rect(cx, cy, rot, -15, -19, 15, 5, w);
        local_rect(cx, cy, rot, -7, -11, 7, 1, c2);
        local_rect(cx, cy, rot, -12, 12, 12, 17, c2);
        break;
    case 7: // star
        local_star(cx, cy, rot, 24, c2);
        local_star(cx, cy, rot, 9, w);
        break;
    case 9: // wink
        local_rect(cx, cy, rot, -19, -9, -6, -5, c2);
        local_rect(cx, cy, rot, 7, -17, 18, -4, c2);
        local_rect(cx, cy, rot, 10, -14, 14, -10, w);
        local_rect(cx, cy, rot, -15, 10, 15, 16, c2);
        local_rect(cx, cy, rot, 15, 3, 21, 12, c2);
        break;
    case 16: { // ninja: headband with tails, narrow eyes under angry brows
        float tail[8] = { 26, -22, 46, -32, 50, -24, 26, -13 }, lb[8] = { -24, -11, -5, -6, -5, -2, -24, -7 };
        float rb[8] = { 24, -11, 5, -6, 5, -2, 24, -7 };
        local_rect(cx, cy, rot, -30, -26, 30, -15, c2);
        local_poly(cx, cy, rot, 4, tail, c2);
        local_poly(cx, cy, rot, 4, lb, c2);
        local_poly(cx, cy, rot, 4, rb, c2);
        local_rect(cx, cy, rot, -21, 0, -6, 6, w);
        local_rect(cx, cy, rot, 6, 0, 21, 6, w);
        local_rect(cx, cy, rot, -13, 0, -7, 6, c2);
        local_rect(cx, cy, rot, 7, 0, 13, 6, c2);
        local_rect(cx, cy, rot, -20, 14, 20, 24, c2);
        break;
    }
    case 17: { // demon: horns, slanted eyes and a grin with fangs
        float lh[6] = { -30, -24, -12, -24, -34, -52 }, rh[6] = { 30, -24, 12, -24, 34, -52 };
        float le[6] = { -24, -14, -4, -5, -20, 2 }, re[6] = { 24, -14, 4, -5, 20, 2 };
        float lf[6] = { -14, 10, -6, 10, -10, 20 }, rf[6] = { 6, 10, 14, 10, 10, 20 };
        local_poly(cx, cy, rot, 3, lh, c2);
        local_poly(cx, cy, rot, 3, rh, c2);
        local_poly(cx, cy, rot, 3, le, w);
        local_poly(cx, cy, rot, 3, re, w);
        local_rect(cx, cy, rot, -16, -7, -10, -2, c2);
        local_rect(cx, cy, rot, 10, -7, 16, -2, c2);
        local_rect(cx, cy, rot, -22, 6, 22, 11, c2);
        local_poly(cx, cy, rot, 3, lf, w);
        local_poly(cx, cy, rot, 3, rf, w);
        break;
    }
    case 8: { // alien
        float le[8] = { -25, -16, -8, -12, -6, 2, -20, -2 }, re[8] = { 25, -16, 8, -12, 6, 2, 20, -2 };
        local_poly(cx, cy, rot, 4, le, c2);
        local_poly(cx, cy, rot, 4, re, c2);
        local_rect(cx, cy, rot, -16, -11, -12, -6, w);
        local_rect(cx, cy, rot, 12, -11, 16, -6, w);
        local_rect(cx, cy, rot, -6, 12, 6, 15, c2);
        break;
    }
    default: // classic
        draw_rot_square(cx, cy, PLAYER_S * 0.46f * pscale, rot, c2);
        draw_rot_square(cx, cy, PLAYER_S * 0.18f * pscale, rot, w);
        break;
    }
}

int look_face(const Look *l)
{
    return l->stock > 0 && l->stock <= STOCK_COUNT ? FACE_STOCK0 + l->stock - 1 : l->v[LOOK_FACE];
}

void run_draw_cube(float cx, float cy, float rot)
{
    color_t c1 = look_col(LOOK_COL1), c2 = look_col(LOOK_COL2);
    int face = look_face(&look), plain = faded != 0;
    float ps = pscale;
    if (!plain) {
        pscale *= rw_big();
        c1 = rw_body(c1);
    }
    draw_rot_square(cx, cy, (PLAYER_S + 14) * pscale, rot, col_scale(c1, 2, 5));
    draw_rot_square(cx, cy, PLAYER_S * pscale, rot, c1);
    if (!plain)
        rw_cube(cx, cy, PLAYER_S * pscale, rot, c1, c2, 0);
    if (face < FACE_STOCK0 && !FACE_IS_ICON(face))
        draw_rot_square(cx, cy, (PLAYER_S - 16) * pscale, rot, col_scale(c1, 1, 8));
    if (!plain)
        rw_cube(cx, cy, PLAYER_S * pscale, rot, c1, c2, 2);
    draw_face(cx, cy, rot, face, c2);
    if (!plain)
        rw_cube(cx, cy, PLAYER_S * pscale, rot, c1, c2, 1);
    pscale = ps;
}

void run_draw_pilot(float cx, float cy, float scale, float rot)
{
    float ps = pscale;
    pscale = ps * scale;
    run_draw_cube(cx, cy, rot);
    pscale = ps;
}

void run_draw_face(float cx, float cy, float scale, float rot, int face)
{
    float ps = pscale;
    pscale = ps * scale;
    draw_face(cx, cy, rot, face, look_col(LOOK_COL2));
    pscale = ps;
}

void veh_pilot(float cx, float cy, float rot, int flip, float x, float y, float size)
{
    float ps = pscale, py = flip ? -y : y, cs = fcos(rot) * ps, sn = fsin(rot) * ps;
    pscale = ps * size / PLAYER_S;
    run_draw_cube(cx + x * cs - py * sn, cy + x * sn + py * cs, rot);
    pscale = ps;
}

void draw_mode_icon(int mode, float x, float y, float scale, int frame)
{
    Body b;
    memset(&b, 0, sizeof(b));
    b.mode = mode;
    b.grav = 1;
    b.on_ground = 1;
    b.fx_t = 1000;
    float ps = pscale;
    pscale = scale;
    mode_def(mode)->draw(&b, x, y, frame);
    pscale = ps;
}

void look_draw_ship(float cx, float cy, float scale, float rot)
{
    Body b;
    memset(&b, 0, sizeof(b));
    b.mode = MODE_SHIP;
    b.grav = 1;
    b.rot = rot;
    b.fx_t = 1000;
    float ps = pscale;
    pscale = ps * scale;
    mode_def(MODE_SHIP)->draw(&b, cx, cy, 0);
    pscale = ps;
}

static void draw_trail(const float *tx, const float *ty, int frame)
{
    static const color_t RAINBOW[6] = { RGB(255, 60, 60), RGB(255, 160, 0), RGB(255, 240, 0),
                                        RGB(60, 255, 90), RGB(40, 160, 255), RGB(200, 80, 255) };
    color_t c1 = look_col(LOOK_COL1), c2 = look_col(LOOK_COL2);
    int style = look.v[LOOK_TRAIL];
    if (style == 1)
        return;
    for (int i = TRAIL_N - 1; i >= 1; i--) {
        float x = sx(tx[i]), y = sy(ty[i]);
        int fade = 60 + i * 12;
        if (style == 2) {
            float nx = sx(tx[i - 1]), ny = sy(ty[i - 1]);
            for (int k = 0; k < 3; k++) {
                float t = (float)k / 3.0f;
                int sz = 18 - i;
                draw_rect((int)(x + (nx - x) * t) - sz / 2, (int)(y + (ny - y) * t) - sz / 2, sz, sz,
                          col_lerp(c2, cur.bottom, fade));
            }
        } else if (style == 3) {
            int jx = ((frame * 7 + i * 13) % 31) - 15, jy = ((frame * 11 + i * 17) % 31) - 15;
            draw_rect((int)x + jx - 4, (int)y + jy - 4, 8, 8, (i + frame) % 3 ? c2 : COL_WHITE);
        } else if (style == 6) {
            if (i % 3 == 1) {
                int r = 16 - i / 2;
                draw_circle((int)x, (int)y - ((frame + i * 3) % 12), r, col_lerp(c2, cur.bottom, fade / 2));
                draw_circle((int)x, (int)y - ((frame + i * 3) % 12), r - 4, col_lerp(c2, cur.bottom, 200));
            }
        } else if (style == 5) {
            if (i % 3 == 1)
                ui_star(x, y + (float)(((frame + i * 5) % 16) - 8), (float)(18 - i), col_lerp(COL_YELLOW, cur.bottom, fade / 2));
        } else if (style == 7) {
            // Fire: flames that shrink, cool from yellow to red and drift up.
            static const color_t FLAME[4] = { RGB(255, 245, 150), RGB(255, 200, 40), RGB(255, 120, 20), RGB(210, 40, 20) };
            int sz = 28 - i * 3 / 2, jx = ((frame * 7 + i * 5) % 9) - 4, rise = i * 3 + (frame + i) % 5;
            draw_rect((int)x - sz / 2 + jx, (int)y - sz / 2 - rise, sz, sz, col_lerp(FLAME[i * 4 / TRAIL_N], cur.bottom, fade / 3));
            if (i < 6)
                draw_rect((int)x - sz / 4 + jx, (int)y - sz / 4 - rise, sz / 2, sz / 2, COL_WHITE);
        } else if (style == 8) {
            // Comet: a glowing tail that is widest at the player, with sparkles.
            float nx = sx(tx[i - 1]), ny = sy(ty[i - 1]);
            float w0 = (float)(TRAIL_N - i) * 1.7f + 2.0f, w1 = w0 + 1.7f;
            float xs[4] = { x, nx, nx, x }, ys[4] = { y - w0, ny - w1, ny + w1, y + w0 };
            float ys2[4] = { y - w0 / 2, ny - w1 / 2, ny + w1 / 2, y + w0 / 2 };
            draw_poly(4, xs, ys, col_lerp(c2, cur.bottom, fade / 2));
            draw_poly(4, xs, ys2, col_lerp(COL_WHITE, c2, i * 256 / TRAIL_N));
            if ((i + frame / 3) % 4 == 0)
                ui_star(x, y + (float)(((frame * 3 + i * 7) % 24) - 12), 8.0f, COL_WHITE);
        } else if (i % 2 == 0) {
            int sz = 30 - i * 2;
            color_t c = style == 4 ? RAINBOW[(i / 2 + frame / 4) % 6] : c1;
            draw_rect((int)x - sz / 2, (int)y - sz / 2, sz, sz, col_lerp(c, cur.bottom, style == 4 ? fade / 2 : fade));
        }
    }
}

// Point k of n spread along a mode's outline (pixels from its centre).
static void outline_point(const ModeDef *d, int k, int n, int flip, float *x, float *y)
{
    float u = (float)k * (float)d->noutline / (float)n;
    int e = (int)u, f = (e + 1) % d->noutline;
    float t = u - (float)e;
    *x = d->outline[2 * e] + (d->outline[2 * f] - d->outline[2 * e]) * t;
    *y = d->outline[2 * e + 1] + (d->outline[2 * f + 1] - d->outline[2 * e + 1]) * t;
    if (flip)
        *y = -*y;
}

// A portal's morph (drawing only): a flash, the old form's pixels flying
// out along an arc to the new form's shape, and the new form growing in.
static void draw_morph(const Body *b, float x, float y, int frame)
{
    const ModeDef *from = mode_def(b->morph_from), *to = mode_def(b->mode);
    int t = b->morph_t, n = 24;
    float s = (float)t / MORPH_FRAMES, e = s * s * (3 - 2 * s), arc = fsin(s * PI_F) * 26.0f;
    color_t c1 = look_col(LOOK_COL1), c2 = look_col(LOOK_COL2);
    if (t < 4)
        draw_circle((int)x, (int)y, (int)((70 - 14 * t) * pscale), faded ? col_lerp(COL_WHITE, cur.bottom, faded) : COL_WHITE);
    if (t >= MORPH_FRAMES - 4) {
        float ps = pscale;
        pscale = ps * (0.55f + 0.45f * (float)(t - (MORPH_FRAMES - 4)) / 4.0f);
        to->draw(b, x, y, frame);
        pscale = ps;
    }
    for (int k = 0; k < n; k++) {
        float ax, ay, bx, by;
        outline_point(from, k, n, b->grav < 0, &ax, &ay);
        outline_point(to, k, n, b->grav < 0, &bx, &by);
        float px = ax + (bx - ax) * e, py = ay + (by - ay) * e;
        float len = (px < 0 ? -px : px) + (py < 0 ? -py : py) + 1;
        px += px / len * arc;
        py += py / len * arc;
        int sz = (int)((14 - t) * pscale) + 3;
        color_t c = k % 3 == 2 ? COL_WHITE : k % 3 ? c2 : c1;
        draw_rect((int)(x + px * pscale) - sz / 2, (int)(y + py * pscale) - sz / 2, sz, sz,
                  faded ? col_lerp(c, cur.bottom, faded) : c);
    }
}

static void draw_player(const Run *r)
{
    const Body *b = &r->b;
    const ModeDef *d = mode_def(b->mode);
    pscale = b->mini ? 0.5f : 1.0f;
    if (d->trail)
        d->trail(r->trail_x, r->trail_y);
    else if (faded || !rw_trail(r->trail_x, r->trail_y, look_col(LOOK_COL1), look_col(LOOK_COL2), cur.bottom))
        draw_trail(r->trail_x, r->trail_y, r->frame);
    if (b->morph_t < MORPH_FRAMES)
        draw_morph(b, sx(b->x), sy(b->y), r->frame);
    else
        d->draw(b, sx(b->x), sy(b->y), r->frame);
    pscale = 1.0f;
}

void look_draw_preview(int x, int y, int ship_y, int frame)
{
    float tx[TRAIL_N], ty[TRAIL_N];
    cam_x = 0;
    cam_y = 0;
    cur = LEVEL_PALETTES[0];
    for (int i = 0; i < TRAIL_N; i++) {
        int t = (frame - i) % 40;
        if (t < 0)
            t += 40;
        tx[i] = (float)(x - i * 14);
        ty[i] = (float)(GROUND_Y - y) + (float)(t * (40 - t)) * 0.4f;
    }
    int t = frame % 40;
    if (!rw_trail(tx, ty, look_col(LOOK_COL1), look_col(LOOK_COL2), cur.bottom))
        draw_trail(tx, ty, frame);
    run_draw_cube((float)x, sy(ty[0]), (float)t * (3.14159265f / 40.0f));
    rw_pet(tx, ty, 0);
    Body b;
    memset(&b, 0, sizeof(b));
    b.mode = MODE_SHIP;
    b.grav = 1;
    b.rot = fsin((float)frame * 0.08f + 1.2f) * 0.3f;
    MODES[MODE_SHIP]->draw(&b, (float)x, (float)ship_y + fsin((float)frame * 0.08f) * 18.0f, frame);
}

void fx_draw(const Particle *parts, float cx, float cy, color_t bg)
{
    for (int i = 0; i < MAX_PARTS; i++) {
        const Particle *p = &parts[i];
        if (p->life <= 0)
            continue;
        int fade = 256 - p->life * 256 / p->max_life;
        color_t c = col_lerp(p->color, bg, fade);
        int x = (int)(p->x - cx), y = (int)((float)GROUND_Y - (p->y - cy));
        draw_rect(x - p->size / 2, y - p->size / 2, p->size, p->size, c);
    }
}

static void draw_hud(const Run *r, int hud)
{
    char buf[32];
    if (hud & HUD_PROGRESS) {
        int w = 760, x = (SCREEN_W - w) / 2, y = 34;
        draw_rect(x - 5, y - 5, w + 10, 32, col_scale(cur.line, 2, 3));
        draw_rect(x, y, w, 22, COL_DARK);
        draw_rect(x, y, w * r->pct / 100, 22, r->practice ? COL_CUBE : COL_CUBE_CORE);
        if (ndeaths && !r->race)
            draw_heat(x, y + 30, w, r->lv->end_x);
        snprintf(buf, sizeof(buf), "%d%%", r->pct);
        draw_text_glow(x + w + 26, y - 4, 5, buf, COL_WHITE, COL_CUBE_CORE);
    }
    if ((hud & HUD_TEXT) && r->lv->ncoins) {
        int n = r->lv->ncoins;
        for (int k = 0; k < n; k++) {
            float x = (float)(SCREEN_W - 60 - (n - 1 - k) * 58), y = 50;
            if (r->b.coins & (1 << k)) {
                draw_coin_at(x, y, 22, 1, 0);
            } else {
                color_t rim, face;
                coin_colors((r->coins_had >> k) & 1, &rim, &face);
                draw_oval8(x, y, 22, 22, (r->coins_had >> k) & 1 ? face : rim);
                draw_oval8(x, y, 15, 15, COL_DARK);
            }
        }
    }
    if (r->practice && (hud & HUD_TEXT)) {
        draw_text_glow(40, 28, 5, "PRACTICE", RGB(200, 255, 170), RGB(30, 140, 10));
        draw_text(40, SCREEN_H - 44, 3, "SQUARE: CHECKPOINT   TRIANGLE: REMOVE IT   TOUCHPAD: SHOW ME HOW",
                  RGB(200, 230, 200));
    }
}

static int hsh(int k, int n)
{
    uint32_t h = (uint32_t)k * 2654435761u;
    h ^= h >> 13;
    h *= 2246822519u;
    h ^= h >> 16;
    return (int)(h % (uint32_t)n);
}

// A theme's own backdrop decoration (THEME MORPH), in screen space behind the grid.
static void draw_theme_deco(int theme, LevelColors c, float cx, int frame)
{
    int span = SCREEN_W + 400 + 2 * (int)view_mx, left = -200 - (int)view_mx;
    switch (theme) {
    case 1: // day: a sun with turning rays, clouds drifting by
        for (int k = 0; k < 8; k++)
            local_rect(1560, 190, (float)k * 0.785f + (float)frame * 0.01f, 80, -8, 120, 8, RGB(255, 230, 90));
        draw_circle(1560, 190, 70, RGB(255, 245, 150));
        draw_circle(1560, 190, 58, RGB(255, 220, 60));
        for (int k = 0; k < 5; k++) {
            int x = left + wrap(k * 530 - (int)(cx * 0.15f), span), y = 110 + hsh(k, 220);
            draw_circle(x, y, 40, RGB(255, 255, 255));
            draw_circle(x + 50, y - 18, 55, RGB(255, 255, 255));
            draw_circle(x + 105, y, 40, RGB(255, 255, 255));
            draw_rect(x, y, 105, 40, RGB(255, 255, 255));
        }
        break;
    case 2: // night: a crescent moon and twinkling stars
    case 3: // space: more stars and a ringed planet
        for (int k = 0; k < (theme == 3 ? 80 : 50); k++) {
            int x = left + wrap(hsh(k, span) - (int)(cx * 0.05f), span), y = hsh(k + 99, 620);
            int sz = (frame / 9 + k) % 11 == 0 ? 7 : k % 3 ? 3 : 5;
            draw_rect(x - sz / 2, y - sz / 2, sz, sz, theme == 3 && k % 4 == 0 ? RGB(255, 180, 255) : RGB(255, 255, 230));
        }
        if (theme == 2) {
            draw_circle(1540, 190, 62, RGB(245, 245, 215));
            draw_circle(1568, 172, 54, c.top);
        } else {
            draw_oval8(1420, 250, 170, 26, RGB(255, 170, 60));
            draw_circle(1420, 250, 84, RGB(150, 60, 220));
            draw_circle(1395, 225, 30, RGB(180, 100, 240));
            draw_rect(1330, 262, 180, 10, RGB(255, 170, 60));
        }
        break;
    case 4: // snow: flakes falling
        for (int k = 0; k < 70; k++) {
            int x = left + wrap(hsh(k, span) - (int)(cx * 0.3f) + (int)(fsin((float)(frame + k * 20) * 0.03f) * 30), span);
            int y = (hsh(k + 7, SCREEN_H) + frame * (2 + k % 3)) % (SCREEN_H + 40) - 20, sz = 4 + k % 3 * 2;
            draw_rect(x - sz / 2, y - sz / 2, sz, sz, RGB(255, 255, 255));
        }
        break;
    case 5: // lava: embers rising
        for (int k = 0; k < 45; k++) {
            int x = left + wrap(hsh(k, span) - (int)(cx * 0.2f), span);
            int y = SCREEN_H - (hsh(k + 3, SCREEN_H) + frame * (3 + k % 4)) % (SCREEN_H + 40), sz = 5 + k % 4 * 2;
            draw_rect(x, y, sz, sz, k % 3 ? RGB(255, 140, 20) : RGB(255, 230, 80));
        }
        break;
    case 6: // ocean: light rays and bubbles rising
        for (int k = 0; k < 4; k++) {
            float x = (float)(wrap(k * 520 - (int)(cx * 0.1f), SCREEN_W + 400) - 200);
            float xs[4] = { x, x + 90, x + 330, x + 200 }, ys[4] = { -10, -10, (float)SCREEN_H, (float)SCREEN_H };
            draw_poly(4, xs, ys, col_lerp(c.top, c.line, 26));
        }
        for (int k = 0; k < 30; k++) {
            int x = left + wrap(hsh(k, span) - (int)(cx * 0.3f), span), r = 6 + k % 4 * 3;
            int y = SCREEN_H - (hsh(k + 5, SCREEN_H) + frame * (1 + k % 3)) % (SCREEN_H + 40);
            draw_circle(x, y, r, col_lerp(c.bottom, c.line, 140));
            draw_circle(x, y, r - 3, col_lerp(c.top, c.bottom, 200));
        }
        break;
    case 7: { // candy: pastel dots floating by
        static const color_t PASTEL[4] = { RGB(255, 255, 255), RGB(255, 190, 120), RGB(160, 230, 255), RGB(200, 160, 255) };
        for (int k = 0; k < 28; k++) {
            int x = left + wrap(hsh(k, span) - (int)(cx * 0.12f), span), y = hsh(k + 11, 640);
            draw_circle(x, y + (int)(fsin((float)(frame + k * 13) * 0.05f) * 10), 10 + k % 4 * 5, PASTEL[k % 4]);
        }
        break;
    }
    }
}

// The level transformations' view for this frame: zoom about the player,
// then mirror and flip about the screen's centre (squashing through flat as
// they turn), and the colour effects.
static void set_view(const Run *r)
{
    const Body *b = &r->b;
    const Transform *f = &b->tf;
    float z = transform_zoom(b) * rw_zoom(), m = (float)f->mirror_t / TF_EASE, fl = (float)f->flip_t / TF_EASE;
    m = m * m * (3 - 2 * m);
    fl = fl * fl * (3 - 2 * fl);
    float px = sx(b->x), py = sy(b->y), am = 1 - 2 * m, af = 1 - 2 * fl;
    float ax = z * am, ay = z * af;
    draw_set_view(ax, SCREEN_W / 2.0f + (px * (1 - z) - SCREEN_W / 2.0f) * am, ay,
                  SCREEN_H / 2.0f + (py * (1 - z) - SCREEN_H / 2.0f) * af);
    float kx = ax < 0 ? -ax : ax, ky = ay < 0 ? -ay : ay;
    kx = kx < 0.25f ? 0.25f : kx;
    ky = ky < 0.25f ? 0.25f : ky;
    view_mx = kx < 1 ? (float)SCREEN_W * (1 / kx - 1) : 0;
    view_my = ky < 1 ? (float)SCREEN_H * (1 / ky - 1) : 0;
    float rb = (float)f->rainbow_t / TF_EASE;
    draw_set_tint(rb * (float)(r->frame % 120) * (6.2831853f / 120) + rw_hue(run_beat_at(r, r->song_t)),
                  f->invert_t * 256 / TF_EASE);
}

// Backdrop, corridors and objects in colours c with theme decoration t.
static void draw_world(const Run *r, LevelColors c, int t, int shx, int shy)
{
    cur = c;
    deco_theme = t;
    draw_set_offset(0, 0);
    run_draw_backdrop(cur, cam_x, cam_y);
    deco_theme = 0;
    draw_set_offset(shx, shy);
    draw_bounds(r);
    draw_objects(r->lv, r->frame);
}

void run_draw(const Run *r, int hud)
{
    const Body *b = &r->b;
    const Transform *f = &b->tf;
    LevelColors now = run_colors(r);
    int shx = 0, shy = 0;
    if (r->shake > 0) {
        shx = (r->frame * 7) % 17 - 8;
        shy = (r->frame * 11) % 13 - 6;
    }
    if (f->shake_t > 0) {
        shx += ((r->frame * 13) % 9 - 4) * f->shake_n / 4;
        shy += ((r->frame * 7) % 7 - 3) * f->shake_n / 3;
    }
    cam_x = b->x - PLAYER_S / 2.0f - PLAYER_X;
    cam_y = b->cam_y;
    moved = b;
    drawing = r;
    beat = run_beat(r);
    coins_had = r->coins_had;
    coins_silver = r->coins_silver;
    rw_scene(RW_PLAYER);
    draw_set_filter(rw_filter());
    set_view(r);

    // A theme sweeps in from the right: the old world left of the edge.
    if (f->theme_t < SWEEP_FRAMES && f->theme_from != f->theme_to) {
        float s = (float)f->theme_t / SWEEP_FRAMES;
        int edge = SCREEN_W - (int)((float)SCREEN_W * s * s * (3 - 2 * s));
        draw_set_clip(0, edge);
        draw_world(r, f->sweep_from, f->theme_from, shx, shy);
        draw_set_clip(edge, SCREEN_W);
        draw_world(r, now, f->theme_to, shx, shy);
        draw_set_clip(0, SCREEN_W);
        draw_set_view(1, 0, 1, 0);
        draw_rect(edge - 6, 0, 12, SCREEN_H, RGB(255, 255, 255));
        draw_rect(edge - 16, 0, 10, SCREEN_H, now.line);
        set_view(r);
    } else {
        draw_world(r, now, f->theme_to, shx, shy);
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "ATTEMPT %d", r->attempt);
    float ax = sx(3.0f * TILE);
    if ((hud & HUD_TEXT) && ax > -1200 && ax < SCREEN_W)
        draw_text_glow((int)ax, (int)sy(5.5f * TILE), 9, buf, COL_WHITE, col_scale(cur.line, 2, 3));

    if (r->practice && !r->race)
        draw_death_marks();
    if (r->practice && !r->race)
        for (int i = 0; i < r->ncp; i++) {
            float x = sx(r->cps[i].x), y = sy(r->cps[i].y);
            if (x < -40 || x > SCREEN_W + 40)
                continue;
            draw_rot_square(x, y, 44, PI_F / 4, col_scale(COL_CUBE, 1, 2));
            draw_rot_square(x, y, 32, PI_F / 4, COL_CUBE);
            draw_rot_square(x, y, 14, PI_F / 4, COL_WHITE);
        }

    if (ghost && ghost->state == RUN_PLAY) {
        if (ghost_hint)
            draw_hint_path(r->frame);
        faded = ghost_hint ? 80 : 150;
        rw_scene(RW_MUTED);
        draw_player(ghost);
        rw_scene(RW_PLAYER);
        faded = 0;
    }
    if (rival) {
        Look mine = look;
        look = rival->look;
        rw_scene(RW_MUTED);
        if (rival->state != RUN_DEAD)
            draw_player(rival);
        fx_draw(rival->parts, cam_x, cam_y, cur.bottom);
        rw_scene(RW_PLAYER);
        look = mine;
    }
    if (r->state != RUN_DEAD) {
        draw_player(r);
        rw_pet(r->trail_x, r->trail_y, r->b.mini);
    } else {
        rw_dead(r);
    }
    fx_draw(r->parts, cam_x, cam_y, cur.bottom);
    if (ghost && ghost_hint && ghost->state == RUN_PLAY)
        draw_hint_marks(ghost, r->frame);
    rw_overlay(r, beat);
    draw_set_offset(0, 0);
    draw_set_view(1, 0, 1, 0);
    draw_set_tint(0, 0);
    view_mx = view_my = 0;
    draw_hud(r, hud);
    draw_set_filter(0);
    rw_scene(RW_MENU);
    drawing = NULL;
}
