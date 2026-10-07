// Fritz Dash - the shapeshifter: every press changes it between a cube and
// a ship on the spot, with a morph each time. The cube falls and rolls (it
// cannot jump); the ship flies while X is held and sinks when it is let go.
// So a press makes it a ship that climbs, and the next one drops it as a cube.
#include "input.h"
#include "mode.h"
#include "packb.h"

#define SS_G     2.56f
#define SS_FALL  30.0f
#define SS_UP    1.05f
#define SS_DOWN  0.90f
#define SS_MAX   14.0f
#define SS_MORPH 10             // frames of its own morph

static void ss_enter(Body *b)
{
    b->fx_t = SS_MORPH;
}

// ms[0]: 0 cube, 1 ship. fx_t: frames since it last changed.
static void ss_step(Run *r, Body *b, int held, int input)
{
    (void)input;
    float g = (float)b->grav;
    if (r->buffered && !run_orb_waiting(r)) {
        b->ms[0] ^= 1;
        b->fx_t = 0;
        r->buffered = 0;
        r->jumps++;
        r->sfx |= SFX_MORPH;
        if (b->ms[0])
            b->vy *= 0.5f;
        pb_sparks(r, b->x, b->y, 10, (b->t & 1) ? RGB(255, 120, 230) : RGB(120, 230, 255), 0, 8, -2, 10, 22, 12);
    }
    if (b->ms[0]) {
        b->vy += (held ? SS_UP : -SS_DOWN) * g * (1.0f / SUB);
        if (b->vy > SS_MAX) b->vy = SS_MAX;
        if (b->vy < -SS_MAX) b->vy = -SS_MAX;
    } else {
        b->vy -= SS_G * g * (1.0f / SUB);
        if (b->vy * g < -SS_FALL)
            b->vy = -SS_FALL * g;
    }
}

static void ss_turn(Body *b)
{
    if (b->ms[0]) {
        float a = -b->vy * 0.035f;
        b->rot = a > 0.6f ? 0.6f : a < -0.6f ? -0.6f : a;
    } else {
        run_turn_cube(b);
    }
}

static const color_t RAINBOW[6] = { RGB(255, 60, 60), RGB(255, 160, 0), RGB(255, 240, 0), RGB(60, 255, 90),
                                    RGB(40, 160, 255), RGB(200, 80, 255) };

// The ship form: a delta jet with a rainbow stripe and his cube under a canopy.
static void ship_form(const Body *b, float x, float y, int frame)
{
    float rot = b->rot;
    int flip = b->grav < 0;
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2);
    float fl = 14.0f + (float)(frame % 3) * 7;
    float flame[6] = { -50, 8, -50, 24, -50 - fl, 16 };
    veh_poly(x, y, rot, flip, 3, flame, RGB(255, 150, 40));
    static const float WING[8] = { -30, 14, 20, 14, -10, 40, -44, 40 };
    static const float HULL[8] = { -54, 2, 58, 12, 40, 30, -50, 28 };
    static const float FIN[6] = { -48, 4, -26, 4, -52, -24 };
    veh_poly(x, y, rot, flip, 3, FIN, c2);
    veh_poly(x, y, rot, flip, 4, WING, col_scale(c2, 2, 3));
    veh_poly(x, y, rot, flip, 4, HULL, c2);
    for (int k = 0; k < 6; k++)
        veh_rect(x, y, rot, flip, -46 + (float)k * 16, 18, -30 + (float)k * 16, 22, RAINBOW[(k + frame / 5) % 6]);
    veh_pilot(x, y, rot, flip, -4, -10, 40);
    float canopy[8] = { -24, -2, 16, -2, 22, 6, -28, 6 };
    veh_poly(x, y, rot, flip, 4, canopy, col_lerp(c1, RGB(255, 255, 255), 120));
}

static const float CUBE_PTS[8] = { -38, -38, 38, -38, 38, 38, -38, 38 };
static const float SHIP_PTS[8] = { -54, -24, 58, 12, 40, 30, -50, 40 };

// His cube or the ship, with a shimmer of rainbow round it that says it
// can change, and its own morph: a flash and pixels flying from the old
// shape to the new one while the new form grows in.
static void ss_draw(const Body *b, float x, float y, int frame)
{
    float s = veh_scale();
    int ship = b->ms[0] & 1, flip = b->grav < 0;
    color_t glow = RAINBOW[(frame / 4) % 6];
    if (b->fx_t < SS_MORPH) {
        int t = b->fx_t;
        float e = (float)t / SS_MORPH, arc = fsin(e * PI_F) * 24;
        const float *from = ship ? CUBE_PTS : SHIP_PTS, *to = ship ? SHIP_PTS : CUBE_PTS;
        if (t < 3)
            draw_circle((int)x, (int)y, (int)((float)(64 - 16 * t) * s), RGB(255, 255, 255));
        for (int k = 0; k < 16; k++) {
            int c = k / 4, d = (c + 1) % 4;
            float u = (float)(k % 4) / 4;
            float ax = from[2 * c] + (from[2 * d] - from[2 * c]) * u, ay = from[2 * c + 1] + (from[2 * d + 1] - from[2 * c + 1]) * u;
            float bx = to[2 * c] + (to[2 * d] - to[2 * c]) * u, by = to[2 * c + 1] + (to[2 * d + 1] - to[2 * c + 1]) * u;
            float px = ax + (bx - ax) * e, py = (ay + (by - ay) * e) * (flip ? -1.0f : 1.0f);
            float len = (px < 0 ? -px : px) + (py < 0 ? -py : py) + 1;
            px += px / len * arc;
            py += py / len * arc;
            int sz = (int)((float)(14 - t) * s) + 3;
            draw_rect((int)(x + px * s) - sz / 2, (int)(y + py * s) - sz / 2, sz, sz, RAINBOW[(k + t) % 6]);
        }
        if (t < SS_MORPH - 4)
            return;
        veh_set_scale(s * (0.55f + 0.45f * (float)(t - (SS_MORPH - 4)) / 4.0f));
    }
    if (ship) {
        float halo[8] = { -60, -28, 64, 10, 46, 36, -56, 46 };
        veh_poly(x, y, b->rot, flip, 4, halo, col_lerp(glow, veh_bg(), 150));
        ship_form(b, x, y, frame);
    } else {
        veh_square(x, y, b->rot, flip, 0, 0, PLAYER_S + 22 + (float)((frame / 3) % 3) * 2, col_lerp(glow, veh_bg(), 120));
        veh_pilot(x, y, b->rot, flip, 0, 0, PLAYER_S);
        for (int k = 0; k < 4; k++) {
            float a = (float)frame * 0.12f + (float)k * HALF_PI_F;
            veh_square(x, y, 0, flip, fcos(a) * 54, fsin(a) * 54, 9, RAINBOW[(k * 2 + frame / 6) % 6]);
        }
    }
    veh_set_scale(s);
}

const ModeDef MODE_SHAPESHIFTER_DEF = {
    .name = "shapeshifter", .label = "SHAPESHIFTER", .portal_name = "portal_shapeshifter", .flags = MF_SLIDE,
    .corridor = 10, .half_w = PLAYER_S / 2.0f, .half_h = 30.0f, .boost = 0.72f, .inputs = IN_CROSS,
    .color = RGB(80, 255, 200),
    .step = ss_step, .enter = ss_enter, .turn = ss_turn, .draw = ss_draw,
    .noutline = 4, .outline = SHIP_PTS,
};
