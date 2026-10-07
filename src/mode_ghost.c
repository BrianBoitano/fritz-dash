// Fritz Dash - the ghost: holding X turns him see-through, and he floats
// gently up and passes through GHOST WALLS (spikes still hurt); letting go
// makes him solid again and he drifts down. Being see-through uses up a
// meter, which fills again while he stands on something.
#include <stdio.h>

#include "input.h"
#include "mode.h"
#include "packb.h"

#define GHOST_G    1.9f
#define GHOST_FALL 22.0f
#define GHOST_UP   1.7f
#define GHOST_RISE 7.0f
#define GHOST_FULL (90 * SUB)   // substeps see-through on a full meter
#define GHOST_FILL 3            // meter back each substep standing on something

static void ghost_enter(Body *b)
{
    b->ms[0] = GHOST_FULL;
}

// ms[0]: the meter; ms[1]: 1 while see-through; ms[2]: 1 once the meter ran
// out with X held (he must let go first). fx_t: frames since he changed.
static void ghost_step(Run *r, Body *b, int held, int input)
{
    (void)input;
    float g = (float)b->grav;
    if (!held)
        b->ms[2] = 0;
    if (b->ms[0] <= 0)
        b->ms[2] = 1;
    int see = held && !b->ms[2];
    if (see != b->ms[1]) {
        b->ms[1] = see;
        b->fx_t = 0;
        if (see)
            r->sfx |= SFX_WHOO;
        pb_sparks(r, b->x, b->y, 6, see ? RGB(200, 190, 255) : RGB(255, 255, 255), 0, 5, -2, 6, 24, 12);
    }
    if (see) {
        b->ms[0]--;
        b->vy += GHOST_UP * g * (1.0f / SUB);
        if (b->vy * g > GHOST_RISE)
            b->vy = GHOST_RISE * g;
        return;
    }
    if (b->on_ground && b->ms[0] < GHOST_FULL)
        b->ms[0] = b->ms[0] + GHOST_FILL > GHOST_FULL ? GHOST_FULL : b->ms[0] + GHOST_FILL;
    b->vy -= GHOST_G * g * (1.0f / SUB);
    if (b->vy * g < -GHOST_FALL)
        b->vy = -GHOST_FALL * g;
}

static void ghost_turn(Body *b)
{
    float a = b->on_ground ? 0 : -b->vy * 0.012f * (float)b->grav;
    b->rot = a > 0.25f ? 0.25f : a < -0.25f ? -0.25f : a;
}

// A ghost's sheet round his cube with a wavy hem and little arms. See-through,
// the sheet fades to the sky and only his face shows, floating in it; a
// meter over his head while it is not full.
static void ghost_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot, s = veh_scale();
    int flip = b->grav < 0, see = b->ms[1];
    color_t bg = veh_bg(), c2 = veh_color(VC_2);
    color_t sheet = see ? col_lerp(RGB(215, 205, 255), bg, 120) : RGB(246, 246, 255);
    color_t shade = see ? RGB(235, 225, 255) : RGB(200, 200, 230);
    float wave = (float)frame * 0.3f;
    static const float DOME[16] = { -40, 6, -38, -18, -26, -36, -8, -44, 8, -44, 26, -36, 38, -18, 40, 6 };
    veh_poly(x, y, rot, flip, 8, DOME, shade);
    float body[8] = { -40, 4, 40, 4, 38, 30, -38, 30 };
    veh_poly(x, y, rot, flip, 4, body, shade);
    for (int k = 0; k < 4; k++) {
        float x0 = -40.0f + (float)k * 20, d = fsin(wave + (float)k * 1.6f) * 6;
        float hem[6] = { x0, 28, x0 + 20, 28, x0 + 10, 44 + d };
        veh_poly(x, y, rot, flip, 3, hem, shade);
    }
    static const float DOME_IN[16] = { -35, 6, -33, -16, -23, -31, -7, -38, 7, -38, 23, -31, 33, -16, 35, 6 };
    veh_poly(x, y, rot, flip, 8, DOME_IN, sheet);
    float body_in[8] = { -35, 4, 35, 4, 33, 28, -33, 28 };
    veh_poly(x, y, rot, flip, 4, body_in, sheet);
    float arm = fsin(wave * 0.7f) * 5;
    float al[8] = { -36, 4, -54, -6 + arm, -58, 2 + arm, -36, 16 };
    float ar[8] = { 36, 4, 54, -6 - arm, 58, 2 - arm, 36, 16 };
    veh_poly(x, y, rot, flip, 4, al, sheet);
    veh_poly(x, y, rot, flip, 4, ar, sheet);
    if (see) {
        for (int k = 0; k < 4; k++) {
            float a = (float)frame * 0.15f + (float)k * HALF_PI_F;
            veh_circle(x, y, 0, flip, fcos(a) * 46, fsin(a) * 30 - 6, (float)(3 + (frame / 5 + k) % 3), RGB(255, 255, 255));
        }
        const Run *r = draw_run();
        int face = r ? look_face(&r->look) : veh_look(LOOK_FACE);
        run_draw_face(x, y - (flip ? -6.0f : 6.0f) * s, 0.8f, rot, face);
    } else {
        veh_pilot(x, y, rot, flip, 0, -8, 50);
        veh_circle(x, y, rot, flip, -24, 18, 5, RGB(255, 170, 200));
        veh_circle(x, y, rot, flip, 24, 18, 5, RGB(255, 170, 200));
    }
    if (b->ms[0] < GHOST_FULL) {
        float f = (float)b->ms[0] / GHOST_FULL;
        int w = (int)(70 * s), h = (int)(10 * s), bx = (int)x - w / 2, by = (int)(y - (flip ? -64.0f : 64.0f) * s) - h / 2;
        draw_rect(bx - 3, by - 3, w + 6, h + 6, RGB(30, 20, 60));
        draw_rect(bx, by, (int)((float)w * f), h, f < 0.25f && (frame / 4) % 2 ? RGB(255, 90, 90) : col_lerp(RGB(190, 170, 255), c2, 60));
    }
    if (b->fx_t < 10) {
        color_t c = col_lerp(RGB(230, 220, 255), bg, b->fx_t * 24);
        for (int k = 0; k < 10; k++) {
            float a = (float)k * 0.628f + (float)b->fx_t * 0.15f, rr = (30.0f + (float)b->fx_t * 7) * s;
            draw_rect((int)(x + fcos(a) * rr) - 4, (int)(y + fsin(a) * rr) - 4, 8, 8, c);
        }
    }
}

