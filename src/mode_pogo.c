// Fritz Dash - the pogo stick: it bounces on every landing by itself, a
// small bounce, or a big one if X is held as it lands. A press in the air
// slams it straight down (hold on to bounce big right after).
#include "input.h"
#include "mode.h"

#define POGO_G     2.56f
#define POGO_SMALL 22.0f        // about 1.1 cells
#define POGO_BIG   38.0f        // about 3.4 cells
#define POGO_SLAM  30.0f
#define MAX_FALL   36.0f

// ms[0]: 1 while slamming. fx_t: frames since the last landing (drawing).
static void pogo_step(Run *r, Body *b, int held, int input)
{
    (void)input;
    float g = (float)b->grav, k = b->mini ? MINI_JUMP : 1.0f;
    if (b->on_ground) {
        b->vy = (held ? POGO_BIG : POGO_SMALL) * k * g;
        b->on_ground = 0;
        b->ms[0] = 0;
        b->fx_t = 0;
        b->fx_y = held ? 1.0f : 0.0f;
        if (held || r->buffered)
            run_jumped(r);
    } else if (r->buffered && !b->ms[0] && !run_orb_waiting(r)) {
        b->vy = -POGO_SLAM * g;
        b->ms[0] = 1;
        run_jumped(r);
    }
    b->vy -= POGO_G * g * (1.0f / SUB);
    if (b->vy * g < -MAX_FALL)
        b->vy = -MAX_FALL * g;
}

static void pogo_turn(Body *b)
{
    float a = b->ms[0] ? 0 : 0.12f - b->vy * 0.004f * (float)b->grav;
    b->rot += (a - b->rot) * 0.3f;
}

// His cube riding a pogo stick: handlebars, a spring that squashes on each
// landing, and a rubber foot. A slam streaks; a big bounce leaves stars.
static void pogo_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot, s = veh_scale();
    int flip = b->grav < 0;
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2), white = veh_color(VC_WHITE), bg = veh_bg();
    float sq = b->fx_t < 6 ? (float)(6 - b->fx_t) * (b->fx_y > 0 ? 2.2f : 1.4f) : 0;
    if (b->ms[0])
        for (int k = 0; k < 4; k++)
            draw_rect((int)(x + (float)(k * 14 - 21) * s), (int)(y - (60 + (float)((frame * 7 + k * 11) % 40)) * s *
                      (flip ? -1 : 1)), 4, (int)(30 * s), col_lerp(white, bg, 60 + k * 30));
    if (b->fx_t < 14 && b->fx_y > 0)
        for (int k = 0; k < 5; k++) {
            float a = (float)k * 0.6f + 0.35f, d = (12.0f + (float)b->fx_t * 6) * s;
            float px = x - (float)b->fx_t * run_speed_px(b->speed) * s * 0.5f + fcos(a + PI_F / 2) * d * 1.2f;
            float py = y + 38 * s * (flip ? -1 : 1) - fsin(a) * d * 0.5f * (flip ? -1 : 1);
            ui_star(px, py, (14.0f - (float)b->fx_t) * s, col_lerp(RGB(255, 230, 80), bg, b->fx_t * 14));
        }
    color_t steel = RGB(190, 195, 215);
    veh_rect(x, y, rot, flip, -5, -14 + sq, 5, 12, steel);
    float coil = 10 + sq * 0.6f, len = 22 - sq;
    for (int k = 0; k < 5; k++) {
        float cy = coil + len * (float)k / 5;
        float z[8] = { -16, cy, 16, cy + len / 10, 16, cy + len / 10 + 4, -16, cy + 4 };
        veh_poly(x, y, rot, flip, 4, z, k % 2 ? c1 : RGB(235, 235, 245));
    }
    veh_rect(x, y, rot, flip, -5, 30, 5, 35, steel);
    veh_rect(x, y, rot, flip, -15, 33, 15, 39, RGB(40, 40, 50));
    veh_rect(x, y, rot, flip, -20, 4 + sq * 0.6f, 20, 9 + sq * 0.6f, c2);
    veh_rect(x, y, rot, flip, -26, 2 + sq * 0.6f, -16, 8 + sq * 0.6f, RGB(40, 40, 50));
    veh_rect(x, y, rot, flip, 16, 2 + sq * 0.6f, 26, 8 + sq * 0.6f, RGB(40, 40, 50));
    veh_pilot(x, y, rot, flip, 0, -36 + sq, 46);
    veh_rect(x, y, rot, flip, -38, -14 + sq, 38, -9 + sq, steel);
    veh_rect(x, y, rot, flip, -46, -18 + sq, -34, -5 + sq, c2);
    veh_rect(x, y, rot, flip, 34, -18 + sq, 46, -5 + sq, c2);
    veh_rect(x, y, rot, flip, -46, -18 + sq, -34, -15 + sq, col_lerp(c2, white, 140));
    veh_rect(x, y, rot, flip, 34, -18 + sq, 46, -15 + sq, col_lerp(c2, white, 140));
}

static const float OUTLINE[12] = { -46, -18, -24, -60, 24, -60, 46, -18, 16, 39, -16, 39 };

const ModeDef MODE_POGO_DEF = {
    .name = "pogo", .label = "POGO", .portal_name = "portal_pogo",
    .corridor = 0, .half_w = PLAYER_S / 2.0f, .half_h = PLAYER_S / 2.0f, .boost = 1.0f, .inputs = IN_CROSS,
    .color = RGB(255, 200, 140),
    .step = pogo_step, .turn = pogo_turn, .draw = pogo_draw,
    .noutline = 6, .outline = OUTLINE,
};
