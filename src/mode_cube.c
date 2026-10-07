// Fritz Dash - the cube: holding X keeps jumping whenever it stands on something.
#include "input.h"
#include "mode.h"

#define CUBE_G    2.56f
#define CUBE_JUMP 29.9f
#define MAX_FALL  36.0f

static void cube_step(Run *r, Body *b, int held, int input)
{
    (void)input;
    float g = (float)b->grav, k = b->mini ? MINI_JUMP : 1.0f;
    if (b->on_ground && held) {
        b->vy = CUBE_JUMP * k * g;
        b->on_ground = 0;
        run_jumped(r);
    }
    b->vy -= CUBE_G * g * (1.0f / SUB);
    if (b->vy * g < -MAX_FALL)
        b->vy = -MAX_FALL * g;
}

static void cube_draw(const Body *b, float x, float y, int frame)
{
    (void)frame;
    run_draw_cube(x, y, b->rot);
}

static const float OUTLINE[8] = { -38, -38, 38, -38, 38, 38, -38, 38 };

const ModeDef MODE_CUBE_DEF = {
    .name = "cube", .label = "CUBE", .portal_name = "portal_cube",
    .corridor = 0, .half_w = PLAYER_S / 2.0f, .half_h = PLAYER_S / 2.0f, .boost = 1.0f, .inputs = IN_CROSS,
    .color = RGB(90, 255, 40),
    .step = cube_step, .turn = run_turn_cube, .draw = cube_draw,
    .noutline = 4, .outline = OUTLINE,
};
