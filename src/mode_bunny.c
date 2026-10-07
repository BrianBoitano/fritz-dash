// Fritz Dash - the bunny: it jumps like the cube (holding X keeps jumping
// on landing), and one more press in the air jumps again with a flip and a
// puff of cloud. Landing gives the air jump back.
#include "input.h"
#include "mode.h"

#define BUNNY_G     2.56f
#define BUNNY_JUMP  29.9f
#define BUNNY_JUMP2 25.0f
#define MAX_FALL    36.0f
#define FLIP_FRAMES 18

// ms[0]: 1 once the air jump is used. fx_y, fx_t: where and when it was (drawing).
static void bunny_step(Run *r, Body *b, int held, int input)
{
    (void)input;
    float g = (float)b->grav, k = b->mini ? MINI_JUMP : 1.0f;
    if (b->on_ground)
        b->ms[0] = 0;
    if (b->on_ground && held) {
        b->vy = BUNNY_JUMP * k * g;
        b->on_ground = 0;
        run_jumped(r);
    } else if (!b->on_ground && r->buffered && !b->ms[0] && !run_orb_waiting(r)) {
        b->vy = BUNNY_JUMP2 * k * g;
        b->ms[0] = 1;
        b->fx_y = b->y;
        b->fx_t = 0;
        run_jumped(r);
    }
    b->vy -= BUNNY_G * g * (1.0f / SUB);
    if (b->vy * g < -MAX_FALL)
        b->vy = -MAX_FALL * g;
}

static void bunny_turn(Body *b)
{
    if (b->ms[0] && b->fx_t < FLIP_FRAMES && !b->on_ground)
        b->rot = -(float)b->fx_t * (2 * PI_F / FLIP_FRAMES) * (float)b->grav;
    else
        b->rot = 0;
}

// His cube with long ears, little feet and a fluffy tail; a puff of cloud
// where the air jump was.
static void bunny_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot, s = veh_scale();
    int flip = b->grav < 0;
    color_t c1 = veh_color(VC_1), white = veh_color(VC_WHITE);
    if (b->fx_t < 16) {
        float px = x - (float)b->fx_t * run_speed_px(b->speed), py = veh_sy(b->fx_y) + 34 * s;
        int r = (int)((14.0f + (float)b->fx_t * 2) * s);
        color_t c = col_lerp(white, veh_bg(), b->fx_t * 14);
        draw_circle((int)px - r, (int)py, r, c);
        draw_circle((int)px + r, (int)py, r, c);
        draw_circle((int)px, (int)py - r / 2, r + 4, c);
    }
    float wob = b->on_ground ? 0 : fsin((float)frame * 0.4f) * 3;
    static const float EAR_L[8] = { -26, -34, -12, -34, -14, -82, -24, -82 };
    static const float EAR_R[8] = { 8, -34, 22, -34, 26, -80, 16, -82 };
    float el[8], er[8];
    for (int i = 0; i < 8; i++) {
        el[i] = EAR_L[i] + (i % 2 == 0 && i >= 4 ? -wob : 0);
        er[i] = EAR_R[i] + (i % 2 == 0 && i >= 4 ? wob : 0);
    }
    veh_poly(x, y, rot, flip, 4, el, c1);
    veh_poly(x, y, rot, flip, 4, er, c1);
    veh_rect(x, y, rot, flip, -22, -76, -16, -38, RGB(255, 170, 210));
    veh_rect(x, y, rot, flip, 13, -74, 19, -38, RGB(255, 170, 210));
    veh_circle(x, y, rot, flip, -40, 14, 13, white);
    veh_rect(x, y, rot, flip, -30, 32, -6, 44, c1);
    veh_rect(x, y, rot, flip, 6, 32, 30, 44, c1);
    veh_pilot(x, y, rot, flip, 0, 0, PLAYER_S);
}

static const float OUTLINE[16] = { -38, -38, -24, -82, -12, -38, 16, -82, 26, -38, 38, -38, 38, 44, -38, 44 };

const ModeDef MODE_BUNNY_DEF = {
    .name = "bunny", .label = "BUNNY", .portal_name = "portal_bunny",
    .corridor = 0, .half_w = PLAYER_S / 2.0f, .half_h = PLAYER_S / 2.0f, .boost = 1.0f, .inputs = IN_CROSS,
    .color = RGB(255, 160, 220),
    .step = bunny_step, .turn = bunny_turn, .draw = bunny_draw,
    .noutline = 8, .outline = OUTLINE,
};
