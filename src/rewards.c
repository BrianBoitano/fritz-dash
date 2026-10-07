// Fritz Dash - what the easter egg rewards draw: backgrounds, trails, cube
// finishes, party hat and googly eyes, pets, crash effects, screen filters,
// party effects and title variants. All of it is drawing (the run's
// particles at most): nothing here is read by physics or the solvers. Each
// effect is a few dozen primitives, so a frame costs about what a plain one
// does on the PS4's software renderer.
#include <stdio.h>
#include <string.h>

#include "eggs.h"
#include "mode.h"
#include "stock.h"

#define WHITE RGB(255, 255, 255)

static int frame;
static int scene;           // RW_ (rewards.h): menus, his own run, or muted
static struct {
    float x, y, vx, vy;
    int life;
    color_t c;
} conf[48];

void rewards_frame(int f) { frame = f; }
void rw_scene(int s) { scene = s; }

static int on(void) { return scene != RW_MUTED; }
static int worn(int kind) { return on() ? reward_worn(kind) : 0; }
static int party(int bit) { return on() && (reward_party() & bit); }

static uint32_t hs(uint32_t k)
{
    k *= 2654435761u;
    k ^= k >> 15;
    k *= 2246822519u;
    k ^= k >> 13;
    return k;
}

// A colour round the rainbow, deg 0..359.
static color_t hue(int deg)
{
    int d = ((deg % 360) + 360) % 360, t = (d % 60) * 255 / 60;
    switch (d / 60) {
    case 0: return RGB(255, t, 40);
    case 1: return RGB(255 - t, 255, 40);
    case 2: return RGB(40, 255, t);
    case 3: return RGB(40, 255 - t, 255);
    case 4: return RGB(t, 40, 255);
    default: return RGB(255, 40, 255 - t);
    }
}

static int wrapi(int v, int m)
{
    return ((v % m) + m) % m;
}

// 1 / sqrt(v) without libm.
static float inv_sqrt(float v)
{
    union {
        float f;
        uint32_t i;
    } u = { v };
    u.i = 0x5f3759dfu - (u.i >> 1);
    u.f *= 1.5f - 0.5f * v * u.f * u.f;
    u.f *= 1.5f - 0.5f * v * u.f * u.f;
    return u.f;
}

// A line w pixels thick from (x0, y0) to (x1, y1).
static void quad(float x0, float y0, float x1, float y1, float w, color_t c)
{
    float dx = x1 - x0, dy = y1 - y0, l = dx * dx + dy * dy;
    if (l < 0.01f)
        return;
    float k = inv_sqrt(l) * w / 2, nx = -dy * k, ny = dx * k;
    float xs[4] = { x0 + nx, x1 + nx, x1 - nx, x0 - nx }, ys[4] = { y0 + ny, y1 + ny, y1 - ny, y0 - ny };
    draw_poly(4, xs, ys, c);
}

// --- Backgrounds -------------------------------------------------------------------

static void bg_warp(color_t bg, float cx)
{
    (void)cx;
    for (int i = 0; i < 64; i++) {
        uint32_t h = hs((uint32_t)i + 7);
        float a = (float)(h % 6283) / 1000.0f, d = (float)((frame * (6 + (int)(h >> 28)) + (int)(h >> 8)) % 1000) / 1000.0f;
        float r0 = d * d * 1300 + 20, r1 = r0 + 6 + d * 60;
        float x0 = SCREEN_W / 2 + fcos(a) * r0, y0 = SCREEN_H * 0.42f + fsin(a) * r0 * 0.7f;
        float x1 = SCREEN_W / 2 + fcos(a) * r1, y1 = SCREEN_H * 0.42f + fsin(a) * r1 * 0.7f;
        quad(x0, y0, x1, y1, 3 + d * 8, col_lerp(bg, i % 5 ? WHITE : RGB(120, 220, 255), 150 + (int)(d * 106)));
    }
}

static void bg_aurora(color_t bg, float cx)
{
    static const color_t BANDS[3] = { RGB(60, 255, 160), RGB(40, 200, 255), RGB(190, 90, 255) };
    for (int b = 0; b < 3; b++)
        for (int k = 0; k < 42; k++) {
            float x = (float)(k * 48) - (float)wrapi((int)(cx * 0.05f), 48);
            float t = (float)frame * 0.02f + (float)b * 1.7f, w = x * 0.004f;
            float y = 170 + (float)b * 90 + fsin(w + t) * 60 + fsin(w * 2.3f - t * 0.7f) * 25;
            int h = 70 + (int)(fsin(w * 1.7f + t) * 30.0f);
            draw_rect((int)x, (int)y, 49, h, col_lerp(bg, BANDS[b], 90));
            draw_rect((int)x, (int)y + h / 3, 49, h / 3, col_lerp(bg, BANDS[b], 160));
        }
}

static void bg_bubbles(color_t bg, float cx)
{
    for (int i = 0; i < 30; i++) {
        uint32_t h = hs((uint32_t)i + 31);
        int r = 12 + (int)(h % 26), sp = 1 + (int)(h >> 29);
        int x = wrapi((int)(h >> 8) % 2100 - (int)(cx * 0.3f), 2100) - 90;
        int y = SCREEN_H + 60 - wrapi(frame * sp + (int)(h >> 16), SCREEN_H + 160);
        x += (int)(fsin((float)(frame + i * 40) * 0.03f) * 12.0f);
        draw_circle(x, y, r + 4, col_lerp(bg, RGB(200, 250, 255), 190));
        draw_circle(x, y, r, col_lerp(bg, RGB(40, 120, 200), 90));
        draw_rect(x - r / 2, y - r / 2, r / 3 + 2, r / 3 + 2, WHITE);
    }
}

