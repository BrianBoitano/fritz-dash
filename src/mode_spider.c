// Fritz Dash - the spider: a press while it stands on something snaps it
// straight to the surface opposite (floor to ceiling and back) and turns
// its gravity round. It runs in an eight cell corridor like the ball.
#include "input.h"
#include "mode.h"

#define SPIDER_G   2.2f
#define SPIDER_MAX 30.0f

static void spider_step(Run *r, Body *b, int held, int input)
{
    (void)held;
    (void)input;
    if (r->buffered && b->on_ground && !run_orb_waiting(r)) {
        float hh = run_half_h(b), to = run_surface(r, b, b->grav);
        b->fx_y = b->y;
        b->fx_t = 0;
        b->y = b->grav > 0 ? to - hh : to + hh;
        b->grav = -b->grav;
        b->vy = 0;
        b->on_ground = 0;
        run_jumped(r);
    }
    float g = (float)b->grav;
    b->vy -= SPIDER_G * g * (1.0f / SUB);
    if (b->vy * g < -SPIDER_MAX)
        b->vy = -SPIDER_MAX * g;
}

static void spider_turn(Body *b)
{
    b->rot = 0;
}

// A round body with his cube riding on top, four scuttling legs a side,
// and a streak where it last zipped.
static void spider_draw(const Body *b, float x, float y, int frame)
{
    int flip = b->grav < 0;
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2);
    if (b->fx_t < 8) {
        float y0 = veh_sy(b->fx_y), w = 18.0f * veh_scale() * (float)(8 - b->fx_t) / 8;
        float top = y0 < y ? y0 : y, bot = y0 < y ? y : y0;
        draw_rect((int)(x - w), (int)top, (int)(2 * w), (int)(bot - top), col_lerp(c2, veh_bg(), b->fx_t * 28));
        draw_rect((int)(x - w / 3), (int)top, (int)(2 * w / 3) + 1, (int)(bot - top), RGB(255, 255, 255));
    }
    for (int side = -1; side <= 1; side += 2)
        for (int k = 0; k < 4; k++) {
            float wig = b->on_ground ? fsin((float)frame * 0.7f + (float)k * 1.6f) * 6 : 0;
            float hx = (float)side * (14 + k * 6), kx = (float)side * (30 + k * 7), fx = (float)side * (24 + k * 9) + wig;
            float upper[8] = { hx - 3, 8, hx + 3, 8, kx + 3, 6 + wig * 0.3f, kx - 3, 6 + wig * 0.3f };
            float lower[8] = { kx - 3, 4, kx + 3, 4, fx + 3, 38, fx - 3, 38 };
            veh_poly(x, y, 0, flip, 4, upper, col_scale(c2, 2, 3));
            veh_poly(x, y, 0, flip, 4, lower, col_scale(c2, 2, 3));
        }
    static const float BODY[16] = { -30, 4, -22, -8, 0, -12, 22, -8, 30, 4, 22, 18, 0, 22, -22, 18 };
    static const float CORE[16] = { -24, 4, -18, -4, 0, -7, 18, -4, 24, 4, 18, 14, 0, 17, -18, 14 };
    veh_poly(x, y, 0, flip, 8, BODY, col_scale(c2, 1, 2));
    veh_poly(x, y, 0, flip, 8, CORE, c2);
    veh_rect(x, y, 0, flip, -12, 6, -4, 12, c1);
    veh_rect(x, y, 0, flip, 4, 6, 12, 12, c1);
    veh_pilot(x, y, 0, flip, 0, -22, 36);
}

static const float OUTLINE[16] = { -40, 38, -34, 0, -20, -40, 20, -40, 34, 0, 40, 38, 10, 22, -10, 22 };

const ModeDef MODE_SPIDER_DEF = {
    .name = "spider", .label = "SPIDER", .portal_name = "portal_spider", .flags = MF_GROUNDED,
    .corridor = 8, .half_w = PLAYER_S / 2.0f, .half_h = PLAYER_S / 2.0f, .boost = 0.83f, .inputs = IN_CROSS,
    .color = RGB(150, 60, 255),
    .step = spider_step, .turn = spider_turn, .draw = spider_draw,
    .noutline = 8, .outline = OUTLINE,
};
