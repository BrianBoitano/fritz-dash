// Fritz Dash - the surfer: on water (also here) he floats on his board
// and rides the surface. Holding X dives under; letting go, the water
// shoots him back up and out, higher the deeper he dived. On land a press
// hops.
#include <stdio.h>

#include "input.h"
#include "mode.h"

#define SURF_G    2.3f
#define SURF_HOP  26.0f
#define MAX_FALL  36.0f
#define BUOY      5.2f          // the water's lift with all of him under
#define DIVE      5.0f          // holding X pushes him down this much
#define SINK_MAX  11.0f
#define RISE_MAX  40.0f

static int is_water(const Level *lv, int x, int y)
{
    int i = level_find(lv, x, y);
    return i >= 0 && lv->objs[i].type == OBJ_WATER;
}

// The top of the water under or around the body (world y), or -1e9 when
// there is none in its columns at or below its head.
static float water_top(const Level *lv, const Body *b)
{
    float hw = run_half_w(b) * 0.5f, top = -1e9f, head = b->y + run_half_h(b);
    int c0 = (int)((b->x - hw) / TILE), c1 = (int)((b->x + hw) / TILE);
    for (int i = level_first_at(lv, c0); i < lv->nobj && lv->objs[i].x <= c1; i++) {
        const LevelObj *o = &lv->objs[i];
        float y1 = (float)(o->y + 1) * TILE;
        if (o->type == OBJ_WATER && (float)o->y * TILE < head && y1 > top)
            top = y1;
    }
    return top;
}

// fx_y: 1 while he is in the water, fx_t: frames since he went in or came out (drawing).
static void surfer_step(Run *r, Body *b, int held, int input)
{
    (void)input;
    float g = (float)b->grav, k = b->mini ? MINI_JUMP : 1.0f, hh = run_half_h(b);
    float top = water_top(r->lv, b), sub = top - (b->y - hh);
    int wet = sub > 0;
    if (wet != (b->fx_y > 0)) {
        b->fx_y = (float)wet;
        b->fx_t = 0;
    }
    if (wet) {
        float f = sub / (2 * hh);
        if (f > 1)
            f = 1;
        b->vy += (BUOY * f - SURF_G - (held ? DIVE : 0)) * (1.0f / SUB);
        float v = b->vy < 0 ? -b->vy : b->vy;
        b->vy *= held ? 0.99f : v < 6 && f < 0.9f ? 0.95f : 0.995f;
        if (b->vy < -SINK_MAX)
            b->vy = -SINK_MAX;
        if (b->vy > RISE_MAX)
            b->vy = RISE_MAX;
        return;
    }
    if (b->on_ground && r->buffered && !run_orb_waiting(r)) {
        b->vy = SURF_HOP * k * g;
        b->on_ground = 0;
        run_jumped(r);
    }
    b->vy -= SURF_G * g * (1.0f / SUB);
    if (b->vy * g < -MAX_FALL)
        b->vy = -MAX_FALL * g;
}

static void surfer_turn(Body *b)
{
    float a = -b->vy * 0.014f;
    a = a > 0.35f ? 0.35f : a < -0.35f ? -0.35f : a;
    b->rot += (a - b->rot) * 0.3f;
}