static void bg_code(color_t bg, float cx)
{
    static const char *const G = "0110100111010011";
    for (int c = 0; c < 18; c++) {
        uint32_t h = hs((uint32_t)c + 77);
        int x = wrapi(c * 110 + 40 - (int)(cx * 0.2f), 18 * 110) - 30, sp = 3 + (int)(h % 4);
        int head = wrapi(frame * sp + (int)(h >> 8), SCREEN_H + 400);
        for (int k = 0; k < 9; k++) {
            int y = head - k * 34;
            if (y < -30 || y > SCREEN_H)
                continue;
            char s[2] = { G[(k + c + frame / 9) % 16], 0 };
            draw_text(x, y, 4, s, col_lerp(bg, k ? RGB(40, 255, 90) : RGB(220, 255, 220), 230 - k * 22));
        }
    }
}

static void bg_snow(color_t bg, float cx)
{
    for (int i = 0; i < 70; i++) {
        uint32_t h = hs((uint32_t)i + 5);
        int s = 8 + (int)(h % 10), sp = 1 + (int)(h >> 30);
        float y = (float)wrapi(frame * sp + (int)(h >> 10), SCREEN_H + 40) - 20;
        float x = (float)wrapi((int)(h >> 4) % 2000 - (int)(cx * (0.1f + (float)sp * 0.1f)), 2000) - 40 +
                  fsin(y * 0.02f + (float)i) * 20;
        color_t c = col_lerp(bg, WHITE, 170 + s * 5);
        draw_rect((int)x - s / 2, (int)y - 2, s, 4, c);
        draw_rect((int)x - 2, (int)y - s / 2, 4, s, c);
        draw_rot_square((float)x, y, (float)s * 0.45f, 0.785f, c);
    }
}

static void bg_fireworks(color_t bg, float cx)
{
    for (int b = 0; b < 5; b++) {
        int period = 110 + b * 17, t = (frame + b * 41) % period, n = (frame + b * 41) / period;
        uint32_t h = hs((uint32_t)(n * 5 + b));
        float x = (float)((int)(h % 1700) + 110) - (float)wrapi((int)(cx * 0.05f), 40), y = (float)(120 + (int)(h >> 20) % 360);
        color_t c = hue((int)(h >> 8) % 360);
        if (t < 24) {
            float ry = y + (float)(24 - t) * 18;
            draw_rect((int)x - 2, (int)ry, 5, 18, col_lerp(bg, RGB(255, 230, 160), 200));
            continue;
        }
        float k = (float)(t - 24), fade = k / (float)(period - 24);
        for (int i = 0; i < 16; i++) {
            float a = (float)i * 0.3927f, d = k * (5.0f - k * 0.025f);
            int px = (int)(x + fcos(a) * d), py = (int)(y + fsin(a) * d + k * k * 0.02f);
            draw_rect(px - 5, py - 5, 11, 11, col_lerp(i % 3 ? c : WHITE, bg, (int)(fade * 200)));
        }
    }
}

static void bg_retro(color_t bg, float cx)
{
    int hz = 600, sun = 470;
    // The sun: bands that thin out toward its bottom, cut by the horizon.
    for (int k = 0; k < 8; k++) {
        int y0 = sun - 150 + k * 28, h = 24 - (k > 3 ? (k - 3) * 4 : 0);
        float dy = (float)(y0 + h / 2 - sun) / 150.0f;
        float w2 = 1 - dy * dy, half = w2 > 0.01f ? 150.0f * w2 * inv_sqrt(w2) : 0;
        if (y0 + h > hz || h <= 0)
            break;
        draw_rect(SCREEN_W / 2 - (int)half, y0, (int)half * 2, h, col_lerp(RGB(255, 220, 60), RGB(255, 40, 160), k * 32));
    }
    color_t line = col_lerp(bg, RGB(255, 60, 220), 200);
    draw_rect(SCREEN_W / 2 - 170, hz - 6, 340, 6, col_lerp(bg, RGB(255, 120, 120), 160));
    draw_rect(0, hz, SCREEN_W, 4, line);
    for (int k = 0; k < 8; k++) {
        float d = (float)wrapi(frame * 2 + k * 45, 360) / 360.0f;
        draw_rect(0, hz + (int)(d * d * 480), SCREEN_W, 2 + (int)(d * 4), line);
    }
    for (int k = -6; k <= 6; k++) {
        float off = (float)wrapi((int)(cx * 0.3f), 220);
        float xb = SCREEN_W / 2 + (float)k * 220 - off, xt = SCREEN_W / 2 + ((float)k * 220 - off) * 0.12f;
        quad(xt, (float)hz, xb, (float)SCREEN_H, 3, line);
    }
}

