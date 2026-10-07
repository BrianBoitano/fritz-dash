// Fritz Dash - the robot: X jumps, and holding it keeps the boost going for
// a higher jump (a tap hops about 1.4 cells, a full hold about 3.5); like
// the cube, holding X on landing jumps again.
#include "input.h"
#include "mode.h"

#define ROBOT_G     2.56f
#define ROBOT_JUMP  21.5f
#define ROBOT_BOOST (9 * SUB)   // substeps the boost can hold its climb
#define MAX_FALL    36.0f

// ms[0]: boost substeps left (0 when X is let go or the climb stops).
static void robot_step(Run *r, Body *b, int held, int input)
{
    (void)input;
    float g = (float)b->grav, k = b->mini ? MINI_JUMP : 1.0f;
    if (b->on_ground && held) {
        b->vy = ROBOT_JUMP * k * g;
        b->on_ground = 0;
        b->ms[0] = ROBOT_BOOST;
        run_jumped(r);
    }
    if (b->ms[0] > 0 && held && b->vy * g > 0) {
        b->ms[0]--;
        return;
    }
    b->ms[0] = 0;
    b->vy -= ROBOT_G * g * (1.0f / SUB);
    if (b->vy * g < -MAX_FALL)
        b->vy = -MAX_FALL * g;
}

static void robot_turn(Body *b)
{
    float a = b->on_ground ? 0 : -b->vy * 0.006f * (float)b->grav;
    b->rot = a > 0.2f ? 0.2f : a < -0.2f ? -0.2f : a;
}

// Two legs that walk, a body in his colour 2, his cube as the head, and a
// boost flame under the feet while the boost lasts.
static void robot_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot;
    int flip = b->grav < 0;
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2), dark = veh_color(VC_DARK);
    float step = b->on_ground ? fsin((float)frame * 0.5f) * 7 : 0;
    if (b->ms[0] > 0) {
        float fl = 14.0f + (float)(frame % 3) * 6;
        float f1[6] = { -24, 38, -8, 38, -16, 38 + fl }, f2[6] = { 8, 38, 24, 38, 16, 38 + fl };
        float f3[6] = { -21, 38, -11, 38, -16, 38 + fl * 0.6f }, f4[6] = { 11, 38, 21, 38, 16, 38 + fl * 0.6f };
        veh_poly(x, y, rot, flip, 3, f1, RGB(255, 140, 20));
        veh_poly(x, y, rot, flip, 3, f2, RGB(255, 140, 20));
        veh_poly(x, y, rot, flip, 3, f3, RGB(255, 240, 80));
        veh_poly(x, y, rot, flip, 3, f4, RGB(255, 240, 80));
    }
    veh_rect(x, y, rot, flip, -22, 10, -10, 32 + step, col_scale(c2, 1, 2));
    veh_rect(x, y, rot, flip, 10, 10, 22, 32 - step, col_scale(c2, 1, 2));
    veh_rect(x, y, rot, flip, -27, 30 + step, -6, 38 + step, c2);
    veh_rect(x, y, rot, flip, 6, 30 - step, 27, 38 - step, c2);
    veh_rect(x, y, rot, flip, -34, -6, 34, 16, col_scale(c2, 2, 5));
    veh_rect(x, y, rot, flip, -30, -3, 30, 13, c2);
    veh_rect(x, y, rot, flip, -10, 1, 10, 9, dark);
    veh_rect(x, y, rot, flip, -6, 3, 6, 7, b->ms[0] > 0 ? RGB(255, 200, 40) : c1);
    veh_rect(x, y, rot, flip, -40, -2, -32, 12, c2);
    veh_rect(x, y, rot, flip, 32, -2, 40, 12, c2);
    veh_pilot(x, y, rot, flip, 0, -24, 42);
}

static const float OUTLINE[12] = { -24, -46, 24, -46, 34, -6, 30, 38, -30, 38, -34, -6 };

const ModeDef MODE_ROBOT_DEF = {
    .name = "robot", .label = "ROBOT", .portal_name = "portal_robot",
    .corridor = 0, .half_w = PLAYER_S / 2.0f, .half_h = PLAYER_S / 2.0f, .boost = 1.0f, .inputs = IN_CROSS,
    .color = RGB(190, 200, 215),
    .step = robot_step, .turn = robot_turn, .draw = robot_draw,
    .noutline = 6, .outline = OUTLINE,
};
