// Fritz Dash - the twin: two of him at once in an eight cell corridor, one
// on the floor and one on the ceiling with gravity the other way. X jumps
// both (each when it stands on something, holding keeps jumping like the
// cube); each lands on its own, and if either one hits something he dies.
#include "input.h"
#include "mode.h"
#include "packb.h"

#define TWIN_G    2.56f
#define TWIN_JUMP 29.9f
#define MAX_FALL  36.0f
#define ROT_STEP  0.1348f

// mf[0], mf[1]: the other twin's y and vy; ms[0]: it stands on something.
// fx_y: its turn (drawing only).
static void twin_enter(Body *b)
{
    b->mf[0] = b->lo + b->hi - b->y;
    b->mf[1] = -b->vy;
    b->ms[0] = 0;
    b->fx_y = 0;
}

static void fall(Body *b, int held, float k)
{
    float g = (float)b->grav;
    if (b->on_ground && held) {
        b->vy = TWIN_JUMP * k * g;
        b->on_ground = 0;
    }
    b->vy -= TWIN_G * g * (1.0f / SUB);
    if (b->vy * g < -MAX_FALL)
        b->vy = -MAX_FALL * g;
}

static void twin_step(Run *r, Body *b, int held, int input)
{
    (void)input;
    float k = b->mini ? MINI_JUMP : 1.0f, hh = run_half_h(b);
    if (held && (b->on_ground || b->ms[0]))
        run_jumped(r);
    fall(b, held, k);
    Body t = *b;
    t.grav = -b->grav;
    t.y = b->mf[0];
    t.vy = b->mf[1];
    t.on_ground = b->ms[0];
    fall(&t, held, k);
    float prev = t.y;
    t.x = b->x + run_speed_px(b->speed) / SUB;
    t.y += t.vy * (1.0f / SUB);
    t.on_ground = 0;
    if (t.y - hh < b->lo) {
        t.y = b->lo + hh;
        if (t.vy < 0)
            t.vy = 0;
        t.on_ground = t.grav > 0;
    }
    if (t.y + hh > b->hi) {
        t.y = b->hi - hh;
        if (t.vy > 0)
            t.vy = 0;
        t.on_ground = t.grav < 0;
    }
    if (run_body_hits(r, &t, prev)) {
        b->y = t.y;
        r->state = RUN_DEAD;
        return;
    }
    b->mf[0] = t.y;
    b->mf[1] = t.vy;
    b->ms[0] = t.on_ground;
}

static void twin_turn(Body *b)
{
    run_turn_cube(b);
    if (!b->ms[0]) {
        b->fx_y -= ROT_STEP * (float)b->grav;
    } else {
        float q = b->fx_y / HALF_PI_F, d = (float)(int)(q + (q < 0 ? -0.5f : 0.5f)) * HALF_PI_F - b->fx_y;
        b->fx_y += d > 0.25f ? 0.25f : d < -0.25f ? -0.25f : d;
    }
    if (b->fx_y > 2 * PI_F * 8 || b->fx_y < -2 * PI_F * 8)
        b->fx_y = 0;
}

// One twin: his cube in a glowing frame with two-tone wings and a star on
// top, the frame in colour c (the floor twin his colour 1, the ceiling twin
// his colour 2).
static void one_twin(float x, float y, float rot, int flip, color_t c, int frame)
{
    float flap = fsin((float)frame * 0.5f) * 7;
    color_t light = col_lerp(c, RGB(255, 255, 255), 120), pale = col_lerp(c, RGB(255, 255, 255), 200);
    for (int side = -1; side <= 1; side += 2) {
        float sd = (float)side;
        float w1[8] = { sd * 40, -10, sd * 70, -28 - flap, sd * 76, -12 - flap, sd * 44, 12 };
        float w2[8] = { sd * 40, 0, sd * 62, -4 - flap * 0.6f, sd * 64, 8 - flap * 0.6f, sd * 44, 16 };
        veh_poly(x, y, 0, flip, 4, w1, light);
        veh_poly(x, y, 0, flip, 4, w2, pale);
    }
    veh_square(x, y, rot, flip, 0, 0, 96, col_scale(c, 1, 3));
    veh_square(x, y, rot, flip, 0, 0, 90, c);
    veh_square(x, y, rot, flip, 0, 0, 80, pale);
    veh_pilot(x, y, rot, flip, 0, 0, 74);
    float st[6] = { 0, -70, 9, -54, -9, -54 }, sb[6] = { -10, -59, 10, -59, 0, -50 };
    veh_poly(x, y, 0, flip, 3, st, (frame / 8) % 2 ? RGB(255, 240, 120) : pale);
    veh_poly(x, y, 0, flip, 3, sb, (frame / 8) % 2 ? RGB(255, 240, 120) : pale);
}

// Both twins, a beam of light joining them that pulses, and the mirror
// line between them.
static void twin_draw(const Body *b, float x, float y, int frame)
{
    float s = veh_scale(), ty = veh_sy(b->mf[0]), mid = veh_sy((b->lo + b->hi) / 2);
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2), bg = veh_bg();
    if (b->lo < b->hi && b->mf[0] > 0) {
        for (int k = -6; k <= 6; k++)
            draw_rect((int)(x + (float)k * 46 * s) - 10, (int)mid - 2, 20, 4, col_lerp(RGB(255, 255, 255), bg, 140 + (k < 0 ? -k : k) * 16));
        float top = ty < y ? ty : y, bot = ty < y ? y : ty;
        for (float py = top + 40 * s; py < bot - 40 * s; py += 18 * s) {
            int on = ((int)((py - top) / (18 * s)) + frame / 2) % 5 == 0;
            int w = on ? 10 : 6;
            draw_rect((int)x - w / 2, (int)py, w, (int)(10 * s), on ? RGB(255, 255, 255) : col_lerp(c1, c2, 128));
        }
        one_twin(x, ty, b->fx_y, b->grav > 0, c2, frame + 9);
    } else {
        float ps = s;
        veh_set_scale(ps * 0.6f);
        one_twin(x, y - 50 * ps, 0, 1, c2, frame + 9);
        one_twin(x, y + 50 * ps, 0, 0, c1, frame);
        veh_set_scale(ps);
        return;
    }
    one_twin(x, y, b->rot, b->grav < 0, c1, frame);
}

static const float OUTLINE[16] = { -76, -12, -48, -48, 0, -70, 48, -48, 76, -12, 48, 48, 0, 48, -48, 48 };

const ModeDef MODE_TWIN_DEF = {
    .name = "twin", .label = "TWIN", .portal_name = "portal_twin", .flags = MF_GROUNDED,
    .corridor = 8, .half_w = PLAYER_S / 2.0f, .half_h = PLAYER_S / 2.0f, .boost = 1.0f, .inputs = IN_CROSS,
    .color = RGB(240, 240, 250),
    .step = twin_step, .enter = twin_enter, .turn = twin_turn, .draw = twin_draw,
    .noutline = 8, .outline = OUTLINE,
};