static void cloud(float x, float y, float s, color_t c)
{
    draw_circle((int)x, (int)y, (int)(40 * s), c);
    draw_circle((int)(x + 46 * s), (int)(y - 18 * s), (int)(52 * s), c);
    draw_circle((int)(x + 98 * s), (int)y, (int)(42 * s), c);
    draw_rect((int)x, (int)y, (int)(98 * s), (int)(40 * s), c);
}

static void bg_clouds(color_t bg, float cx)
{
    static const color_t CANDY[3] = { RGB(255, 170, 220), RGB(255, 240, 250), RGB(190, 170, 255) };
    for (int i = 0; i < 9; i++) {
        uint32_t h = hs((uint32_t)i + 91);
        float s = 0.6f + (float)(h % 50) / 60.0f;
        float x = (float)wrapi((int)(h >> 8) % 2400 - (int)(cx * 0.15f * s) - frame / 3, 2400) - 240;
        float y = (float)(90 + (int)(h >> 20) % 420);
        cloud(x, y, s, col_lerp(bg, CANDY[i % 3], 140 + (int)(s * 60)));
    }
}

static void bg_planets(color_t bg, float cx)
{
    for (int i = 0; i < 40; i++) {
        uint32_t h = hs((uint32_t)i + 300);
        int x = wrapi((int)(h % 2000) - (int)(cx * 0.03f), 2000) - 40, y = (int)(h >> 12) % 700;
        int tw = (frame / 8 + i) % 9 == 0;
        draw_rect(x, y, 3 + tw * 2, 3 + tw * 2, col_lerp(bg, WHITE, 140 + tw * 100));
    }
    static const struct { int x, y, r; color_t c, ring; } P[3] = {
        { 420, 260, 110, RGB(240, 140, 60), RGB(255, 220, 150) },
        { 1300, 160, 60, RGB(90, 160, 255), 0 },
        { 1650, 470, 80, RGB(200, 90, 220), RGB(255, 170, 255) },
    };
    for (int i = 0; i < 3; i++) {
        float x = (float)P[i].x - (float)wrapi((int)(cx * 0.04f * (float)(i + 1)), 2200) + (P[i].x < 900 ? 0 : 0);
        if (x < -300)
            x += 2200;
        float y = (float)P[i].y + fsin((float)frame * 0.01f + (float)i) * 10;
        if (P[i].ring) {
            float xs[6] = { x - P[i].r * 1.9f, x - P[i].r * 0.6f, x + P[i].r * 1.9f, x + P[i].r * 1.9f, x + P[i].r * 0.6f, x - P[i].r * 1.9f };
            float ys[6] = { y + 8, y - 12, y - 18, y - 6, y + 14, y + 20 };
            draw_poly(6, xs, ys, col_lerp(bg, P[i].ring, 160));
        }
        draw_circle((int)x, (int)y, P[i].r, col_lerp(bg, P[i].c, 200));
        draw_circle((int)x - P[i].r / 4, (int)y - P[i].r / 4, P[i].r / 2, col_lerp(bg, col_lerp(P[i].c, WHITE, 70), 200));
        if (P[i].ring) {
            float xs[4] = { x - P[i].r * 1.9f, x + P[i].r * 1.9f, x + P[i].r * 1.9f, x - P[i].r * 1.9f };
            float ys[4] = { y + 8, y - 6, y + 2, y + 18 };
            draw_poly(4, xs, ys, col_lerp(bg, P[i].ring, 200));
        }
    }
}

void rw_backdrop(LevelColors c, float cx, float cy)
{
    (void)cy;
    color_t bg = col_lerp(c.top, c.bottom, 128);
    switch (worn(RK_BG)) {
    case BGX_WARP: bg_warp(bg, cx); break;
    case BGX_AURORA: bg_aurora(bg, cx); break;
    case BGX_BUBBLES: bg_bubbles(bg, cx); break;
    case BGX_CODE: bg_code(bg, cx); break;
    case BGX_SNOW: bg_snow(bg, cx); break;
    case BGX_FIREWORKS: bg_fireworks(bg, cx); break;
    case BGX_RETRO: bg_retro(bg, cx); break;
    case BGX_CLOUDS: bg_clouds(bg, cx); break;
    case BGX_PLANETS: bg_planets(bg, cx); break;
    }
}

int rw_floor(int gy, int x0, int x1, float cx)
{
    if (!party(PA_FLOOR))
        return 0;
    for (int x = x0 - wrapi((int)cx, 120); x < x1; x += 60)
        draw_rect(x, gy - 6, 60, 16, hue((x + (int)cx) / 60 * 40 + frame * 4));
    return 1;
}

// --- Trails -------------------------------------------------------------------------

static void heart(float x, float y, float s, color_t c)
{
    draw_circle((int)(x - s * 0.5f), (int)(y - s * 0.25f), (int)(s * 0.55f), c);
    draw_circle((int)(x + s * 0.5f), (int)(y - s * 0.25f), (int)(s * 0.55f), c);
    float xs[3] = { x - s * 1.05f, x + s * 1.05f, x }, ys[3] = { y - s * 0.05f, y - s * 0.05f, y + s * 1.1f };
    draw_poly(3, xs, ys, c);
}

