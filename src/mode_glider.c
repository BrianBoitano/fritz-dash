// Fritz Dash - the glider: a hang glider in a ten cell corridor. Holding X
// spreads the wing and he drifts down slowly, each press is one small flap,
// and letting go drops him fast. Updraft fans (also here) lift him.
#include <stdio.h>

#include "input.h"
#include "mode.h"

#define GLIDE_G    1.25f
#define GLIDE_SINK 2.4f         // holding X: the fastest he sinks
#define GLIDE_FLAP 12.5f
#define GLIDE_FALL 15.0f
#define GLIDE_RISE 13.0f
#define FAN_LIFT   3.4f         // a fan's push each frame (the glider's; others by their boost)
#define FAN_TOP    12.0f        // and the speed it pushes up to
#define FAN_PUSH   6.0f         // a fan turned sideways pushes him along

static void glider_step(Run *r, Body *b, int held, int input)
{
    (void)input;
    float g = (float)b->grav, k = b->mini ? MINI_JUMP : 1.0f;
    if (r->buffered && !run_orb_waiting(r)) {
        if (b->vy * g < GLIDE_FLAP * k)
            b->vy = GLIDE_FLAP * k * g;
        b->on_ground = 0;
        b->fx_t = 0;
        run_jumped(r);
    }
    b->vy -= GLIDE_G * g * (1.0f / SUB);
    if (held && b->vy * g < -GLIDE_SINK)
        b->vy += (-GLIDE_SINK * g - b->vy) * 0.2f;
    if (b->vy * g < -GLIDE_FALL)
        b->vy = -GLIDE_FALL * g;
    if (b->vy * g > GLIDE_RISE)
        b->vy = GLIDE_RISE * g;
    b->fx_y = held ? 1.0f : 0.0f;
}

static void glider_turn(Body *b)
{
    float a = -b->vy * 0.022f * (float)b->grav;
    b->rot = a > 0.4f ? 0.4f : a < -0.4f ? -0.4f : a;
}

// A thin bar from (ax, ay) to (bx, by), w pixels each side.
static void bar(float x, float y, float rot, int flip, float ax, float ay, float bx, float by, float w, color_t c)
{
    float dx = bx - ax, dy = by - ay, ox = 0, oy = w;
    if ((dx < 0 ? -dx : dx) < (dy < 0 ? -dy : dy)) {
        ox = w;
        oy = 0;
    }
    float q[8] = { ax - ox, ay - oy, bx - ox, by - oy, bx + ox, by + oy, ax + ox, ay + oy };
    veh_poly(x, y, rot, flip, 4, q, c);
}

// A big delta wing seen from above and behind, his cube hanging under it
// from the control bar. Holding X spreads the wing wide; a flap beats its
// tips down; letting go tucks them in for the dive.
static void glider_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot;
    int flip = b->grav < 0;
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2), white = veh_color(VC_WHITE), bg = veh_bg();
    float beat = b->fx_t < 12 ? fsin((float)b->fx_t * (PI_F / 12)) * 18 : 0, tuck = b->fx_y > 0 ? 0 : 1;
    float nx = 60, ny = -30, kx = -48, ky = -36;
    float ux = -64 + tuck * 14, uy = -70 + beat + tuck * 12, lx = -60 + tuck * 12, ly = -10 - beat * 0.4f - tuck * 8;
    float far[6] = { nx, ny, ux, uy, kx, ky };
    veh_poly(x, y, rot, flip, 3, far, col_scale(c2, 3, 5));
    float farstripe[6] = { nx - 20, ny - 3, ux + 26, uy + 11, kx + 18, ky + 1 };
    veh_poly(x, y, rot, flip, 3, farstripe, col_scale(c1, 3, 5));
    bar(x, y, rot, flip, 0, -32, -16, 14, 2, RGB(190, 195, 215));
    bar(x, y, rot, flip, 4, -32, 22, 14, 2, RGB(190, 195, 215));
    veh_pilot(x, y, rot, flip, 3, 2, 44);
    bar(x, y, rot, flip, -20, 15, 26, 15, 3, RGB(225, 228, 240));
    float near[6] = { nx, ny, kx, ky, lx, ly };
    veh_poly(x, y, rot, flip, 3, near, c2);
    float stripe[6] = { nx - 18, ny + 1, kx + 22, ky + 2, lx + 24, ly - 6 };
    veh_poly(x, y, rot, flip, 3, stripe, c1);
    float tip[6] = { nx - 4, ny, nx - 30, ny - 2, nx - 28, ny + 5 };
    veh_poly(x, y, rot, flip, 3, tip, white);
    bar(x, y, rot, flip, nx, ny, ux, uy, 2, white);
    bar(x, y, rot, flip, nx, ny, lx, ly, 2, white);
    bar(x, y, rot, flip, nx, ny, kx, ky, 1.5f, col_scale(c2, 1, 3));
    if (b->fx_y > 0)
        for (int k = 0; k < 3; k++) {
            float d = (float)((frame * 9 + k * 23) % 60);
            float sy = k == 0 ? uy : k == 1 ? ly : ky;
            bar(x, y, rot, flip, (k == 0 ? ux : k == 1 ? lx : kx) - 10 - d, sy, (k == 0 ? ux : k == 1 ? lx : kx) - 26 - d,
                sy, 1.5f, col_lerp(white, bg, 70 + (int)d * 3));
        }
}

// Two thin wind streams off the wing tips.
static void glider_trail(const float *tx, const float *ty)
{
    color_t bg = veh_bg();
    float s = veh_scale();
    for (int i = 1; i < TRAIL_N; i++) {
        color_t c = col_lerp(RGB(235, 245, 255), bg, 70 + i * 13);
        int w = (int)(10 * s) + 2;
        draw_rect((int)(veh_sx(tx[i]) - 64 * s), (int)(veh_sy(ty[i]) - 8 * s), w, 3, c);
        if (i % 2)
            draw_rect((int)(veh_sx(tx[i]) - 40 * s), (int)(veh_sy(ty[i]) - 44 * s), w, 3, c);
    }
}