// His cube standing on a surfboard. Riding the surface throws spray off
// the tail; under water bubbles rise from him and the surface ripples over
// him; going in or out makes a splash.
static void surfer_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot, s = veh_scale();
    int flip = b->grav < 0;
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2), white = veh_color(VC_WHITE), bg = veh_bg();
    const Run *r = draw_run();
    float top = r ? water_top(r->lv, b) : -1e9f, wy = veh_sy(top);
    int wet = b->fx_y > 0, under = wet && top > b->y + run_half_h(b) * 0.4f;
    if (b->fx_t < 14 && top > -1e8f) {
        float t = (float)b->fx_t;
        for (int k = 0; k < 9; k++) {
            float a = (float)k * 0.39f + 0.2f, sp = (5.0f + (float)(k % 3) * 2) * s;
            float px = x + (fcos(a) * 1.4f - 0.6f) * sp * t, py = wy - fsin(a) * sp * t + 0.45f * t * t * s;
            int sz = (int)((11 - t * 0.6f) * s) + 2;
            draw_rect((int)px - sz / 2, (int)py - sz / 2, sz, sz, col_lerp(RGB(200, 240, 255), bg, 30 + b->fx_t * 12));
        }
    }
    if (wet && !under)
        for (int k = 0; k < 6; k++) {
            int age = (frame * 3 + k * 5) % 16;
            float px = x - (40.0f + (float)age * 6) * s, py = wy - (float)(age * (10 - age)) * 0.7f * s;
            int sz = (int)((9 - age / 2) * s) + 2;
            draw_rect((int)px, (int)py, sz, sz, col_lerp(white, bg, 40 + age * 12));
        }
    float board[12] = { -50, 26, -40, 20, 40, 20, 56, 23, 44, 32, -46, 32 };
    veh_poly(x, y, rot, flip, 6, board, c2);
    float stripe[8] = { -40, 24, 44, 24, 46, 27, -42, 27 };
    veh_poly(x, y, rot, flip, 4, stripe, c1);
    float fin[6] = { -36, 32, -24, 32, -34, 42 };
    veh_poly(x, y, rot, flip, 3, fin, col_scale(c2, 1, 2));
    veh_pilot(x, y, rot, flip, 0, -12, 60);
    float lean = fsin((float)frame * 0.1f) * 3;
    veh_rect(x, y, rot, flip, -30 + lean, -24, -24 + lean, 0, c1);
    veh_rect(x, y, rot, flip, -38 + lean, -28, -24 + lean, -22, c1);
    if (under) {
        for (int k = 0; k < 5; k++) {
            int age = (frame * 2 + k * 13) % 40;
            float px = x + (float)((k * 23) % 40 - 20) * s + fsin((float)(age + k) * 0.4f) * 5 * s;
            float py = y - (float)age * 4 * s;
            if (py > wy + 6)
                draw_circle((int)px, (int)py, (int)((4 + k % 3) * s) + 1, col_lerp(RGB(200, 240, 255), bg, 60));
        }
        for (int k = -6; k <= 6; k++) {
            float px = x + (float)k * 14 * s, ph = (px - veh_sx(0)) * 0.045f + (float)frame * 0.11f;
            draw_rect((int)px - 7, (int)(wy - 6 + fsin(ph) * 5), 14, 6, RGB(130, 210, 255));
        }
    }
}

static const float OUTLINE[16] = { -50, 26, -38, -28, -30, -42, 30, -42, 30, 18, 56, 23, 44, 32, -46, 32 };

const ModeDef MODE_SURFER_DEF = {
    .name = "surfer", .label = "SURFER", .portal_name = "portal_surfer",
    .corridor = 0, .half_w = PLAYER_S / 2.0f, .half_h = PLAYER_S / 2.0f, .boost = 1.0f, .inputs = IN_CROSS,
    .color = RGB(0, 130, 210),
    .step = surfer_step, .turn = surfer_turn, .draw = surfer_draw,
    .noutline = 8, .outline = OUTLINE,
};

// --- Water: the surfer floats in it; everyone else passes through.

// Deep blue with drifting ripples; the top cell of a pool gets a moving
// crest and foam (in the editor every cell does).
static void water_draw(const LevelObj *o, float x, float y, int frame)
{
    const Run *r = draw_run();
    int l = (int)x - TILE / 2, t = (int)y - TILE / 2;
    int surface = !r || !is_water(r->lv, o->x, o->y + 1);
    color_t deep = RGB(18, 64, 150), mid = RGB(28, 104, 200), light = RGB(130, 210, 255);
    draw_rect(l, t, TILE, TILE, surface ? mid : deep);
    if (surface)
        draw_rect(l, t + 40, TILE, 40, col_lerp(mid, deep, 128));
    for (int k = 0; k < 2; k++) {
        int px = (frame + o->x * 37 + o->y * 11 + k * 41) % 96 - 16;
        int w = px < 0 ? 16 + px : px > 64 ? 80 - px : 16;
        if (w > 0)
            draw_rect(l + (px < 0 ? 0 : px), t + 22 + k * 30, w, 3, col_lerp(light, deep, 150));
    }
    if (surface) {
        for (int i = 0; i < 8; i++) {
            float ph = ((float)o->x * TILE + (float)i * 10) * 0.045f + (float)frame * 0.11f;
            int wy = (int)(fsin(ph) * 5);
            draw_rect(l + i * 10, t - 6 + wy, 10, 10 - wy, light);
            if ((i + frame / 8 + o->x) % 5 == 0)
                draw_rect(l + i * 10 + 2, t - 9 + wy, 6, 4, RGB(255, 255, 255));
        }
    }
}

static void water_info(const LevelObj *o, char *buf, int n)
{
    (void)o;
    snprintf(buf, (size_t)n, "THE SURFER RIDES IT: HOLD X TO DIVE");
}

const ObjDef OBJ_WATER_DEF = {
    .name = "water", .label = "WATER", .cat = PAL_MODES, .kind = K_NONE,
    .x0 = 0, .y0 = 0, .x1 = 1, .y1 = 1,
    .draw = water_draw, .info = water_info,
};