static void note(float x, float y, float s, color_t c)
{
    draw_circle((int)x, (int)y, (int)(s * 0.45f), c);
    draw_rect((int)(x + s * 0.3f), (int)(y - s * 1.4f), (int)(s * 0.16f) + 2, (int)(s * 1.4f), c);
    draw_rect((int)(x + s * 0.3f), (int)(y - s * 1.4f), (int)(s * 0.6f), (int)(s * 0.25f) + 2, c);
}

static void flake(float x, float y, float s, color_t c)
{
    draw_rect((int)(x - s), (int)y - 1, (int)(s * 2), 3, c);
    draw_rect((int)x - 1, (int)(y - s), 3, (int)(s * 2), c);
    draw_rot_square(x, y, s * 0.9f, 0.785f, c);
    draw_rot_square(x, y, s * 0.5f, 0.785f, RGB(200, 230, 255));
}

int rw_trail(const float *tx, const float *ty, color_t c1, color_t c2, color_t bg)
{
    int style = worn(RK_TRAIL);
    if (!style)
        return 0;
    for (int i = TRAIL_N - 1; i >= 1; i--) {
        float x = veh_sx(tx[i]), y = veh_sy(ty[i]), nx = veh_sx(tx[i - 1]), ny = veh_sy(ty[i - 1]);
        int fade = 40 + i * 14;
        float s = veh_scale();
        switch (style) {
        case TRX_RAINBOW:
            for (int b = 0; b < 6; b++) {
                float o = ((float)b - 2.5f) * 9 * s;
                float xs[4] = { x, nx, nx, x }, ys[4] = { y + o - 5 * s, ny + o - 5 * s, ny + o + 5 * s, y + o + 5 * s };
                draw_poly(4, xs, ys, col_lerp(hue(b * 60), bg, fade / 3));
            }
            break;
        case TRX_HEARTS:
            if (i % 2 == 1)
                heart(x, y - (float)((frame + i * 4) % 14), (26 - (float)i) * s, col_lerp(i % 4 == 1 ? RGB(255, 60, 130) : RGB(255, 150, 200), bg, fade / 3));
            break;
        case TRX_NOTES:
            if (i % 3 == 2)
                note(x, y + fsin((float)(frame + i * 9) * 0.2f) * 14, (40 - (float)i * 1.3f) * s, col_lerp(hue(i * 50 + frame), bg, fade / 3));
            break;
        case TRX_LIGHTNING: {
            int j = (int)(hs((uint32_t)(i + frame / 3 * 16)) % 50) - 25, k = (int)(hs((uint32_t)(i + 1 + frame / 3 * 16)) % 50) - 25;
            quad(x, y + (float)j * s, nx, ny + (float)k * s, 14 * s, col_lerp(RGB(255, 240, 80), bg, fade / 3));
            quad(x, y + (float)j * s, nx, ny + (float)k * s, 5 * s, col_lerp(WHITE, bg, fade / 4));
            break;
        }
        case TRX_DUST:
            for (int k = 0; k < 3; k++) {
                uint32_t h = hs((uint32_t)(i * 3 + k + (frame / 2) * 48));
                int px = (int)(h % 61) - 30, py = (int)((h >> 8) % 61) - 30 - i;
                int sz = (int)(16 * s) - i / 2 + 2;
                draw_rect((int)x + px, (int)y + py, sz, sz, col_lerp((h >> 16) % 3 == 0 ? WHITE : (h >> 16) % 3 == 1 ? c1 : c2, bg, fade));
            }
            break;
        case TRX_SNOW:
            if (i % 2)
                flake(x + fsin((float)(frame + i * 7) * 0.15f) * 10, y + (float)(i * 4), (20 - (float)i * 0.7f) * s, col_lerp(WHITE, bg, fade / 3));
            break;
        case TRX_EGGS:
            if (i % 3 == 1)
                draw_egg(x, y + (float)((frame + i * 5) % 10) - 5, (40 - (float)i * 1.4f) * s, fsin((float)(frame + i * 10) * 0.3f) * 4,
                         (uint32_t)(i * 77 + 5), fade / 3, bg);
            break;
        case TRX_CONFETTI:
            for (int k = 0; k < 3; k++) {
                uint32_t h = hs((uint32_t)(i * 3 + k));
                float px = (float)((int)(h % 71) - 35), py = (float)((int)((h >> 8) % 71) - 35) + (float)(i * 3);
                draw_rot_square(x + px, y + py, (22 - (float)i * 0.7f) * s, (float)(frame + (int)(h % 97)) * 0.2f,
                                col_lerp(hue((int)(h >> 16) % 360), bg, fade / 3));
            }
            break;
        }
    }
    return 1;
}

// --- The cube -------------------------------------------------------------------------

float rw_big(void)
{
    return scene == RW_PLAYER && party(PA_BIGHEAD) ? 1.6f : 1.0f;
}

color_t rw_body(color_t c1)
{
    switch (worn(RK_FINISH)) {
    case FIX_CHROME: return RGB(196, 204, 218);
    case FIX_RAINBOW: return hue(frame * 5);
    case FIX_GALAXY: return RGB(40, 14, 90);
    case FIX_NEON: return RGB(14, 6, 30);
    case FIX_CANDY: return RGB(255, 248, 250);
    case FIX_GOLDEN: return RGB(255, 196, 30);
    }
    return c1;
}

