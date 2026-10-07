// Fritz Dash - the wave: straight diagonals, up while X is held and down
// when not (mini is twice as steep); it dies on any block, and pads and orbs
// do nothing to it.
#include "input.h"
#include "mode.h"

#define WAVE_HALF 12.0f         // the wave's small hitbox

static void wave_step(Run *r, Body *b, int held, int input)
{
    (void)r;
    (void)input;
    float v = run_speed_px(b->speed) * (b->mini ? 2.0f : 1.0f);
    b->vy = (held ? v : -v) * (float)b->grav;
}

static void wave_turn(Body *b)
{
    float a = b->mini ? 1.107f : 0.785f;
    b->rot = b->vy > 0.5f ? -a : b->vy < -0.5f ? a : 0;
}

// A dart pointing the way it flies.
static void wave_draw(const Body *b, float cx, float cy, int frame)
{
    static const float GLOW_A[6] = { 36, 0, -26, -26, -12, 0 }, GLOW_B[6] = { 36, 0, -12, 0, -26, 26 };
    static const float BODY_A[6] = { 28, 0, -20, -20, -8, 0 }, BODY_B[6] = { 28, 0, -8, 0, -20, 20 };
    static const float CORE[6] = { 14, 0, -6, -7, -6, 7 };
    (void)frame;
    float ps = veh_scale(), rot = b->rot;
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2);
    veh_set_scale(ps * 1.3f);
    veh_poly(cx, cy, rot, 0, 3, GLOW_A, col_scale(c1, 2, 5));
    veh_poly(cx, cy, rot, 0, 3, GLOW_B, col_scale(c1, 2, 5));
    veh_poly(cx, cy, rot, 0, 3, BODY_A, c1);
    veh_poly(cx, cy, rot, 0, 3, BODY_B, c1);
    veh_poly(cx, cy, rot, 0, 3, CORE, c2);
    veh_set_scale(ps);
}

// A solid zig-zag line instead of the trail he picked.
static void wave_trail(const float *tx, const float *ty)
{
    color_t c2 = veh_color(VC_2), c1 = veh_color(VC_1), bg = veh_bg();
    float w = 11.0f * veh_scale();
    for (int i = TRAIL_N - 2; i >= 0; i--) {
        float x0 = veh_sx(tx[i + 1]), y0 = veh_sy(ty[i + 1]), x1 = veh_sx(tx[i]), y1 = veh_sy(ty[i]);
        int fade = i * 256 / TRAIL_N;
        float xs[4] = { x0, x1, x1, x0 }, ys[4] = { y0 - w, y1 - w, y1 + w, y0 + w };
        draw_poly(4, xs, ys, col_lerp(c2, bg, fade));
        float ys2[4] = { y0 - w / 3, y1 - w / 3, y1 + w / 3, y0 + w / 3 };
        draw_poly(4, xs, ys2, col_lerp(c1, bg, fade / 2));
    }
}

static const float OUTLINE[8] = { 36, 0, -26, -26, -12, 0, -26, 26 };

const ModeDef MODE_WAVE_DEF = {
    .name = "wave", .label = "WAVE", .portal_name = "portal_wave", .flags = MF_FRAGILE,
    .corridor = 10, .half_w = WAVE_HALF, .half_h = WAVE_HALF, .boost = 0, .inputs = IN_CROSS,
    .color = RGB(40, 210, 255),
    .step = wave_step, .turn = wave_turn, .draw = wave_draw, .trail = wave_trail,
    .noutline = 4, .outline = OUTLINE,
};
