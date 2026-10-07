// Fritz Dash - the skater: his cube on a skateboard. A press ollies, from
// the ground or a rail; holding X does not ollie again, so held on a RAIL he
// keeps grinding it, a third faster with sparks, and ollies from rail to rail
// with the next press. Rails are one-way for everyone: they hold what lands
// on them from above and let the rest by.
#include <stdio.h>

#include "input.h"
#include "mode.h"
#include "packb.h"

#define SK_G     2.45f
#define SK_OLLIE 27.5f
#define MAX_FALL 36.0f
#define GRIND    1.35f          // speed on a rail
#define LAND_TOL 12.0f

// Standing on a rail now (a rail top right under its feet).
static int on_rail(const Run *r, const Body *b)
{
    if (!b->on_ground)
        return 0;
    float hw = run_half_w(b), hh = run_half_h(b), feet = b->grav > 0 ? b->y - hh : b->y + hh;
    int near[64], n = pb_near(r, b->x - hw, b->x + hw, near, 64);
    for (int k = 0; k < n; k++) {
        const LevelObj *o = &r->lv->objs[near[k]];
        float x0, y0, x1, y1;
        if (o->type != OBJ_RAIL)
            continue;
        pb_box(b, o, &x0, &y0, &x1, &y1);
        float top = b->grav > 0 ? y1 : y0;
        if (x1 > b->x - hw && x0 < b->x + hw && top - feet < 1.5f && feet - top < 1.5f)
            return 1;
    }
    return 0;
}

// ms[0]: 1 while grinding. fx_t: frames since the ollie (a kickflip).
static void skater_step(Run *r, Body *b, int held, int input)
{
    (void)held;
    (void)input;
    float g = (float)b->grav, k = b->mini ? MINI_JUMP : 1.0f;
    if (b->on_ground && r->buffered && !run_orb_waiting(r)) {
        b->vy = SK_OLLIE * k * g;
        b->on_ground = 0;
        b->fx_t = 0;
        run_jumped(r);
    }
    int grind = on_rail(r, b);
    if (grind && !b->ms[0]) {
        r->sfx |= SFX_TICK;
        pb_sparks(r, b->x - 20, b->y - g * run_half_h(b), 10, RGB(255, 210, 80), -8, 5, 1, 9, 22, 10);
    }
    b->ms[0] = grind;
    if (grind)
        b->x += run_speed_px(b->speed) * (GRIND - 1) * (1.0f / SUB);
    b->vy -= SK_G * g * (1.0f / SUB);
    if (b->vy * g < -MAX_FALL)
        b->vy = -MAX_FALL * g;
}

static void skater_turn(Body *b)
{
    float a = b->ms[0] ? 0.08f : b->on_ground ? 0 : -b->vy * 0.006f * (float)b->grav;
    b->rot += (a - b->rot) * 0.35f;
}

// The board: a deck with kicktails in his colour 1, grip tape, trucks and
// spinning wheels; on an ollie it kickflips under him. Grinding, the
// trucks glow and sparks spray off the back.
static void skater_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot;
    int flip = b->grav < 0, grind = b->ms[0];
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2), dark = veh_color(VC_DARK);
    float flipk = b->fx_t < 16 ? fcos((float)b->fx_t * (2 * PI_F / 16)) : 1.0f;
    float lift = b->fx_t < 16 ? fsin((float)b->fx_t * (PI_F / 16)) * 10 : 0;
    float th = 3 + 4 * (flipk < 0 ? -flipk : flipk);
    if (grind)
        for (int k = 0; k < 7; k++) {
            int ph = (frame * 3 + k * 5) % 14;
            float sx = -34.0f - (float)ph * 5, sy = 34.0f - (float)((k * 7) % 9) - (float)ph * 1.5f + (float)(ph * ph) * 0.18f;
            veh_rect(x, y, 0, flip, sx, sy, sx + 6, sy + 4, ph < 7 ? RGB(255, 245, 160) : RGB(255, 150, 40));
        }
    veh_pilot(x, y, rot, flip, 4, -6 - lift * 0.3f, 56);
    float dy = 24 - lift;
    float deck[8] = { -40, dy - th, 40, dy - th, 40, dy + th, -40, dy + th };
    float nose[8] = { 40, dy - th, 50, dy - th - 10, 54, dy - th - 8, 40, dy + th };
    float tail[8] = { -40, dy - th, -50, dy - th - 10, -54, dy - th - 8, -40, dy + th };
    color_t top = flipk > 0 ? RGB(40, 40, 48) : c1;
    veh_poly(x, y, rot, flip, 4, deck, c1);
    veh_poly(x, y, rot, flip, 4, nose, c1);
    veh_poly(x, y, rot, flip, 4, tail, c1);
    veh_rect(x, y, rot, flip, -36, dy - th, 36, dy - th + (th > 4 ? 3 : 1), top);
    veh_rect(x, y, rot, flip, -20, dy - 1, 20, dy + 1, c2);
    if (flipk > -0.3f) {
        color_t truck = grind ? ((frame / 2) % 2 ? RGB(255, 200, 60) : RGB(255, 120, 30)) : RGB(170, 175, 190);
        float wy = dy + th + 5;
        for (int side = -1; side <= 1; side += 2) {
            veh_rect(x, y, rot, flip, (float)side * 26 - 8, dy + th, (float)side * 26 + 8, dy + th + 4, truck);
            veh_circle(x, y, rot, flip, (float)side * 26, wy, 6, RGB(250, 250, 240));
            float a = (float)frame * 0.9f;
            veh_rect(x, y, rot, flip, (float)side * 26 + fcos(a) * 3 - 1, wy + fsin(a) * 3 - 1, (float)side * 26 + fcos(a) * 3 + 1,
                     wy + fsin(a) * 3 + 1, dark);
        }
    }
    if (b->fx_t < 6 && b->on_ground == 0) {
        color_t c = col_lerp(RGB(255, 255, 255), veh_bg(), b->fx_t * 40);
        for (int k = 0; k < 4; k++)
            veh_rect(x, y, 0, flip, -30.0f + (float)k * 18, 44 + (float)b->fx_t * 4, -24.0f + (float)k * 18, 48 + (float)b->fx_t * 4, c);
    }
}