// Point (x, y) of a square of side s turned by rot, about (cx, cy).
static void turn(float cx, float cy, float rot, float x, float y, float *ox, float *oy)
{
    float c = fcos(rot), sn = fsin(rot);
    *ox = cx + x * c - y * sn;
    *oy = cy + x * sn + y * c;
}

static void local(float cx, float cy, float rot, int n, const float *pts, color_t c)
{
    float xs[8], ys[8];
    for (int i = 0; i < n; i++)
        turn(cx, cy, rot, pts[2 * i], pts[2 * i + 1], &xs[i], &ys[i]);
    draw_poly(n, xs, ys, c);
}

// over 0: patterns on the cube's body, under its dark middle; 2: over the
// middle, under the face; 1: on top of everything (hat and eyes).
void rw_cube(float cx, float cy, float s, float rot, color_t c1, color_t c2, int over)
{
    float h = s / 2;
    int fin = worn(RK_FINISH);
    (void)c1;
    if (over == 0) {
        if (fin == FIX_CHROME || fin == FIX_GOLDEN) {
            int gold = fin == FIX_GOLDEN;
            float band[8] = { -h, -h * 0.2f, -h * 0.2f, -h, h * 0.2f, -h, -h, h * 0.2f };
            float shine[8] = { -h * 0.05f, h, h, -h * 0.05f, h, h * 0.35f, h * 0.35f, h };
            local(cx, cy, rot, 4, band, gold ? RGB(255, 236, 140) : WHITE);
            local(cx, cy, rot, 4, shine, gold ? RGB(200, 130, 0) : RGB(120, 130, 150));
            if (gold && (frame / 6) % 5 == 0) {
                float sx, sy;
                turn(cx, cy, rot, h * 0.5f, -h * 0.55f, &sx, &sy);
                ui_star(sx, sy, s * 0.18f, WHITE);
            }
        } else if (fin == FIX_NEON) {
            draw_rot_square(cx, cy, s * 0.86f, rot, c2);
            draw_rot_square(cx, cy, s * 0.72f, rot, RGB(14, 6, 30));
        } else if (fin == FIX_CANDY) {
            for (int k = -2; k <= 2; k++) {
                float o = (float)k * h * 0.5f;
                float st[8] = { o - h * 0.12f, -h, o + h * 0.12f, -h, o + h * 0.12f, h, o - h * 0.12f, h };
                for (int p = 0; p < 4; p++)
                    st[2 * p] += st[2 * p + 1] * 0.5f;
                for (int p = 0; p < 4; p++)
                    st[2 * p] = st[2 * p] < -h ? -h : st[2 * p] > h ? h : st[2 * p];
                local(cx, cy, rot, 4, st, k % 2 ? c2 : RGB(255, 60, 120));
            }
        } else if (fin == FIX_GLITCH) {
            // Red and cyan halves out of step, and bars of noise that jump about.
            int j = (int)(hs((uint32_t)frame / 4) % 9) + 3;
            float lh[8] = { -h, -h, -h * 0.1f, -h, -h * 0.1f, h, -h, h }, rh[8] = { h * 0.1f, -h, h, -h, h, h, h * 0.1f, h };
            local(cx - (float)j * s / 76, cy, rot, 4, lh, RGB(255, 40, 90));
            local(cx + (float)j * s / 76, cy, rot, 4, rh, RGB(30, 230, 255));
            for (int k = 0; k < 3; k++) {
                uint32_t r2 = hs((uint32_t)(frame / 3 * 3 + k));
                int by = (int)(cy - h) + (int)(r2 % (uint32_t)(s + 1)), bw = (int)(s * (0.4f + (float)(r2 >> 20 & 7) / 10.0f));
                draw_rect((int)(cx - h) + (int)((r2 >> 8) % 20) - 10, by, bw, (int)(s * 0.06f) + 2,
                          k == 0 ? WHITE : k == 1 ? RGB(40, 255, 120) : RGB(255, 230, 40));
            }
        }
        return;
    }
    if (over == 2) {
        if (fin == FIX_GALAXY) {
            for (int i = 0; i < 9; i++) {
                float sx, sy;
                uint32_t k = hs((uint32_t)i + 11);
                turn(cx, cy, rot, ((float)(k % 100) / 100 - 0.5f) * s * 0.75f, ((float)((k >> 8) % 100) / 100 - 0.5f) * s * 0.75f,
                     &sx, &sy);
                int tw = (frame / 6 + i) % 4 == 0, d = (int)(s * 0.06f) + 2 + tw * 2;
                if (i % 3)
                    draw_rect((int)sx - d / 2, (int)sy - d / 2, d, d, WHITE);
                else
                    ui_star(sx, sy, (float)d, RGB(160, 220, 255));
            }
            draw_circle((int)(cx - s * 0.12f), (int)(cy + s * 0.1f), (int)(s * 0.2f), RGB(110, 40, 170));
            draw_circle((int)(cx - s * 0.12f), (int)(cy + s * 0.1f), (int)(s * 0.1f), RGB(220, 120, 255));
        }
        return;
    }
    if (party(PA_HAT)) {
        float hat[6] = { -h * 0.55f, -h, h * 0.55f, -h, h * 0.05f, -h * 2.1f };
        float stripe[8] = { -h * 0.38f, -h * 1.3f, h * 0.36f, -h * 1.3f, h * 0.27f, -h * 1.48f, -h * 0.27f, -h * 1.48f };
        local(cx, cy, rot, 3, hat, RGB(255, 60, 160));
        local(cx, cy, rot, 4, stripe, RGB(255, 230, 60));
        float px, py;
        turn(cx, cy, rot, h * 0.05f, -h * 2.15f, &px, &py);
        draw_circle((int)px, (int)py, (int)(s * 0.1f) + 2, RGB(90, 230, 255));
    }
    if (party(PA_GOOGLY)) {
        for (int e = -1; e <= 1; e += 2) {
            float ex, ey, px, py;
            turn(cx, cy, rot, (float)e * h * 0.42f, -h * 0.25f, &ex, &ey);
            draw_circle((int)ex, (int)ey, (int)(s * 0.2f), RGB(20, 20, 30));
            draw_circle((int)ex, (int)ey, (int)(s * 0.17f), WHITE);
            float wob = fsin((float)frame * 0.3f + (float)e) * s * 0.05f;
            px = ex + wob;
            py = ey + s * 0.07f;
            draw_circle((int)px, (int)py, (int)(s * 0.08f) + 1, RGB(10, 10, 20));
        }
    }
}

