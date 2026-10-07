// Fritz Dash - the slime: it runs and jumps like the cube (holding X keeps
// hopping), but a wall in front does not stop it: it sticks and runs
// straight up it, and goes over the top. A press on a wall splat-jumps it
// three cells straight up the wall in one go (over a spike on the wall,
// never through a block above). Drips as it goes.
#include "input.h"
#include "mode.h"
#include "packb.h"

#define SLIME_G    2.4f
#define SLIME_JUMP 27.5f
#define ZIP        (3.0f * TILE) // how far a press on a wall takes it up
#define MAX_FALL   34.0f
#define LAND_TOL   12.0f

// The nearest block face this substep's move would run into, across the
// body's height and higher than a step it would land on. 0: none.
static int wall_ahead(const Run *r, const Body *b, float adv, float *face)
{
    float hw = run_half_w(b), hh = run_half_h(b), front = b->x + hw;
    float lo = b->grav > 0 ? b->y - hh + LAND_TOL : b->y - hh + 2;
    float hi = b->grav > 0 ? b->y + hh - 2 : b->y + hh - LAND_TOL;
    int near[96], n = pb_near(r, front - TILE, front + adv + TILE, near, 96), found = 0;
    for (int k = 0; k < n; k++) {
        float x0, y0, x1, y1;
        if (run_obj_now(r, near[k], &x0, &y0, &x1, &y1) != K_SOLID)
            continue;
        if (x0 < front - 0.5f || x0 > front + adv || y1 <= lo || y0 >= hi)
            continue;
        if (!found || x0 < *face)
            *face = x0;
        found = 1;
    }
    return found;
}

// ms[0]: 1 while stuck to a wall. fx_y, fx_t: where and when it last
// splat-jumped (or frames since it stuck), for the streak and the splat.
static void slime_step(Run *r, Body *b, int held, int input)
{
    (void)input;
    float g = (float)b->grav, k = b->mini ? MINI_JUMP : 1.0f, adv = run_speed_px(b->speed) / SUB;
    float hh = run_half_h(b), climb = run_speed_px(b->speed) * k;
    if (b->ms[0] && r->buffered && !run_orb_waiting(r)) {
        float up = run_surface(r, b, b->grav), to = b->y + ZIP * k * g;
        if (g > 0 && to + hh > up)
            to = up - hh;
        if (g < 0 && to - hh < up)
            to = up + hh;
        b->fx_y = b->y;
        b->fx_t = 0;
        b->y = to;
        b->vy = climb * g;
        run_jumped(r);
        r->sfx |= SFX_SPLAT;
        for (int i = 0; i < 4; i++)
            pb_sparks(r, b->x + run_half_w(b), b->fx_y + (to - b->fx_y) * (float)i / 4, 3, RGB(150, 255, 110), -3, 3, -3,
                      5, 22, 12);
    } else if (b->on_ground && held) {
        b->vy = SLIME_JUMP * k * g;
        b->on_ground = 0;
        run_jumped(r);
    }
    float face;
    if (wall_ahead(r, b, adv, &face)) {
        if (!b->ms[0]) {
            b->fx_y = -1;
            b->fx_t = 0;
            r->sfx |= SFX_SPLAT;
            pb_sparks(r, face, b->y, 10, RGB(130, 250, 90), -5, 5, -6, 8, 24, 14);
        }
        b->ms[0] = 1;
        b->x = face - run_half_w(b) - adv;
        b->vy -= SLIME_G * g * (1.0f / SUB);
        if (b->vy * g < climb)
            b->vy = climb * g;
        return;
    }
    b->ms[0] = 0;
    b->vy -= SLIME_G * g * (1.0f / SUB);
    if (b->vy * g < -MAX_FALL)
        b->vy = -MAX_FALL * g;
}

static void slime_turn(Body *b)
{
    float a = b->ms[0] ? -0.22f * (float)b->grav : b->on_ground ? 0 : -b->vy * 0.005f * (float)b->grav;
    b->rot += (a - b->rot) * 0.4f;
}