static const float OUTLINE[16] = { 60, -30, 0, -52, -64, -70, -48, -36, -60, -10, -20, 16, 26, 16, 24, -4 };

const ModeDef MODE_GLIDER_DEF = {
    .name = "glider", .label = "GLIDER", .portal_name = "portal_glider", .flags = MF_SLIDE,
    .corridor = 10, .half_w = PLAYER_S / 2.0f, .half_h = 30.0f, .boost = 0.72f, .inputs = IN_CROSS,
    .color = RGB(200, 235, 255),
    .step = glider_step, .turn = glider_turn, .draw = glider_draw, .trail = glider_trail,
    .noutline = 8, .outline = OUTLINE,
};

// --- Updraft fan: a column of air five cells long, blowing the way it points.

static void fan_act(Run *r, const LevelObj *o)
{
    Body *b = &r->b;
    const ModeDef *d = mode_def(b->mode);
    if (d->boost == 0)
        return;
    float k = b->mode == MODE_GLIDER ? 1.0f : d->boost;
    switch (o->rot & 3) {
    case 0:
        if (b->vy < FAN_TOP * k)
            b->vy += FAN_LIFT * k * (1.0f / SUB);
        break;
    case 2:
        if (b->vy > -FAN_TOP * k)
            b->vy -= FAN_LIFT * k * (1.0f / SUB);
        break;
    case 1: b->x += FAN_PUSH * (1.0f / SUB); break;
    default: b->x -= FAN_PUSH * (1.0f / SUB); break;
    }
}

// Screen point (px, py) from the cell centre, turned with the fan.
static void fan_pt(int rot, float px, float py, float *ox, float *oy)
{
    switch (rot & 3) {
    case 0: *ox = px; *oy = py; break;
    case 1: *ox = -py; *oy = px; break;
    case 2: *ox = -px; *oy = -py; break;
    default: *ox = py; *oy = -px; break;
    }
}

static void fan_rect(const LevelObj *o, float x, float y, float x0, float y0, float x1, float y1, color_t c)
{
    float ax, ay, bx, by;
    fan_pt(o->rot, x0, y0, &ax, &ay);
    fan_pt(o->rot, x1, y1, &bx, &by);
    int l = (int)(x + (ax < bx ? ax : bx)), t = (int)(y + (ay < by ? ay : by));
    draw_rect(l, t, (int)(ax < bx ? bx - ax : ax - bx), (int)(ay < by ? by - ay : ay - by), c);
}

// A grille at the bottom of its cell with a blurred spinning blade, and
// the wind it blows: bright streaks and sparkles rushing along a column
// five cells long, between two faint guide lines.
static void fan_draw(const LevelObj *o, float x, float y, int frame)
{
    color_t air = RGB(190, 245, 255), bg = veh_bg();
    for (int side = -1; side <= 1; side += 2)
        for (int k = 0; k < 9; k++)
            fan_rect(o, x, y, (float)side * 38 - 1, 18 - (float)k * 42, (float)side * 38 + 1, -2 - (float)k * 42,
                     col_lerp(air, bg, 150 + k * 11));
    for (int k = 0; k < 9; k++) {
        float lx = -28.0f + (float)((k * 29) % 56);
        float h = (float)((frame * 7 + k * 47) % 360);
        float len = 34.0f + (float)(k % 3) * 14;
        fan_rect(o, x, y, lx, 22 - h - len, lx + 3, 22 - h, col_lerp(air, bg, 30 + (int)(h * 0.6f)));
    }
    for (int k = 0; k < 4; k++) {
        float h = (float)((frame * 4 + k * 97) % 360);
        float wob = fsin((float)(frame + k * 40) * 0.15f) * 16;
        float px = wob + (float)(k - 2) * 14;
        fan_rect(o, x, y, px - 4, 16 - h, px + 4, 24 - h, col_lerp(RGB(255, 255, 255), bg, 20 + (int)(h * 0.6f)));
    }
    fan_rect(o, x, y, -40, 22, 40, 40, RGB(60, 66, 84));
    fan_rect(o, x, y, -36, 26, 36, 38, RGB(100, 108, 130));
    for (int k = 0; k < 4; k++)
        fan_rect(o, x, y, -32 + (float)k * 18, 28, -22 + (float)k * 18, 36, RGB(40, 44, 58));
    float w = 34.0f * fsin((float)frame * 0.9f);
    w = w < 0 ? -w : w;
    fan_rect(o, x, y, -36, 16, 36, 22, col_lerp(RGB(200, 210, 230), bg, 120));
    fan_rect(o, x, y, -w - 2, 14, w + 2, 22, RGB(230, 236, 250));
    fan_rect(o, x, y, -6, 12, 6, 24, RGB(255, 200, 40));
}

static void fan_info(const LevelObj *o, char *buf, int n)
{
    static const char *const WAY[4] = { "UP", "RIGHT", "DOWN", "LEFT" };
    snprintf(buf, (size_t)n, "BLOWS %s FOR 5 CELLS: THE GLIDER RIDES IT", WAY[o->rot & 3]);
}

const ObjDef OBJ_FAN_DEF = {
    .name = "fan", .label = "UPDRAFT FAN", .cat = PAL_JUMPS, .kind = K_TRIGGER,
    .x0 = 0.1f, .y0 = 0, .x1 = 0.9f, .y1 = 5,
    .act = fan_act, .draw = fan_draw, .info = fan_info,
};