// --- Pets --------------------------------------------------------------------------

void rw_pet(const float *tx, const float *ty, int mini)
{
    int p = worn(RK_PET);
    if (!p)
        return;
    float x = veh_sx(tx[TRAIL_N / 2]) - 70, y = veh_sy(ty[TRAIL_N / 2]) - 90 + fsin((float)frame * 0.12f) * 12;
    int px = mini ? 3 : 5;
    pet_draw(p - 1, (frame / 10) % 2, (int)x - 8 * px, (int)y - 8 * px, px);
}

// --- Crashes -----------------------------------------------------------------------

int rw_burst(Particle *parts, const Look *look, float x, float y, uint32_t *rng)
{
    int style = worn(RK_DEATH);
    if (!style)
        return 0;
    color_t c1 = LOOK_COLORS[look->v[LOOK_COL1]], c2 = LOOK_COLORS[look->v[LOOK_COL2]];
    for (int i = 0; i < MAX_PARTS; i++) {
        Particle *p = &parts[i];
        uint32_t h = hs(*rng + (uint32_t)i * 977u);
        p->x = x;
        p->y = y;
        p->life = 0;
        if (style == DEX_PIXELS) {
            // His cube breaks into a grid of pixels that fall.
            p->x = x + (float)(i % 6 - 3) * 13;
            p->y = y + (float)(i / 6 - 3) * 13;
            p->vx = (float)(i % 6 - 3) * 1.6f + (float)((int)(h % 7) - 3) * 0.4f;
            p->vy = (float)(3 - i / 6) * 1.4f + 7 + (float)(h % 5);
            p->max_life = p->life = 60 + (int)(h % 20);
            p->size = 13;
            p->color = (i + i / 6) % 2 ? c1 : c2;
        } else if (style == DEX_RAINBOW) {
            float a = (float)i * (6.2831853f / MAX_PARTS);
            p->vx = fcos(a) * (9 + (float)(h % 6));
            p->vy = fsin(a) * (9 + (float)(h % 6)) + 6;
            p->max_life = p->life = 50;
            p->size = 16;
            p->color = hue(i * 9);
        } else if (style == DEX_EGG || style == DEX_JELLY) {
            if (i >= 12)
                continue;
            p->vx = (float)((int)(h % 17) - 8);
            p->vy = 8 + (float)(h % 12);
            p->max_life = p->life = 40;
            p->size = 10;
            p->color = style == DEX_EGG ? (i % 2 ? WHITE : RGB(255, 210, 40)) : c1;
        } else if (style == DEX_POOF && i < 10) {
            p->vx = (float)((int)(h % 13) - 6);
            p->vy = (float)(h % 9);
            p->max_life = p->life = 30;
            p->size = 12;
            p->color = RGB(230, 230, 240);
        }
    }
    *rng = hs(*rng + 1);
    return 1;
}

int rw_slowmo(void)
{
    return scene != RW_MUTED && (reward_party() & PA_SLOWMO);
}

