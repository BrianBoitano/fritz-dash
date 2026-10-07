// Fritz Dash - the magnet: holding X pulls it up to the ceiling, letting go
// drops it back to the floor. It runs in an eight cell corridor like the
// ball, with sparks where it clings and field lines while it flies over.
#include "input.h"
#include "mode.h"
#include "packb.h"

#define MAG_G   3.3f
#define MAG_MAX 26.0f

// ms[0]: it stood on something last substep. fx_t: frames since it last
// grabbed a floor or ceiling (a crackle there).
static void magnet_step(Run *r, Body *b, int held, int input)
{
    (void)input;
    int want = held ? -1 : 1;
    if (r->buffered && !run_orb_waiting(r))
        run_jumped(r);
    if (b->on_ground && !b->ms[0]) {
        b->fx_t = 0;
        r->sfx |= SFX_TICK;
        pb_sparks(r, b->x, b->y - (float)b->grav * run_half_h(b), 8, RGB(255, 240, 120), 0, 7, -3, 9, 18, 10);
    }
    b->ms[0] = b->on_ground;
    if (b->grav != want) {
        b->grav = want;
        b->on_ground = 0;
    }
    float g = (float)b->grav;
    b->vy -= MAG_G * g * (1.0f / SUB);
    if (b->vy * g < -MAG_MAX)
        b->vy = -MAG_MAX * g;
}

static void magnet_turn(Body *b)
{
    b->rot = b->on_ground ? 0 : -b->vy * 0.004f * (float)b->grav;
}

// A red horseshoe magnet with silver poles toward the surface it pulls to
// (N and S on its legs), his cube held between them, lightning at the poles
// when it clings and field lines streaming toward where it is going.
static void magnet_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot, s = veh_scale(), fy = b->grav < 0 ? -1.0f : 1.0f;
    int flip = b->grav < 0;
    color_t c2 = veh_color(VC_2), red = col_lerp(RGB(235, 30, 45), c2, 40), dark = col_scale(red, 1, 2);
    color_t silver = RGB(225, 230, 240), white = veh_color(VC_WHITE), field = RGB(150, 220, 255);
    if (!b->on_ground)
        for (int k = 0; k < 4; k++) {
            float d = (float)((frame * 7 + k * 18) % 72);
            color_t c = col_lerp(field, veh_bg(), (int)(d * 3.2f));
            for (int side = -1; side <= 1; side += 2) {
                float sx = (float)side * (35 + d * 0.25f);
                float ln[8] = { sx - 9, 46 + d, sx + 9, 46 + d, sx + 6, 52 + d, sx - 6, 52 + d };
                veh_poly(x, y, rot, flip, 4, ln, c);
            }
        }
    for (int k = 0; k < 8; k++) {
        float a0 = PI_F + (float)k * PI_F / 8, a1 = a0 + PI_F / 8;
        float arc[8] = { fcos(a0) * 47, fsin(a0) * 47 - 2, fcos(a1) * 47, fsin(a1) * 47 - 2, fcos(a1) * 25,
                         fsin(a1) * 25 - 2, fcos(a0) * 25, fsin(a0) * 25 - 2 };
        veh_poly(x, y, rot, flip, 4, arc, k == 3 || k == 4 ? col_lerp(red, white, 70) : red);
    }
    veh_rect(x, y, rot, flip, -47, -2, -25, 28, red);
    veh_rect(x, y, rot, flip, 25, -2, 47, 28, red);
    veh_rect(x, y, rot, flip, -47, -2, -41, 28, dark);
    veh_rect(x, y, rot, flip, 25, -2, 31, 28, dark);
    veh_rect(x, y, rot, flip, -48, 28, -24, 44, silver);
    veh_rect(x, y, rot, flip, 24, 28, 48, 44, silver);
    veh_rect(x, y, rot, flip, -48, 28, -24, 31, RGB(150, 155, 170));
    veh_rect(x, y, rot, flip, 24, 28, 48, 31, RGB(150, 155, 170));
    veh_pilot(x, y, rot, flip, 0, 8, 46);
    if (s > 0.7f) {
        draw_text((int)(x - 39 * s), (int)(y + (fy * 12 - 7) * s), 2, "N", white);
        draw_text((int)(x + 32 * s), (int)(y + (fy * 12 - 7) * s), 2, "S", white);
    }
    if (b->on_ground && (frame / 3) % 3 != 2)
        for (int side = -1; side <= 1; side += 2) {
            float jx = (float)((frame * 7 + side * 5) % 11) - 5;
            float z1[8] = { (float)side * 36 - 3, 44, (float)side * 36 + 3, 44, (float)side * 36 + jx + 3, 51,
                            (float)side * 36 + jx - 3, 51 };
            float z2[8] = { (float)side * 36 + jx - 3, 51, (float)side * 36 + jx + 3, 51, (float)side * 36 - jx + 3, 58,
                            (float)side * 36 - jx - 3, 58 };
            veh_poly(x, y, rot, flip, 4, z1, RGB(255, 250, 160));
            veh_poly(x, y, rot, flip, 4, z2, RGB(255, 250, 160));
        }
    if (b->fx_t < 8) {
        int rr = (int)((16.0f + (float)b->fx_t * 10) * s);
        float py = y + fy * 46 * s;
        color_t c = col_lerp(RGB(255, 255, 200), veh_bg(), b->fx_t * 30);
        for (int k = 0; k < 12; k++) {
            float a = (float)k * 0.5236f;
            draw_rect((int)(x + fcos(a) * (float)rr) - 4, (int)(py + fsin(a) * (float)rr * 0.4f) - 4, 8, 8, c);
        }
    }
}

static const float OUTLINE[16] = { -46, -40, 0, -49, 46, -40, 48, 44, 24, 44, 24, -10, -24, -10, -48, 44 };

const ModeDef MODE_MAGNET_DEF = {
    .name = "magnet", .label = "MAGNET", .portal_name = "portal_magnet", .flags = MF_GROUNDED,
    .corridor = 8, .half_w = PLAYER_S / 2.0f, .half_h = 36.0f, .boost = 0.83f, .inputs = IN_CROSS,
    .color = RGB(225, 30, 50),
    .step = magnet_step, .turn = magnet_turn, .draw = magnet_draw,
    .noutline = 8, .outline = OUTLINE,
};
