// Fritz Dash - the time bender: a flying pocket watch. Every press flaps
// like the UFO; holding X bends time so he moves at half speed (forward and
// up and down alike, the same path only slower) while the level's beat, its
// moving blocks and morph blocks go on as before. Up to two seconds of slow
// time, and the meter fills again while X is up.
#include "input.h"
#include "mode.h"
#include "packb.h"

#define TB_G    1.35f
#define TB_FLAP 17.0f
#define TB_MAX  16.0f
#define TB_FULL (120 * SUB)     // substeps of slow time on a full meter

static void tb_enter(Body *b)
{
    b->ms[0] = TB_FULL;
}

// ms[0]: the meter; ms[1]: 1 while time is slow; ms[2]: 1 once the meter ran
// out with X held (he must let go first). fx_t: frames since it changed.
static void tb_step(Run *r, Body *b, int held, int input)
{
    (void)input;
    float g = (float)b->grav, k = b->mini ? MINI_JUMP : 1.0f;
    if (r->buffered && !run_orb_waiting(r)) {
        b->vy = TB_FLAP * k * g;
        b->on_ground = 0;
        run_jumped(r);
    }
    if (!held)
        b->ms[2] = 0;
    if (b->ms[0] <= 0)
        b->ms[2] = 1;
    int slow = held && !b->ms[2];
    if (slow != b->ms[1]) {
        b->ms[1] = slow;
        b->fx_t = 0;
        r->sfx |= SFX_TICK;
    }
    if (slow) {
        b->ms[0]--;
        if (b->t % 15 == 0)
            r->sfx |= SFX_TICK;
    } else if (b->ms[0] < TB_FULL) {
        b->ms[0]++;
    }
    float s = slow ? 0.5f : 1.0f;
    b->vy -= TB_G * g * s * (1.0f / SUB);
    if (b->vy > TB_MAX) b->vy = TB_MAX;
    if (b->vy < -TB_MAX) b->vy = -TB_MAX;
    // The engine then moves it a whole substep: start it back by the part
    // slow time leaves out (along the way it came, so never into a block).
    b->x -= run_speed_px(b->speed) * (1.0f / SUB) * (1 - s);
    b->y -= b->vy * (1.0f / SUB) * (1 - s);
}

static void tb_turn(Body *b)
{
    float a = -b->vy * 0.012f;
    b->rot = a > 0.5f ? 0.5f : a < -0.5f ? -0.5f : a;
}