void rw_dead(const Run *r)
{
    int style = worn(RK_DEATH), t = r->state_frames;
    if (!style)
        return;
    float x = veh_sx(r->b.x), y = veh_sy(r->b.y), s = veh_scale();
    color_t c1 = LOOK_COLORS[r->look.v[LOOK_COL1]];
    if (style == DEX_POOF) {
        float k = t < 16 ? (float)t / 16 : 1;
        for (int i = 0; i < 7; i++) {
            float a = (float)i * 0.9f;
            draw_circle((int)(x + fcos(a) * 46 * k * s), (int)(y + fsin(a) * 30 * k * s - (float)t),
                        (int)((26 + i % 3 * 8) * k * s), i % 2 ? RGB(235, 235, 245) : RGB(200, 200, 215));
        }
        if (t > 6) {
            int sc = t < 14 ? 4 + (t - 6) / 2 : 8;
            draw_text_glow((int)x - text_width("POOF!", sc) / 2, (int)y - 150 - t, sc, "POOF!", RGB(255, 240, 120), RGB(255, 80, 160));
        }
    } else if (style == DEX_GHOST) {
        float gy = y - (float)t * 3, gx = x + fsin((float)t * 0.15f) * 22;
        color_t c = col_lerp(WHITE, veh_bg(), t * 3 > 200 ? 200 : t * 3);
        draw_circle((int)gx, (int)gy - 14, 50, c);
        draw_rect((int)gx - 50, (int)gy - 14, 100, 60, c);
        for (int k = 0; k < 4; k++)
            draw_circle((int)gx - 37 + k * 25, (int)gy + 46, 13, (k + t / 6) % 2 ? c : veh_bg());
        draw_rect((int)gx - 24, (int)gy - 30, 13, 20, RGB(20, 20, 40));
        draw_rect((int)gx + 11, (int)gy - 30, 13, 20, RGB(20, 20, 40));
        draw_circle((int)gx, (int)gy + 8, 9, RGB(20, 20, 40));
    } else if (style == DEX_JELLY) {
        float k = t < 10 ? (float)t / 10 : 1, gy = veh_sy(r->b.y - run_half_h(&r->b));
        float xs[8] = { x - 90 * k, x - 60 * k, x - 20 * k, x + 30 * k, x + 80 * k, x + 95 * k, x + 40 * k, x - 50 * k };
        float ys[8] = { gy, gy - 18 * k, gy - 30 * k, gy - 26 * k, gy - 12 * k, gy, gy + 6, gy + 6 };
        draw_poly(8, xs, ys, c1);
        draw_circle((int)(x - 30 * k), (int)(gy - 16 * k), (int)(7 * k) + 1, col_lerp(c1, WHITE, 140));
    } else if (style == DEX_EGG) {
        float k = (float)t;
        draw_egg_rows(x - k * 3, y - k * 4 + k * k * 0.08f, 80 * s, -k * 0.9f, 0, 0, veh_bg(), 0, 15);
        draw_egg_rows(x + k * 2, y + k * 1.5f, 80 * s, k * 0.4f, 0, 0, veh_bg(), 15, 32);
        if (t > 4)
            draw_circle((int)x, (int)(y + 6), (int)(16 * s), RGB(255, 200, 30));
    } else if (style == DEX_RAINBOW) {
        int rr = t * 9;
        for (int b = 0; b < 6 && rr - b * 8 > 0; b++)
            for (int k = 0; k < 24; k++) {
                float a = (float)k * 0.2618f;
                draw_rect((int)(x + fcos(a) * (float)(rr - b * 8)) - 3, (int)(y + fsin(a) * (float)(rr - b * 8)) - 3, 7, 7, hue(b * 60));
            }
    }
}

// --- Effects over the whole screen -----------------------------------------------------

void rw_event(int what, float x, float y)
{
    if (what == RW_JUMP && party(PA_CONFETTI))
        for (int i = 0, n = 0; i < 48 && n < 10; i++)
            if (conf[i].life <= 0) {
                uint32_t h = hs((uint32_t)(frame * 64 + i));
                conf[i].x = x;
                conf[i].y = y;
                conf[i].vx = (float)((int)(h % 15) - 7);
                conf[i].vy = 6 + (float)((h >> 8) % 9);
                conf[i].life = 40;
                conf[i].c = hue((int)(h >> 16) % 360);
                n++;
            }
}

void rw_tick(void)
{
    for (int i = 0; i < 48; i++)
        if (conf[i].life > 0) {
            conf[i].life--;
            conf[i].x += conf[i].vx;
            conf[i].y += conf[i].vy;
            conf[i].vy -= 0.6f;
        }
}

float rw_zoom(void)
{
    return scene == RW_PLAYER && party(PA_TINY) ? 0.78f : 1.0f;
}

float rw_hue(int beat)
{
    return scene == RW_PLAYER && party(PA_DISCO) ? (float)(beat % 4) * 0.8f : 0;
}

int rw_filter(void)
{
    return scene == RW_PLAYER ? reward_worn(RK_FILTER) : 0;
}