static const float OUTLINE[16] = { -54, 14, -38, -34, 38, -34, 54, 14, 40, 30, 26, 38, -26, 38, -40, 30 };

const ModeDef MODE_SKATER_DEF = {
    .name = "skater", .label = "SKATER", .portal_name = "portal_skater",
    .corridor = 0, .half_w = PLAYER_S / 2.0f, .half_h = PLAYER_S / 2.0f, .boost = 1.0f, .inputs = IN_CROSS,
    .color = RGB(255, 130, 20),
    .step = skater_step, .turn = skater_turn, .draw = skater_draw,
    .noutline = 8, .outline = OUTLINE,
};

// --- The rail -------------------------------------------------------------------

// Solid only for what comes down onto its top (or up to its underside with
// gravity turned round) from above it.
static int rail_now(const Run *r, const LevelObj *o)
{
    const Body *b = &r->b;
    float x0, y0, x1, y1, hh = run_half_h(b), prev = b->y - b->vy * (1.0f / SUB);
    pb_box(b, o, &x0, &y0, &x1, &y1);
    if (b->grav > 0)
        return b->vy <= 0 && prev - hh >= y1 - LAND_TOL ? K_SOLID : K_NONE;
    return b->vy >= 0 && prev + hh <= y0 + LAND_TOL ? K_SOLID : K_NONE;
}

// A shiny chrome bar halfway up its cell on posts (every other cell), with
// a dark underside, bolts, and a glint that runs along it.
static void rail_draw(const LevelObj *o, float x, float y, int frame)
{
    int x0 = (int)x - 40, top = (int)y;
    int glint = (frame * 6 + o->x * 40) % 240;
    if (!o->param && o->x % 2 == 0) {
        draw_rect((int)x - 7, top + 10, 14, 30, RGB(70, 74, 92));
        draw_rect((int)x - 4, top + 10, 4, 30, RGB(130, 136, 160));
        draw_rect((int)x - 16, top + 34, 32, 6, RGB(60, 62, 80));
    }
    draw_rect(x0, top - 2, 80, 14, RGB(50, 52, 70));
    draw_rect(x0, top, 80, 10, RGB(185, 192, 214));
    draw_rect(x0, top + 6, 80, 4, RGB(120, 126, 150));
    draw_rect(x0, top + 1, 80, 3, RGB(255, 255, 255));
    draw_rect(x0 + 36, top + 6, 8, 3, RGB(255, 160, 40));
    if (glint < 80)
        draw_rect(x0 + glint, top, 12, 10, RGB(255, 250, 200));
}

static void rail_set_step(LevelObj *o, int k, int step)
{
    (void)k;
    (void)step;
    o->param = (uint8_t)!o->param;
}

static void rail_set_text(const LevelObj *o, int k, char *buf, int n)
{
    (void)k;
    snprintf(buf, (size_t)n, "%s", o->param ? "NO POSTS" : "POSTS");
}

static void rail_info(const LevelObj *o, char *buf, int n)
{
    (void)o;
    snprintf(buf, (size_t)n, "LAND ON IT; THE SKATER GRINDS IT");
}

const ObjDef OBJ_RAIL_DEF = {
    .name = "rail", .label = "RAIL", .cat = PAL_MODES, .kind = K_SOLID,
    .x0 = 0, .y0 = 0.38f, .x1 = 1, .y1 = 0.5f,
    .draw = rail_draw, .nset = 1, .set_names = { "POSTS" }, .set_step = rail_set_step, .set_text = rail_set_text,
    .info = rail_info, .kind_now = rail_now,
};
