// Fritz Dash - the frog: hold X on the ground to crouch and charge, let go
// to leap. The charge goes in three clear steps (a hop, a jump, a big leap)
// that each last 12 frames, so the timing is fair; pips over him show it.
#include "input.h"
#include "mode.h"

#define FROG_G      2.56f
#define MAX_FALL    36.0f
#define CHARGE_STEP (12 * SUB)  // substeps of holding for each charge step
#define COYOTE      (6 * SUB)   // a leap still works this soon after running off an edge

static const float LEAP[3] = { 23.5f, 32.0f, 42.0f };  // about 1.3, 2.5 and 4.2 cells high

static int charge_step(int held_for)
{
    return held_for < CHARGE_STEP ? 0 : held_for < 2 * CHARGE_STEP ? 1 : 2;
}

// ms[0]: substeps X has been held charging (0: not charging); ms[1]:
// substeps since it last stood on something.
static void frog_step(Run *r, Body *b, int held, int input)
{
    (void)input;
    float g = (float)b->grav, k = b->mini ? MINI_JUMP : 1.0f;
    if (b->on_ground)
        b->ms[1] = 0;
    else if (b->ms[1] < COYOTE)
        b->ms[1]++;
    int footing = b->on_ground || b->ms[1] < COYOTE;
    if (held && (b->ms[0] || b->on_ground)) {
        if (b->ms[0] < 3 * CHARGE_STEP)
            b->ms[0]++;
        if (!footing)
            b->ms[0] = 0;
    } else if (!held && b->ms[0]) {
        if (footing) {
            b->vy = LEAP[charge_step(b->ms[0])] * k * g;
            b->on_ground = 0;
            b->ms[1] = COYOTE;
            b->fx_t = 0;
            b->fx_y = b->y;
            run_jumped(r);
        }
        b->ms[0] = 0;
    }
    b->vy -= FROG_G * g * (1.0f / SUB);
    if (b->vy * g < -MAX_FALL)
        b->vy = -MAX_FALL * g;
}

static void frog_turn(Body *b)
{
    float a = b->on_ground ? 0 : -b->vy * 0.008f * (float)b->grav;
    b->rot = a > 0.3f ? 0.3f : a < -0.3f ? -0.3f : a;
}

// A big frog in his colour 2 with his cube riding on its back. Crouching
// squashes it down onto its feet and folds the legs; in a leap the back
// legs stretch out. Three pips over his head light up as the charge grows.
static void frog_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot, s = veh_scale();
    int flip = b->grav < 0;
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2), white = veh_color(VC_WHITE), dark = veh_color(VC_DARK);
    color_t belly = col_lerp(c2, RGB(255, 255, 230), 130), deep = col_scale(c2, 1, 2);
    int charging = b->ms[0] > 0, step = charge_step(b->ms[0]);
    float sq = charging ? 0.06f + 0.07f * (float)step + (step == 2 && frame % 4 < 2 ? 0.02f : 0) : 0;
    float f = 1.0f - sq;
