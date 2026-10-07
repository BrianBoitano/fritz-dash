// Fritz Dash - the jetpack: holding X gives a gentle thrust up, softer and
// slower than the ship; letting go lets him drift down.
#include "input.h"
#include "mode.h"

#define JET_UP   0.80f
#define JET_DOWN 0.70f
#define JET_MAX  10.0f

// fx_y: 1 while the jets fire (drawing only).
static void jetpack_step(Run *r, Body *b, int held, int input)
{
    (void)r;
    (void)input;
    b->vy += (held ? JET_UP : -JET_DOWN) * (float)b->grav * (1.0f / SUB);
    if (b->vy > JET_MAX) b->vy = JET_MAX;
    if (b->vy < -JET_MAX) b->vy = -JET_MAX;
    b->fx_y = held ? 1.0f : 0.0f;
}

static void jetpack_turn(Body *b)
{
    float a = -b->vy * 0.03f;
    b->rot = a > 0.35f ? 0.35f : a < -0.35f ? -0.35f : a;
}

// His cube with a jetpack on its back: twin tanks, nozzles, and flames
// while he holds X.
static void jetpack_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot;
    int flip = b->grav < 0;
    color_t c2 = veh_color(VC_2);
    if (b->fx_y > 0) {
        float fl = 26.0f + (float)(frame % 4) * 7;
        for (int k = 0; k < 2; k++) {
            float ox = -56.0f + (float)k * 18;
            float outer[6] = { ox - 8, 34, ox + 8, 34, ox, 34 + fl }, inner[6] = { ox - 4, 34, ox + 4, 34, ox, 34 + fl * 0.6f };
            veh_poly(x, y, rot, flip, 3, outer, RGB(255, 120, 20));
            veh_poly(x, y, rot, flip, 3, inner, RGB(255, 240, 90));
        }
    }
    for (int k = 0; k < 2; k++) {
        float ox = -56.0f + (float)k * 18;
        veh_rect(x, y, rot, flip, ox - 9, -26, ox + 9, 26, col_scale(c2, 1, 2));
        veh_rect(x, y, rot, flip, ox - 7, -24, ox + 7, 24, c2);
        veh_rect(x, y, rot, flip, ox - 6, 26, ox + 6, 34, RGB(90, 90, 110));
        veh_rect(x, y, rot, flip, ox - 5, -30, ox + 5, -24, RGB(255, 255, 255));
    }
    veh_rect(x, y, rot, flip, -40, -8, -30, 8, RGB(90, 90, 110));
    veh_pilot(x, y, rot, flip, 0, 0, PLAYER_S);
}

// Smoke puffs behind the jets.
static void jetpack_trail(const float *tx, const float *ty)
{
    color_t bg = veh_bg();
    float s = veh_scale();
    for (int i = TRAIL_N - 1; i >= 1; i -= 2) {
        int r = (int)((6.0f + (float)i * 1.5f) * s);
        color_t c = i < 4 ? RGB(255, 170, 60) : RGB(200, 200, 210);
        draw_circle((int)(veh_sx(tx[i]) - 48 * s), (int)(veh_sy(ty[i]) + 30 * s), r, col_lerp(c, bg, 80 + i * 12));
    }
}

static const float OUTLINE[12] = { -66, -30, 38, -38, 38, 38, -38, 38, -66, 34, -70, 0 };

const ModeDef MODE_JETPACK_DEF = {
    .name = "jetpack", .label = "JETPACK", .portal_name = "portal_jetpack", .flags = MF_SLIDE,
    .corridor = 10, .half_w = PLAYER_S / 2.0f, .half_h = 30.0f, .boost = 0.35f, .inputs = IN_CROSS,
    .color = RGB(0, 200, 170),
    .step = jetpack_step, .turn = jetpack_turn, .draw = jetpack_draw, .trail = jetpack_trail,
    .noutline = 6, .outline = OUTLINE,
};
