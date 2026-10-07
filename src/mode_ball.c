// Fritz Dash - the ball: X flips gravity while it rolls on a floor or
// ceiling; held, it flips again on every landing.
#include "input.h"
#include "mode.h"

#define BALL_G    1.75f
#define BALL_KICK 6.0f          // speed towards the new floor when the ball flips
#define BALL_MAX  30.0f

static void ball_step(Run *r, Body *b, int held, int input)
{
    (void)input;
    float g = (float)b->grav;
    if (b->on_ground && held) {
        b->grav = -b->grav;
        g = -g;
        b->vy = -BALL_KICK * g;
        b->on_ground = 0;
        run_jumped(r);
    }
    b->vy -= BALL_G * g * (1.0f / SUB);
    if (b->vy * g < -BALL_MAX)
        b->vy = -BALL_MAX * g;
}

static void ball_turn(Body *b)
{
    b->rot += run_speed_px(b->speed) / run_half_w(b) * (float)b->grav;
    run_wrap_rot(b);
}

// A wheel in his colours whose spokes show it rolling, his cube in the hub.
static void ball_draw(const Body *b, float cx, float cy, int frame)
{
    (void)frame;
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2);
    float ps = veh_scale();
    int x = (int)cx, y = (int)cy, r = (int)(40 * ps);
    draw_circle(x, y, r + (int)(8 * ps), col_scale(c1, 2, 5));
    draw_circle(x, y, r, c1);
    draw_circle(x, y, r - (int)(9 * ps), col_scale(c1, 1, 8));
    for (int k = 0; k < 2; k++)
        veh_rect(cx, cy, b->rot + (float)k * 1.5707963f, 0, -28, -6, 28, 6, c2);
    draw_circle(x, y, (int)(21 * ps), col_scale(c1, 1, 8));
    veh_pilot(cx, cy, b->rot, 0, 0, 0, 30);
}

static const float OUTLINE[16] = { -46, -19, -19, -46, 19, -46, 46, -19, 46, 19, 19, 46, -19, 46, -46, 19 };

const ModeDef MODE_BALL_DEF = {
    .name = "ball", .label = "BALL", .portal_name = "portal_ball", .flags = MF_GROUNDED,
    .corridor = 8, .half_w = PLAYER_S / 2.0f, .half_h = PLAYER_S / 2.0f, .boost = 0.83f, .inputs = IN_CROSS,
    .color = RGB(255, 70, 50),
    .step = ball_step, .turn = ball_turn, .draw = ball_draw,
    .noutline = 8, .outline = OUTLINE,
};
