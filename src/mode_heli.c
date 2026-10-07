// Fritz Dash - the helicopter: in a ten cell corridor, holding X lifts it
// slowly and letting go lets it sink slowly. It is heavy: it speeds up and
// slows down gently, so it moves in smooth curves.
#include "input.h"
#include "mode.h"

#define HELI_UP   0.55f
#define HELI_DOWN 0.42f
#define HELI_RISE 7.5f
#define HELI_FALL 6.5f

// fx_y: 1 while the rotor works hard (drawing).
static void heli_step(Run *r, Body *b, int held, int input)
{
    (void)r;
    (void)input;
    float g = (float)b->grav;
    b->vy += (held ? HELI_UP : -HELI_DOWN) * g * (1.0f / SUB);
    b->vy *= 0.996f;
    if (b->vy * g > HELI_RISE)
        b->vy = HELI_RISE * g;
    if (b->vy * g < -HELI_FALL)
        b->vy = -HELI_FALL * g;
    b->fx_y = held ? 1.0f : 0.0f;
}

static void heli_turn(Body *b)
{
    float a = 0.1f - b->vy * 0.012f * (float)b->grav;
    a = a > 0.3f ? 0.3f : a < -0.2f ? -0.2f : a;
    b->rot += (a - b->rot) * 0.15f;
}

// A bubble cockpit with his cube in it, a tail boom with a spinning tail
// rotor, skids, and the main rotor as a blur of blades over the top.
static void heli_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot;
    int flip = b->grav < 0, hard = b->fx_y > 0;
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2), white = veh_color(VC_WHITE), dark = veh_color(VC_DARK);
    float boom[8] = { -82, -18, -20, -10, -20, 4, -82, -10 };
    veh_poly(x, y, rot, flip, 4, boom, c2);
    veh_rect(x, y, rot, flip, -86, -30, -76, -8, col_scale(c2, 2, 3));
    float tr = fsin((float)frame * 1.7f) * 14;
    veh_rect(x, y, rot, flip, -84, -20 - (tr < 0 ? -tr : tr), -78, -18 + (tr < 0 ? -tr : tr), RGB(220, 220, 235));
    veh_circle(x, y, rot, flip, -81, -19, 4, dark);
    veh_rect(x, y, rot, flip, -26, 30, 30, 34, RGB(90, 90, 105));
    veh_rect(x, y, rot, flip, -16, 22, -12, 32, RGB(90, 90, 105));
    veh_rect(x, y, rot, flip, 14, 22, 18, 32, RGB(90, 90, 105));
    float tip[6] = { 30, 30, 36, 24, 34, 34 };
    veh_poly(x, y, rot, flip, 3, tip, RGB(90, 90, 105));
    static const float CAB[16] = { -30, -14, -20, -28, 16, -28, 34, -12, 38, 6, 26, 22, -22, 22, -32, 6 };
    veh_poly(x, y, rot, flip, 8, CAB, c2);
    static const float GLASS[12] = { -4, -24, 16, -24, 30, -10, 32, 6, 22, 16, -4, 16 };
    veh_poly(x, y, rot, flip, 6, GLASS, col_lerp(c1, RGB(10, 4, 22), 170));
    veh_pilot(x, y, rot, flip, 12, -2, 30);
    float glint[8] = { 4, -22, 14, -22, 6, -10, 0, -10 };
    veh_poly(x, y, rot, flip, 4, glint, col_lerp(white, c1, 100));
    veh_rect(x, y, rot, flip, -26, 4, -6, 10, c1);
    veh_rect(x, y, rot, flip, -4, -38, 4, -28, dark);
    int spin = hard ? 2 : 1;
    float disk[8] = { -84, -41, -40, -46, 40, -46, 84, -41 };
    float disk2[8] = { -84, -41, 84, -41, 40, -37, -40, -37 };
    veh_poly(x, y, rot, flip, 4, disk, col_lerp(RGB(220, 225, 240), veh_bg(), 170));
    veh_poly(x, y, rot, flip, 4, disk2, col_lerp(RGB(220, 225, 240), veh_bg(), 190));
    for (int k = 0; k < 2; k++) {
        float ph = fsin((float)(frame * spin) * 0.9f + (float)k * 1.6f);
        float w = 16.0f + (ph < 0 ? -ph : ph) * 68;
        veh_rect(x, y, rot, flip, -w, -43, w, -39, k ? RGB(170, 175, 190) : RGB(240, 242, 250));
    }
    veh_circle(x, y, rot, flip, 0, -40, 6, c1);
    if (frame % 30 < 4)
        veh_circle(x, y, rot, flip, -84, -6, 4, RGB(255, 60, 60));
}

// Downwash: little puffs of air pushed out under it.
static void heli_trail(const float *tx, const float *ty)
{
    color_t bg = veh_bg();
    float s = veh_scale();
    for (int i = 2; i < TRAIL_N; i += 3) {
        int r = (int)((5.0f + (float)i) * s);
        color_t c = col_lerp(RGB(220, 225, 240), bg, 90 + i * 11);
        draw_circle((int)(veh_sx(tx[i]) - 30 * s), (int)(veh_sy(ty[i]) + (34 + (float)i * 2) * s), r, c);
        draw_circle((int)(veh_sx(tx[i]) + 20 * s), (int)(veh_sy(ty[i]) + (30 + (float)i * 2.5f) * s), r, c);
    }
}

static const float OUTLINE[16] = { -86, -30, -84, -42, 84, -42, 38, 6, 36, 24, 30, 34, -26, 34, -82, -10 };

const ModeDef MODE_HELI_DEF = {
    .name = "heli", .label = "HELI", .portal_name = "portal_heli", .flags = MF_SLIDE,
    .corridor = 10, .half_w = PLAYER_S / 2.0f, .half_h = 30.0f, .boost = 0.35f, .inputs = IN_CROSS,
    .color = RGB(130, 160, 60),
    .step = heli_step, .turn = heli_turn, .draw = heli_draw, .trail = heli_trail,
    .noutline = 8, .outline = OUTLINE,
};