// The watch: a gold case with a crown, a white face with twelve marks and
// two hands that race (or crawl when time is slow) round his cube in the
// middle, and little wings. Slow time wraps him in a ticking ring of blue
// dots, with an hourglass meter over him while it is not full.
static void tb_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot, s = veh_scale();
    int flip = b->grav < 0, slow = b->ms[1];
    color_t gold = RGB(240, 190, 60), dgold = RGB(160, 110, 30), face = RGB(250, 246, 230), c2 = veh_color(VC_2);
    color_t bg = veh_bg(), blue = RGB(110, 190, 255);
    const Run *run = draw_run();
    if (run && &run->b == b && (slow || b->fx_t < 10)) {
        int h = (int)(44.0f * (slow ? (b->fx_t < 10 ? (float)b->fx_t / 10 : 1.0f) : 1.0f - (float)b->fx_t / 10));
        draw_rect(0, 0, SCREEN_W, h, RGB(10, 20, 50));
        draw_rect(0, SCREEN_H - h, SCREEN_W, h, RGB(10, 20, 50));
        draw_rect(0, h - 4, SCREEN_W, 4, blue);
        draw_rect(0, SCREEN_H - h, SCREEN_W, 4, blue);
        for (int k = 0; k < 24 && h > 20; k++)
            draw_rect((k * 80 + (int)(b->t * 2) % 80), SCREEN_H - h + 16, 6, 14, col_lerp(blue, RGB(10, 20, 50), 120));
    }
    if (slow || b->fx_t < 10) {
        float k = slow ? (b->fx_t < 10 ? (float)b->fx_t / 10 : 1.0f) : 1.0f - (float)b->fx_t / 10;
        float rr = (58.0f + fsin((float)b->t * 0.2f) * 4) * s;
        for (int i = 0; i < 24; i++) {
            float a = (float)i * 0.2618f + (float)b->t * (slow ? 0.01f : 0.05f);
            int sz = (int)((i % 2 ? 5.0f : 9.0f) * s * k) + 1;
            draw_rect((int)(x + fcos(a) * rr) - sz / 2, (int)(y + fsin(a) * rr) - sz / 2, sz, sz,
                      col_lerp(blue, bg, i % 6 ? 60 : 0));
        }
    }
    float flapw = b->vy * (flip ? -1.0f : 1.0f) > 2 ? -10.0f : 6.0f + fsin((float)frame * 0.4f) * 3;
    float wl[8] = { -36, -10, -66, -26 + flapw, -70, -12 + flapw, -38, 8 };
    float wr[8] = { 36, -10, 66, -26 + flapw, 70, -12 + flapw, 38, 8 };
    veh_poly(x, y, rot, flip, 4, wl, col_lerp(face, c2, 80));
    veh_poly(x, y, rot, flip, 4, wr, col_lerp(face, c2, 80));
    veh_rect(x, y, rot, flip, -8, -52, 8, -42, dgold);
    veh_rect(x, y, rot, flip, -12, -58, 12, -51, gold);
    veh_circle(x, y, rot, flip, 0, 0, 44, dgold);
    veh_circle(x, y, rot, flip, 0, 0, 40, gold);
    veh_circle(x, y, rot, flip, 0, 0, 34, face);
    for (int i = 0; i < 12; i++) {
        float a = (float)i * 0.5236f, r0 = i % 3 ? 28.0f : 25.0f;
        float m[8] = { fcos(a) * r0 - 2, fsin(a) * r0 - 2, fcos(a) * 32 + 2, fsin(a) * 32 - 2, fcos(a) * 32 + 2,
                       fsin(a) * 32 + 2, fcos(a) * r0 - 2, fsin(a) * r0 + 2 };
        veh_poly(x, y, rot, flip, 4, m, i % 3 ? RGB(150, 140, 120) : RGB(40, 30, 30));
    }
    float hand = (float)b->t * (slow ? 0.02f : 0.16f), hour = hand / 12;
    float mh[8] = { -3, 0, 3, 0, fcos(hand) * 31 + 2, fsin(hand) * 31, fcos(hand) * 31 - 2, fsin(hand) * 31 };
    float hh[8] = { -4, 0, 4, 0, fcos(hour) * 24 + 3, fsin(hour) * 24, fcos(hour) * 24 - 3, fsin(hour) * 24 };
    veh_poly(x, y, rot, flip, 4, mh, RGB(200, 40, 40));
    veh_poly(x, y, rot, flip, 4, hh, RGB(30, 30, 40));
    veh_pilot(x, y, rot, flip, 0, 0, 34);
    if (b->ms[0] < TB_FULL) {
        float f = (float)b->ms[0] / TB_FULL;
        float hx = x - 26 * s, hy = y - (flip ? -70.0f : 70.0f) * s;
        float tx[4] = { hx - 9 * s, hx + 9 * s, hx, hx }, ty[4] = { hy - 12 * s, hy - 12 * s, hy, hy };
        float ux[4] = { hx, hx, hx + 9 * s, hx - 9 * s }, uy[4] = { hy, hy, hy + 12 * s, hy + 12 * s };
        draw_poly(3, tx, ty, gold);
        draw_poly(4, ux, uy, gold);
        int w = (int)(56 * s), h = (int)(10 * s);
        draw_rect((int)(hx + 16 * s), (int)hy - h / 2 - 3, w + 6, h + 6, RGB(30, 30, 60));
        draw_rect((int)(hx + 19 * s), (int)hy - h / 2, (int)((float)w * f), h,
                  f < 0.25f && (frame / 4) % 2 ? RGB(255, 90, 90) : blue);
    }
}

// Echoes of the watch along its path while time is slow; sparkles otherwise.
static void tb_trail(const float *tx, const float *ty)
{
    const Run *r = draw_run();
    color_t bg = veh_bg();
    float s = veh_scale();
    int slow = r && r->b.mode == MODE_TIME_BENDER && r->b.ms[1];
    for (int i = TRAIL_N - 1; i >= 2; i -= slow ? 4 : 2) {
        if (slow) {
            draw_circle((int)veh_sx(tx[i]), (int)veh_sy(ty[i]), (int)(40 * s), col_lerp(RGB(110, 190, 255), bg, 120 + i * 9));
            draw_circle((int)veh_sx(tx[i]), (int)veh_sy(ty[i]), (int)(30 * s), col_lerp(RGB(250, 246, 230), bg, 150 + i * 7));
        } else {
            int sz = (int)((float)(14 - i) * s) + 2;
            draw_rect((int)veh_sx(tx[i]) - sz / 2, (int)veh_sy(ty[i]) - sz / 2, sz, sz, col_lerp(RGB(255, 220, 90), bg, 80 + i * 12));
        }
    }
}

static const float OUTLINE[16] = { -70, -12, -40, -30, 0, -58, 40, -30, 70, -12, 40, 30, 0, 44, -40, 30 };

const ModeDef MODE_TIME_BENDER_DEF = {
    .name = "time_bender", .label = "TIME BENDER", .portal_name = "portal_time_bender", .flags = MF_SLIDE,
    .corridor = 10, .half_w = PLAYER_S / 2.0f, .half_h = 34.0f, .boost = 0.72f, .inputs = IN_CROSS,
    .color = RGB(240, 190, 60),
    .step = tb_step, .enter = tb_enter, .turn = tb_turn, .draw = tb_draw, .trail = tb_trail,
    .noutline = 8, .outline = OUTLINE,
};