void rw_overlay(const Run *r, float beat)
{
    if (scene != RW_PLAYER)
        return;
    for (int i = 0; i < 48; i++)
        if (conf[i].life > 0)
            draw_rot_square(veh_sx(conf[i].x), veh_sy(conf[i].y), 12, (float)(conf[i].life + i) * 0.3f, conf[i].c);
    if (party(PA_DISCO)) {
        float bx = SCREEN_W / 2, by = 70;
        for (int k = 0; k < 6; k++) {
            float a = 0.5f + (float)k * 0.42f + fsin((float)frame * 0.02f) * 0.3f;
            quad(bx, by, bx + fcos(a) * 1400, by + fsin(a) * 1400, 6 + beat * 10, hue(k * 60 + frame * 2));
        }
        draw_rect((int)bx - 2, 0, 4, (int)by - 30, RGB(160, 160, 180));
        draw_circle((int)bx, (int)by, 40, RGB(200, 200, 220));
        for (int k = 0; k < 8; k++)
            draw_rect((int)bx - 34 + (k % 4) * 17, (int)by - 30 + (k / 4) * 30 + ((frame / 8 + k) % 2) * 6, 14, 14,
                      (frame / 8 + k) % 3 ? RGB(240, 240, 255) : hue(k * 45));
    }
    if (r->state == RUN_DEAD && party(PA_SLOWMO) && r->state_frames < 36) {
        int bar = r->state_frames < 8 ? r->state_frames * 12 : 96;
        draw_rect(0, 0, SCREEN_W, bar, RGB(0, 0, 0));
        draw_rect(0, SCREEN_H - bar, SCREEN_W, bar, RGB(0, 0, 0));
        draw_text_glow(60, SCREEN_H - 80, 5, "SLOW MO", RGB(255, 255, 255), RGB(255, 60, 120));
    }
    if (r->state == RUN_DONE && party(PA_FIREWORKS))
        for (int b = 0; b < 6; b++) {
            int t = r->state_frames - b * 12;
            if (t < 0)
                continue;
            t %= 72;
            float x = (float)(200 + b * 300 + (b % 2) * 80), y = (float)(180 + (b % 3) * 120);
            color_t c = hue(b * 70);
            for (int i = 0; i < 20; i++) {
                float a = (float)i * 0.314f, d = (float)t * (6 - (float)t * 0.04f);
                draw_rect((int)(x + fcos(a) * d) - 6, (int)(y + fsin(a) * d + (float)(t * t) * 0.02f) - 6, 13, 13,
                          col_lerp(i % 2 ? c : WHITE, RGB(0, 0, 0), t * 2));
            }
        }
    int f = reward_worn(RK_FILTER);
    if (f == FILTER_CRT) {
        draw_set_filter(0);
        for (int y = 0; y < SCREEN_H; y += 4)
            draw_rect(0, y, SCREEN_W, 1, RGB(0, 0, 0));
        draw_set_filter(f);
    } else if (f == FILTER_SEPIA) {
        for (int k = 0; k < 3; k++) {
            uint32_t h = hs((uint32_t)(frame / 3 * 3 + k));
            if (h % 3)
                draw_rect((int)(h % SCREEN_W), 0, 2, SCREEN_H, RGB(60, 40, 20));
        }
        if ((frame / 5) % 7 == 0)
            draw_rect(0, 0, SCREEN_W, 6, RGB(30, 20, 10));
    }
}

// --- The title -----------------------------------------------------------------------

void rw_title_back(void)
{
    int t = reward_worn(RK_TITLE);
    if (t == TIX_NIGHT) {
        draw_gradient(RGB(2, 4, 26), RGB(20, 30, 80));
        for (int i = 0; i < 70; i++) {
            uint32_t h = hs((uint32_t)i + 900);
            int tw = (frame / 10 + i) % 11 == 0;
            draw_rect((int)(h % SCREEN_W), (int)((h >> 12) % 760), 3 + tw * 3, 3 + tw * 3, tw ? WHITE : RGB(170, 180, 230));
        }
        draw_circle(1640, 170, 90, RGB(250, 245, 210));
        draw_circle(1610, 150, 90, RGB(6, 8, 34));
    } else if (t == TIX_EGGS) {
        for (int i = 0; i < 9; i++) {
            float x = (float)(110 + i * 215), hop = fsin((float)(frame + i * 20) * 0.1f);
            draw_egg(x, (float)(GROUND_Y - 40) - (hop > 0 ? hop * 50 : 0), 52, hop * 6, (uint32_t)(i * 31 + 7), 0, RGB(30, 10, 60));
        }
    } else if (t == TIX_PARTY) {
        for (int i = 0; i < 60; i++) {
            uint32_t h = hs((uint32_t)i + 40);
            int y = wrapi(frame * (2 + (int)(h % 3)) + (int)(h >> 8), SCREEN_H + 40) - 20;
            draw_rot_square((float)(h % SCREEN_W), (float)y, 14, (float)(frame + i * 10) * 0.08f, hue((int)(h >> 16) % 360));
        }
        for (int k = 0; k < 12; k++)
            quad((float)(k * 170), 0, (float)(k * 170 + 85), 60, 10, hue(k * 30));
    }
}

int rw_title_logo(int x, int y, int scale, int gap)
{
    if (reward_worn(RK_TITLE) != TIX_RAINBOW)
        return 0;
    const char *w = "FRITZDASH";
    char s[2] = { 0, 0 };
    for (int i = 0; i < 9; i++) {
        s[0] = w[i];
        int bob = (int)(fsin((float)frame * 0.12f + (float)i * 0.6f) * 10.0f);
        color_t c = hue(frame * 3 + i * 40);
        draw_text_glow(x, y + bob, scale, s, col_lerp(c, WHITE, 90), c);
        x += 6 * scale + (i == 4 ? gap : 0);
    }
    return 1;
}

void rw_screensaver(void)
{
    draw_gradient(RGB(0, 0, 10), RGB(10, 0, 30));
    bg_warp(RGB(4, 0, 20), 0);
    int bx = wrapi(frame * 3, 2 * (SCREEN_W - 900)), by = wrapi(frame * 2, 2 * (SCREEN_H - 200));
    bx = bx > SCREEN_W - 900 ? 2 * (SCREEN_W - 900) - bx : bx;
    by = by > SCREEN_H - 200 ? 2 * (SCREEN_H - 200) - by : by;
    draw_text_glow(bx, by, 14, "FRITZ DASH", hue(frame), RGB(255, 255, 255));
    draw_text_glow_centered(SCREEN_H - 70, 4, "PRESS ANY BUTTON", RGB(200, 200, 255), RGB(80, 80, 200));
}
