// Fritz Dash - the swing (a swingcopter): it flies in a corridor and every
// press turns its gravity round, so it swings up and down in smooth arcs.
#include "input.h"
#include "mode.h"

#define SWING_G   1.15f
#define SWING_MAX 13.0f

static void swing_step(Run *r, Body *b, int held, int input)
{
    (void)held;
    (void)input;
    if (r->buffered && !run_orb_waiting(r)) {
        b->grav = -b->grav;
        b->on_ground = 0;
        run_jumped(r);
    }
    b->vy -= SWING_G * (float)b->grav * (1.0f / SUB);
    if (b->vy > SWING_MAX) b->vy = SWING_MAX;
    if (b->vy < -SWING_MAX) b->vy = -SWING_MAX;
}

static void swing_turn(Body *b)
{
    float a = -b->vy * 0.025f;
    b->rot = a > 0.45f ? 0.45f : a < -0.45f ? -0.45f : a;
}

// A pod with his cube in a glass bubble, and a spinning rotor on the side
// gravity pulls away from (it flips when he taps).
static void swing_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot;
    int flip = b->grav < 0;
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2);
    float blade = 30.0f + fsin((float)frame * 1.3f) * 26.0f;
    veh_rect(x, y, rot, flip, -4, -46, 4, -30, col_scale(c2, 1, 2));
    veh_rect(x, y, rot, flip, -blade, -50, blade, -44, RGB(230, 230, 240));
    veh_rect(x, y, rot, flip, -10, -52, 10, -42, c2);
    static const float POD[16] = { -36, -10, -24, -30, 24, -30, 36, -10, 36, 14, 22, 30, -22, 30, -36, 14 };
    static const float GLASS[16] = { -26, -6, -18, -22, 18, -22, 26, -6, 26, 10, 16, 22, -16, 22, -26, 10 };
    veh_poly(x, y, rot, flip, 8, POD, c2);
    veh_poly(x, y, rot, flip, 8, GLASS, col_lerp(c1, RGB(10, 4, 22), 190));
    veh_rect(x, y, rot, flip, -44, 0, -36, 8, c2);
    veh_rect(x, y, rot, flip, -50, -4, -44, 12, col_scale(c2, 1, 2));
    veh_pilot(x, y, rot, flip, 0, 0, 34);
}

static const float OUTLINE[16] = { -60, -52, 60, -52, 36, -10, 36, 14, 22, 30, -22, 30, -36, 14, -36, -10 };

const ModeDef MODE_SWING_DEF = {
    .name = "swing", .label = "SWING", .portal_name = "portal_swing", .flags = MF_SLIDE,
    .corridor = 10, .half_w = PLAYER_S / 2.0f, .half_h = 30.0f, .boost = 0.72f, .inputs = IN_CROSS,
    .color = RGB(255, 225, 0),
    .step = swing_step, .turn = swing_turn, .draw = swing_draw,
    .noutline = 8, .outline = OUTLINE,
};
