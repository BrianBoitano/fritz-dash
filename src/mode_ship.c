// Fritz Dash - the ship: holding X flies it up, letting go lets it sink.
#include "input.h"
#include "mode.h"

#define SHIP_UP   1.05f
#define SHIP_DOWN 0.90f
#define SHIP_MAX  14.0f

static void ship_step(Run *r, Body *b, int held, int input)
{
    (void)r;
    (void)input;
    b->vy += (held ? SHIP_UP : -SHIP_DOWN) * (float)b->grav * (1.0f / SUB);
    if (b->vy > SHIP_MAX) b->vy = SHIP_MAX;
    if (b->vy < -SHIP_MAX) b->vy = -SHIP_MAX;
}

static void ship_turn(Body *b)
{
    float a = -b->vy * 0.035f;
    b->rot = a > 0.6f ? 0.6f : a < -0.6f ? -0.6f : a;
}

// The hull in his colour 2 (five styles in Customize), his cube in the cockpit.
static void ship_draw(const Body *b, float cx, float cy, int frame)
{
    static const float ROCKET[3][8] = { { -52, -2, 54, 6, 38, 32, -48, 32 }, { -45, 3, 46, 9, 33, 26, -41, 26 },
                                        { -34, 10, 31, 13, 25, 20, -32, 20 } };
    static const float JET[3][8] = { { -56, 0, 60, 10, 36, 28, -50, 28 }, { -48, 4, 52, 12, 30, 24, -44, 24 },
                                     { -36, 10, 34, 14, 22, 19, -34, 19 } };
    static const float SAUCER[3][8] = { { -62, 10, 62, 10, 44, 34, -44, 34 }, { -54, 13, 54, 13, 38, 30, -38, 30 },
                                        { -40, 18, 40, 18, 28, 25, -28, 25 } };
    static const float SHARK[3][8] = { { -54, -2, 58, 12, 30, 34, -50, 30 }, { -46, 3, 50, 13, 26, 28, -44, 25 },
                                       { -34, 11, 34, 15, 20, 21, -32, 20 } };
    static const float STAR[3][8] = { { -60, -2, 68, 12, 40, 30, -52, 30 }, { -52, 3, 60, 13, 35, 25, -46, 25 },
                                      { -30, 10, 34, 15, 22, 20, -28, 20 } };
    static const float (*const HULLS[5])[8] = { ROCKET, JET, SAUCER, SHARK, STAR };
    (void)frame;
    float rot = b->rot;
    int flip = b->grav < 0;
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2);
    int style = veh_look(LOOK_SHIP) % LOOK_COUNT[LOOK_SHIP];
    const float (*h)[8] = HULLS[style];
    if (style == 1) {
        float wing[6] = { -24, 14, 12, 14, -36, 40 };
        veh_poly(cx, cy, rot, flip, 3, wing, col_scale(c2, 2, 3));
    } else if (style == 3) {
        float fin[6] = { -40, 2, -18, 2, -46, -26 };
        veh_poly(cx, cy, rot, flip, 3, fin, c2);
    } else if (style == 4) {
        // Swept wings and twin tail fins, with a glowing engine.
        float wing[8] = { -34, 18, 8, 18, -44, 50, -58, 50 }, fin[8] = { -54, 2, -34, 2, -58, -28, -66, -28 };
        float fin2[8] = { -46, 4, -30, 4, -46, -16, -52, -16 }, flame[6] = { -54, 8, -54, 24, -76, 16 };
        veh_poly(cx, cy, rot, flip, 4, wing, col_scale(c2, 2, 3));
        veh_poly(cx, cy, rot, flip, 4, fin, c2);
        veh_poly(cx, cy, rot, flip, 4, fin2, col_scale(c1, 3, 4));
        veh_poly(cx, cy, rot, flip, 3, flame, RGB(255, 235, 40));
    }
    veh_poly(cx, cy, rot, flip, 4, h[0], col_scale(c2, 2, 5));
    veh_poly(cx, cy, rot, flip, 4, h[1], c2);
    veh_poly(cx, cy, rot, flip, 4, h[2], veh_color(VC_DARK));
    veh_pilot(cx, cy, rot, flip, style == 2 ? 0 : -8, -16, 38);
}

static const float OUTLINE[8] = { -52, -10, 54, 6, 38, 32, -48, 32 };

const ModeDef MODE_SHIP_DEF = {
    .name = "ship", .label = "SHIP", .portal_name = "portal_ship", .flags = MF_SLIDE,
    .corridor = 10, .half_w = PLAYER_S / 2.0f, .half_h = 24.0f, .boost = 0.35f, .inputs = IN_CROSS,
    .color = RGB(255, 50, 200),
    .step = ship_step, .turn = ship_turn, .draw = ship_draw,
    .noutline = 4, .outline = OUTLINE,
};