// Wisps that wave behind him, fainter while he is solid.
static void ghost_trail(const float *tx, const float *ty)
{
    const Run *r = draw_run();
    color_t bg = veh_bg();
    float s = veh_scale();
    int see = r && r->b.mode == MODE_GHOST && r->b.ms[1];
    for (int i = 1; i < TRAIL_N; i++) {
        float wy = fsin((float)i * 0.9f + (float)(r ? r->frame : 0) * 0.25f) * 10 * s;
        int sz = (int)((float)(22 - i) * s);
        draw_rect((int)(veh_sx(tx[i]) - 30 * s) - sz / 2, (int)(veh_sy(ty[i]) + wy) - sz / 2, sz, sz,
                  col_lerp(see ? RGB(190, 170, 255) : RGB(235, 235, 255), bg, 110 + i * 10));
    }
}

static const float OUTLINE[16] = { -58, 2, -38, -18, -8, -44, 8, -44, 38, -18, 58, 2, 38, 44, -38, 44 };

const ModeDef MODE_GHOST_DEF = {
    .name = "ghost", .label = "GHOST", .portal_name = "portal_ghost",
    .corridor = 0, .half_w = 34.0f, .half_h = 34.0f, .boost = 0.8f, .inputs = IN_CROSS,
    .color = RGB(185, 160, 255),
    .step = ghost_step, .enter = ghost_enter, .turn = ghost_turn, .draw = ghost_draw, .trail = ghost_trail,
    .noutline = 8, .outline = OUTLINE,
};

// --- The ghost wall -----------------------------------------------------------

static int see_through(const Run *r)
{
    return r->b.mode == MODE_GHOST && r->b.ms[1];
}

static int ghost_wall_now(const Run *r, const LevelObj *o)
{
    (void)o;
    return see_through(r) ? K_NONE : K_SOLID;
}

// Purple stone with wisps drifting up it and a pair of spooky eyes; while
// he is see-through it fades to a dashed outline.
static void ghost_wall_draw(const LevelObj *o, float x, float y, int frame)
{
    const Run *r = draw_run();
    int x0 = (int)x - 40, y0 = (int)y - 40;
    color_t edge = RGB(170, 120, 255);
    if (r && see_through(r)) {
        color_t c = col_lerp(edge, veh_bg(), 120);
        for (int k = 0; k < 80; k += 20) {
            draw_rect(x0 + k, y0, 10, 4, c);
            draw_rect(x0 + k + 10, y0 + 76, 10, 4, c);
            draw_rect(x0, y0 + k + 10, 4, 10, c);
            draw_rect(x0 + 76, y0 + k, 4, 10, c);
        }
        return;
    }
    draw_rect(x0, y0, 80, 80, edge);
    draw_rect(x0 + 5, y0 + 5, 70, 70, RGB(60, 30, 110));
    draw_rect(x0 + 9, y0 + 9, 62, 62, RGB(84, 46, 150));
    for (int k = 0; k < 3; k++) {
        int ph = (frame + k * 23 + o->x * 7) % 60, wx = x0 + 18 + k * 22 + (int)(fsin((float)(frame + k * 30) * 0.12f) * 4);
        draw_rect(wx, y0 + 66 - ph, 6, 10, col_lerp(RGB(200, 170, 255), RGB(84, 46, 150), ph * 4));
    }
    if ((o->x + o->y) % 3 == 0 && (frame / 50 + o->x) % 6) {
        draw_rect(x0 + 24, y0 + 30, 10, 14, RGB(230, 220, 255));
        draw_rect(x0 + 46, y0 + 30, 10, 14, RGB(230, 220, 255));
        draw_rect(x0 + 27, y0 + 36, 5, 6, RGB(40, 20, 80));
        draw_rect(x0 + 49, y0 + 36, 5, 6, RGB(40, 20, 80));
    }
}

static void ghost_wall_info(const LevelObj *o, char *buf, int n)
{
    (void)o;
    snprintf(buf, (size_t)n, "THE GHOST GOES THROUGH IT");
}

const ObjDef OBJ_GHOST_WALL_DEF = {
    .name = "ghost_wall", .label = "GHOST WALL", .cat = PAL_MODES, .kind = K_SOLID,
    .x0 = 0, .y0 = 0, .x1 = 1, .y1 = 1,
    .draw = ghost_wall_draw, .info = ghost_wall_info, .kind_now = ghost_wall_now,
};
