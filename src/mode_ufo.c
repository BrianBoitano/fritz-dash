// Fritz Dash - the UFO: every press flaps, in the air too, unless an orb takes it.
#include "input.h"
#include "mode.h"

#define UFO_G    1.35f
#define UFO_FLAP 17.0f
#define UFO_MAX  16.0f

static void ufo_step(Run *r, Body *b, int held, int input)
{
    (void)held;
    (void)input;
    float g = (float)b->grav, k = b->mini ? MINI_JUMP : 1.0f;
    if (r->buffered && !run_orb_waiting(r)) {
        b->vy = UFO_FLAP * k * g;
        b->on_ground = 0;
        run_jumped(r);
    }
    b->vy -= UFO_G * g * (1.0f / SUB);
    if (b->vy * g < -UFO_MAX)
        b->vy = -UFO_MAX * g;
}

static void ufo_turn(Body *b)
{
    float a = -b->vy * 0.012f;
    b->rot = a > 0.6f ? 0.6f : a < -0.6f ? -0.6f : a;
}

// The cube rides in a glass dome on a saucer; flipped gravity turns it upside down.
static void ufo_draw(const Body *b, float cx, float cy, int frame)
{
    static const float DOME[14] = { -30, 6, -27, -11, -17, -23, 0, -28, 17, -23, 27, -11, 30, 6 };
    static const float RIM[12] = { -64, 8, -50, -2, 50, -2, 64, 8, 50, 18, -50, 18 };
    static const float BELLY[8] = { -40, 18, 40, 18, 28, 32, -28, 32 };
    float rot = b->rot;
    int flip = b->grav < 0;
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2);
    veh_poly(cx, cy, rot, flip, 7, DOME, col_lerp(c1, RGB(10, 4, 22), 170));
    veh_pilot(cx, cy, rot, flip, 0, -8, PLAYER_S / 2.0f);
    veh_poly(cx, cy, rot, flip, 4, BELLY, col_scale(c2, 1, 2));
    veh_poly(cx, cy, rot, flip, 6, RIM, c2);
    for (int i = 0; i < 4; i++) {
        float lx = (float)i * 30.0f - 45.0f, light[8] = { lx - 6, 4, lx + 6, 4, lx + 6, 12, lx - 6, 12 };
        veh_poly(cx, cy, rot, flip, 4, light, (frame / 6 + i) % 4 ? col_scale(c1, 3, 4) : RGB(255, 255, 255));
    }
}

static const float OUTLINE[16] = { -64, 8, -30, 0, -17, -23, 17, -23, 30, 0, 64, 8, 28, 32, -28, 32 };

const ModeDef MODE_UFO_DEF = {
    .name = "ufo", .label = "UFO", .portal_name = "portal_ufo", .flags = MF_SLIDE,
    .corridor = 10, .half_w = PLAYER_S / 2.0f, .half_h = 30.0f, .boost = 0.72f, .inputs = IN_CROSS,
    .color = RGB(255, 150, 30),
    .step = ufo_step, .turn = ufo_turn, .draw = ufo_draw,
    .noutline = 8, .outline = OUTLINE,
};