// A wobbling blob of goo (green, with a little of his colour 2) round his
// cube, bubbles, drips that fall off it, and a splat ring where it sticks.
static void slime_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot, s = veh_scale();
    int flip = b->grav < 0, wall = b->ms[0];
    color_t goo = col_lerp(RGB(100, 235, 60), veh_color(VC_2), 60), dark = col_scale(goo, 1, 2);
    color_t light = col_lerp(goo, RGB(255, 255, 255), 110);
    float wob = fsin((float)frame * 0.35f) * 3, sq = b->on_ground && b->fx_t < 6 ? (float)(6 - b->fx_t) : 0;
    for (int k = 0; k < 3; k++) {
        int ph = (frame * 2 + k * 29) % 54;
        float dx = wall ? 40.0f - (float)k * 4 : -26.0f + (float)k * 26, dy = 34.0f + (float)ph * 0.9f;
        float ds = (float)(10 - ph / 6);
        if (ds > 2) {
            float drop[8] = { dx - ds * 0.5f, dy - ds, dx + ds * 0.5f, dy - ds, dx + ds * 0.6f, dy, dx - ds * 0.6f, dy };
            veh_poly(x, y, 0, flip, 4, drop, ph < 30 ? goo : col_lerp(goo, veh_bg(), (ph - 30) * 8));
        }
    }
    float back[16] = { -52 - sq, 16, -46, -22 + wob, -26, -44 - wob, 26, -44 + wob, 46 + (wall ? 6.0f : 0), -22 - wob,
                       52 + sq + (wall ? 4.0f : 0), 16, 46 + sq, 40, -46 - sq, 40 };
    veh_poly(x, y, rot, flip, 8, back, dark);
    float body[16] = { -48 - sq, 16, -42, -18 + wob, -23, -39 - wob, 23, -39 + wob, 42, -18 - wob, 48 + sq, 16,
                       43 + sq, 37, -43 - sq, 37 };
    veh_poly(x, y, rot, flip, 8, body, col_lerp(goo, veh_bg(), 60));
    float dome[16] = { -40, 0, -36, -22, -20, -34, 20, -34, 36, -22, 40, 0, 36, 20, -36, 20 };
    veh_poly(x, y, rot, flip, 8, dome, col_lerp(goo, RGB(255, 255, 255), 50));
    veh_pilot(x, y, rot, flip, 0, -4 + sq, 56 - sq);
    float front[8] = { -48 - sq, 14, 48 + sq, 14, 43 + sq, 38, -43 - sq, 38 };
    veh_poly(x, y, rot, flip, 4, front, goo);
    veh_rect(x, y, rot, flip, -38, 16, 38, 21, light);
    veh_rect(x, y, rot, flip, -30, -32, -14, -28, light);
    for (int k = 0; k < 3; k++) {
        float bx = -26.0f + (float)k * 22 + fsin((float)(frame + k * 40) * 0.1f) * 4;
        veh_circle(x, y, rot, flip, bx, 28, (float)(3 + (frame / 9 + k) % 3), light);
    }
    veh_circle(x, y, rot, flip, -30, -14 + wob, 4, light);
    if (b->fx_t < 10 && b->fx_y >= 0) {
        float y0 = veh_sy(b->fx_y), w = 20.0f * s * (float)(10 - b->fx_t) / 10;
        float top = y0 < y ? y0 : y, bot = y0 < y ? y : y0;
        draw_rect((int)(x + 30 * s - w), (int)top, (int)(2 * w), (int)(bot - top), col_lerp(goo, veh_bg(), b->fx_t * 22));
        draw_rect((int)(x + 30 * s - w / 3), (int)top, (int)(2 * w / 3) + 1, (int)(bot - top), light);
    }
    if (b->fx_t < 10) {
        float cx = wall ? x + 44 * s : x, cy = wall ? y : y + (flip ? -38.0f : 38.0f) * s;
        color_t c = col_lerp(light, veh_bg(), b->fx_t * 24);
        for (int k = 0; k < 8; k++) {
            float a = (float)k * 0.785f, rr = (12.0f + (float)b->fx_t * 7) * s;
            draw_rect((int)(cx + fcos(a) * rr * (wall ? 0.5f : 1.0f)) - 5, (int)(cy + fsin(a) * rr * (wall ? 1.0f : 0.4f)) - 5,
                      10, 10, c);
        }
    }
}

static const float OUTLINE[16] = { -52, 16, -46, -22, -26, -44, 26, -44, 46, -22, 52, 16, 46, 40, -46, 40 };

const ModeDef MODE_SLIME_DEF = {
    .name = "slime", .label = "SLIME", .portal_name = "portal_slime",
    .corridor = 0, .half_w = PLAYER_S / 2.0f, .half_h = PLAYER_S / 2.0f, .boost = 1.0f, .inputs = IN_CROSS,
    .color = RGB(110, 230, 60),
    .step = slime_step, .turn = slime_turn, .draw = slime_draw,
    .noutline = 8, .outline = OUTLINE,
};