#define SQ(v) (36.0f - (36.0f - (v)) * f)
    float stretch = b->on_ground ? 0 : b->vy * (float)b->grav > 0 ? 1.0f : 0.4f;
    if (b->fx_t < 12) {
        float r0 = (6.0f + (float)b->fx_t * 4) * s, py = veh_sy(b->fx_y) + 34 * s * (flip ? -1 : 1);
        float px = x - (float)b->fx_t * run_speed_px(b->speed) * s;
        color_t dust = col_lerp(white, veh_bg(), 70 + b->fx_t * 15);
        for (int k = 0; k < 3; k++) {
            draw_circle((int)(px - (14 + k * 16) * s - (float)b->fx_t * 3), (int)py, (int)r0, dust);
            draw_circle((int)(px + (14 + k * 16) * s + (float)b->fx_t * 3), (int)py, (int)r0, dust);
        }
    }
    float thigh[8] = { -30, SQ(8), -6, SQ(6), -10, 32, -38, 32 };
    float foot[8] = { -50, 33, -12, 33, -10, 38, -52, 38 };
    if (stretch > 0) {
        float lx = -30 - 30 * stretch;
        float t2[8] = { -30, 6, -8, 4, lx + 6, 30 + 6 * stretch, lx - 4, 22 + 6 * stretch };
        float f2[8] = { lx - 22, 26 + 8 * stretch, lx, 30 + 6 * stretch, lx, 36 + 6 * stretch, lx - 24, 32 + 8 * stretch };
        for (int i = 0; i < 8; i++) {
            thigh[i] = t2[i];
            foot[i] = f2[i];
        }
    }
    veh_poly(x, y, rot, flip, 4, thigh, deep);
    veh_poly(x, y, rot, flip, 4, foot, deep);
    static const float BODY[16] = { -44, 4, -30, -22, 8, -28, 36, -18, 46, 4, 38, 32, -10, 36, -38, 30 };
    float body[16];
    for (int i = 0; i < 16; i += 2) {
        body[i] = BODY[i] * (1.0f + sq * 0.5f);
        body[i + 1] = SQ(BODY[i + 1]) - stretch * 3;
    }
    veh_poly(x, y, rot, flip, 8, body, c2);
    float bel[8] = { -20, SQ(18), 32, SQ(10), 38, SQ(26), -16, SQ(33) };
    veh_poly(x, y, rot, flip, 4, bel, belly);
    static const float SPOTS[3][2] = { { -26, -2 }, { -12, -10 }, { 6, -10 } };
    for (int k = 0; k < 3; k++)
        veh_circle(x, y, rot, flip, SPOTS[k][0], SQ(SPOTS[k][1]), 4, deep);
    float leg[8] = { 22, SQ(18), 32, SQ(18), 40 + stretch * 6, 38, 24 + stretch * 6, 38 };
    veh_poly(x, y, rot, flip, 4, leg, deep);
    float ey = SQ(-30) - stretch * 3;
    for (int k = 0; k < 2; k++) {
        float ex = 18.0f + (float)k * 22;
        veh_circle(x, y, rot, flip, ex, ey, 14, c2);
        veh_circle(x, y, rot, flip, ex, ey, 10, white);
        if ((frame + k * 7) % 150 < 5)
            veh_rect(x, y, rot, flip, ex - 10, ey - 1, ex + 10, ey + 2, dark);
        else
            veh_circle(x, y, rot, flip, ex + 4, ey + 1, 5, dark);
    }
    veh_rect(x, y, rot, flip, 20, SQ(4), 44, SQ(8), deep);
    veh_rect(x, y, rot, flip, 40, SQ(0), 44, SQ(8), deep);
    veh_rect(x, y, rot, flip, -30, SQ(-28), 8, SQ(-22), c1);
    veh_pilot(x, y, rot, flip, -12, SQ(-48), 46);
    if (charging) {
        static const color_t PIP[3] = { RGB(255, 240, 60), RGB(255, 160, 30), RGB(255, 70, 50) };
        for (int k = 0; k < 3; k++) {
            float px = -36.0f + (float)k * 24, py = SQ(-90);
            int lit = k <= step;
            veh_circle(x, y, 0, flip, px, py, lit ? 9 : 6, lit ? PIP[step] : col_scale(c2, 1, 3));
            if (lit && step == 2 && frame % 6 < 3)
                veh_circle(x, y, 0, flip, px, py, 4, white);
        }
    }
#undef SQ
}

static const float OUTLINE[16] = { -40, 2, -26, -16, -30, -60, 12, -60, 44, -32, 44, 6, 36, 38, -50, 38 };

const ModeDef MODE_FROG_DEF = {
    .name = "frog", .label = "FROG", .portal_name = "portal_frog",
    .corridor = 0, .half_w = PLAYER_S / 2.0f, .half_h = PLAYER_S / 2.0f, .boost = 1.0f, .inputs = IN_CROSS,
    .color = RGB(30, 160, 70),
    .step = frog_step, .turn = frog_turn, .draw = frog_draw,
    .noutline = 8, .outline = OUTLINE,
};
