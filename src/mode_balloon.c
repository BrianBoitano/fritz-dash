// Fritz Dash - the balloon: a hot-air balloon in a ten cell corridor. It
// floats up on its own; holding X lets air out and it sinks. It is slow and
// floaty, and spikes pop it.
#include "input.h"
#include "mode.h"

#define BAL_UP   0.8f
#define BAL_SINK 1.7f
#define BAL_RISE 7.5f
#define BAL_FALL 9.0f

// fx_y: 1 while the burner is lit (drawing).
static void balloon_step(Run *r, Body *b, int held, int input)
{
    (void)r;
    (void)input;
    float g = (float)b->grav;
    b->vy += (held ? -BAL_SINK : BAL_UP) * g * (1.0f / SUB);
    b->vy *= 0.997f;
    if (b->vy * g > BAL_RISE)
        b->vy = BAL_RISE * g;
    if (b->vy * g < -BAL_FALL)
        b->vy = -BAL_FALL * g;
    b->fx_y = held ? 0.0f : 1.0f;
}

static void balloon_turn(Body *b)
{
    b->rot = fsin((float)b->t * 0.06f) * 0.07f - b->vy * 0.008f * (float)b->grav;
}

// A striped envelope in his two colours, ropes down to a wicker basket
// with his cube in it, and the burner's flame while it rises (letting go
// of X); holding X opens the vent at the top instead.
static void balloon_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot;
    int flip = b->grav < 0;
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2), white = veh_color(VC_WHITE);
    static const float ENV[16] = { -48, -36, -36, -64, 0, -76, 36, -64, 48, -36, 40, -10, 16, 8, -16, 8 };
    static const float MID[16] = { -20, -36, -14, -68, 0, -76, 14, -68, 20, -36, 16, -10, 7, 8, -7, 8 };
    veh_poly(x, y, rot, flip, 8, ENV, c1);
    float l1[8] = { -40, -60, -30, -64, -34, -12, -42, -18 }, l2[8] = { 30, -64, 40, -60, 42, -18, 34, -12 };
    veh_poly(x, y, rot, flip, 4, l1, c2);
    veh_poly(x, y, rot, flip, 4, l2, c2);
    veh_poly(x, y, rot, flip, 8, MID, c2);
    float band[8] = { -48, -38, 48, -38, 46, -30, -46, -30 };
    veh_poly(x, y, rot, flip, 4, band, col_scale(c2, 3, 4));
    float shine[8] = { -36, -52, -26, -64, -18, -60, -28, -44 };
    veh_poly(x, y, rot, flip, 4, shine, col_lerp(c1, white, 160));
    if (b->fx_y == 0) {
        veh_rect(x, y, rot, flip, -9, -80, 9, -75, col_scale(c1, 1, 2));
        float puff = (float)(frame % 10);
        veh_circle(x, y, rot, flip, (float)((frame / 10) % 3 - 1) * 7, -84 - puff * 2, 5 + puff * 0.5f,
                   col_lerp(white, veh_bg(), 90 + (int)puff * 14));
    }
    veh_rect(x, y, rot, flip, -16, 8, -14, 22, RGB(220, 200, 160));
    veh_rect(x, y, rot, flip, 14, 8, 16, 22, RGB(220, 200, 160));
    veh_rect(x, y, rot, flip, -7, 6, 7, 12, RGB(90, 90, 105));
    if (b->fx_y > 0) {
        float fl = 12.0f + (float)(frame % 3) * 5;
        float fire[6] = { -6, 12 + fl * 0.2f, 6, 12 + fl * 0.2f, 0, 12 - fl };
        float core[6] = { -3, 12, 3, 12, 0, 12 - fl * 0.6f };
        veh_poly(x, y, rot, flip, 3, fire, RGB(255, 140, 30));
        veh_poly(x, y, rot, flip, 3, core, RGB(255, 240, 120));
    }
    veh_pilot(x, y, rot, flip, 0, 24, 36);
    veh_rect(x, y, rot, flip, -22, 30, 22, 46, RGB(150, 100, 50));
    for (int k = 0; k < 5; k++)
        veh_rect(x, y, rot, flip, -20.0f + (float)k * 9, 32, -16.0f + (float)k * 9, 44, RGB(190, 140, 80));
    veh_rect(x, y, rot, flip, -24, 28, 24, 32, RGB(110, 70, 35));
}

static const float OUTLINE[16] = { -48, -36, -36, -64, 0, -76, 36, -64, 48, -36, 24, 28, 22, 46, -22, 46 };

const ModeDef MODE_BALLOON_DEF = {
    .name = "balloon", .label = "BALLOON", .portal_name = "portal_balloon", .flags = MF_SLIDE,
    .corridor = 10, .half_w = 34.0f, .half_h = 44.0f, .boost = 0.5f, .inputs = IN_CROSS,
    .color = RGB(255, 110, 120),
    .step = balloon_step, .turn = balloon_turn, .draw = balloon_draw,
    .noutline = 8, .outline = OUTLINE,
};
