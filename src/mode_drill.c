// Fritz Dash - the drill: it runs like the cube and stands on dirt. On dirt
// a press dives into it, and inside it tunnels along under the surface
// (only dirt: blocks stay hard); a press inside bursts up and out with a
// big jump. On anything else a press jumps. Dirt blocks are here too.
#include <stdio.h>

#include "input.h"
#include "mode.h"

#define DRILL_G    2.56f
#define DRILL_JUMP 29.9f
#define DRILL_BURST 36.0f       // out of the dirt: about 3.1 cells
#define DRILL_DIVE 20.0f
#define MAX_FALL   36.0f
#define DEEP       16.0f        // this far into dirt counts as inside it
#define LAND_TOL   12.0f

static int dirt_hit(const Body *b, const LevelObj *o, float x0, float y0, float x1, float y1)
{
    float ox = 0, oy = 0;
    if (obj_grouped(o))
        run_group_offset(b, o->group, &ox, &oy);
    float cx = (float)o->x * TILE + ox, cy = (float)o->y * TILE + oy;
    return x0 < cx + TILE && x1 > cx && y0 < cy + TILE && y1 > cy;
}

// Does any dirt overlap the box (world pixels)?
static int dirt_in(const Level *lv, const Body *b, float x0, float y0, float x1, float y1)
{
    for (int i = level_first_at(lv, (int)(x0 / TILE) - 1); i < lv->nobj && lv->objs[i].x <= (int)(x1 / TILE); i++)
        if (lv->objs[i].type == OBJ_DIRT && !obj_grouped(&lv->objs[i]) && dirt_hit(b, &lv->objs[i], x0, y0, x1, y1))
            return 1;
    for (int k = 0; k < lv->nmoving; k++)
        if (lv->objs[lv->moving[k]].type == OBJ_DIRT && dirt_hit(b, &lv->objs[lv->moving[k]], x0, y0, x1, y1))
            return 1;
    return 0;
}

// ms[0]: 1 while it digs (dirt does not hold it up). fx_y, fx_t: where and
// when it last dived or burst out (drawing).
static void drill_step(Run *r, Body *b, int held, int input)
{
    (void)held;
    (void)input;
    const Level *lv = r->lv;
    float g = (float)b->grav, k = b->mini ? MINI_JUMP : 1.0f, hw = run_half_w(b), hh = run_half_h(b);
    float d = b->mini ? DEEP / 2 : DEEP;
    int inside = dirt_in(lv, b, b->x - hw + d, b->y - hh + d, b->x + hw - d, b->y + hh - d);
    if (b->ms[0] && (b->on_ground || b->vy * g > 0) && !dirt_in(lv, b, b->x - hw, b->y - hh, b->x + hw, b->y + hh))
        b->ms[0] = 0;
    if (inside)
        b->ms[0] = 1;
    if (r->buffered && !run_orb_waiting(r)) {
        float feet = b->y - hh * g;
        if (b->ms[0]) {
            b->vy = DRILL_BURST * k * g;
            b->fx_y = b->y;
            b->fx_t = 0;
            run_jumped(r);
        } else if (b->on_ground && dirt_in(lv, b, b->x - hw + 6, g > 0 ? feet - 8 : feet + 1, b->x + hw - 6,
                                           g > 0 ? feet - 1 : feet + 8)) {
            b->ms[0] = 1;
            b->vy = -DRILL_DIVE * g;
            b->fx_y = b->y;
            b->fx_t = 0;
            run_jumped(r);
        } else if (b->on_ground) {
            b->vy = DRILL_JUMP * k * g;
            run_jumped(r);
        }
        b->on_ground = 0;
    }
    b->vy -= DRILL_G * g * (1.0f / SUB);
    if (b->vy * g < -MAX_FALL)
        b->vy = -MAX_FALL * g;
}

static void drill_turn(Body *b)
{
    float a = b->on_ground ? 0 : -b->vy * 0.03f * (float)b->grav;
    a = a > 0.9f ? 0.9f : a < -0.9f ? -0.9f : a;
    b->rot += (a - b->rot) * 0.4f;
}

// A little digger: treads, a cab with his cube at the wheel, and a big
// spinning drill bit in front. Digging throws dirt out behind it.
static void drill_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot, s = veh_scale();
    int flip = b->grav < 0, dig = b->ms[0];
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2), white = veh_color(VC_WHITE), dark = veh_color(VC_DARK);
    if (dig || b->fx_t < 10)
        for (int k = 0; k < 7; k++) {
            int age = (frame * 2 + k * 9) % 18;
            float px = x - (30.0f + (float)age * 7) * s, py = y + ((float)(age * age) * 0.35f - (float)age * 6 - 10) * s;
            int sz = (int)((12 - age / 2) * s) + 2;
            draw_rect((int)px - sz / 2, (int)py - sz / 2, sz, sz, k % 3 ? RGB(130, 82, 46) : RGB(90, 56, 30));
        }
    veh_rect(x, y, rot, flip, -40, 20, 30, 38, dark);
    for (int k = 0; k < 4; k++)
        veh_circle(x, y, rot, flip, -30.0f + (float)k * 19, 29, 8, RGB(110, 110, 125));
    for (int k = 0; k < 6; k++) {
        float tx = -40.0f + (float)((k * 12 + frame * 3) % 70);
        veh_rect(x, y, rot, flip, tx, 19, tx + 5, 22, RGB(170, 170, 185));
        veh_rect(x, y, rot, flip, tx, 36, tx + 5, 39, RGB(170, 170, 185));
    }
    static const float BODY[12] = { -42, 20, -42, -6, -30, -14, 26, -14, 32, -4, 32, 20 };
    veh_poly(x, y, rot, flip, 6, BODY, c2);
    veh_rect(x, y, rot, flip, -42, 8, 32, 12, col_scale(c2, 1, 2));
    veh_rect(x, y, rot, flip, -34, -48, 10, -10, col_scale(c2, 1, 2));
    veh_rect(x, y, rot, flip, -32, -46, 8, -12, col_lerp(c1, RGB(10, 4, 22), 180));
    veh_pilot(x, y, rot, flip, -12, -28, 32);
    veh_rect(x, y, rot, flip, -38, -52, 14, -46, c2);
    veh_rect(x, y, rot, flip, -46, -30, -40, -14, RGB(90, 90, 105));
    if (!dig && frame % 12 < 6)
        veh_circle(x, y, rot, flip, -44, -38 - (float)(frame % 12), 5, col_lerp(white, veh_bg(), 120));
    veh_rect(x, y, rot, flip, 30, -18, 38, 24, c1);
    float cone[6] = { 38, -20, 38, 26, 84, 3 };
    veh_poly(x, y, rot, flip, 3, cone, RGB(190, 195, 210));
    float spin = (float)((frame * (dig ? 5 : 2)) % 16);
    for (int k = 0; k < 3; k++) {
        float u = 38 + spin + (float)k * 16;
        if (u > 80)
            continue;
        float h0 = 23 * (84 - u) / 46, h1 = 23 * (84 - u - 6) / 46;
        float band[8] = { u, 3 - h0, u + 6, 3 - h1, u + 2, 3 + h1, u - 4, 3 + h0 };
        veh_poly(x, y, rot, flip, 4, band, RGB(110, 115, 135));
    }
    float glint[6] = { 40, -16, 44, -14, 78, 2 };
    veh_poly(x, y, rot, flip, 3, glint, white);
}

// Inside dirt the trail is the tunnel it leaves; outside, his own trail.
static void drill_trail(const float *tx, const float *ty)
{
    const Run *r = draw_run();
    float s = veh_scale();
    for (int i = TRAIL_N - 1; i >= 0; i--) {
        int in = r && dirt_in(r->lv, &r->b, tx[i] - 4, ty[i] - 4, tx[i] + 4, ty[i] + 4);
        if (in)
            draw_circle((int)veh_sx(tx[i]), (int)veh_sy(ty[i]), (int)(34 * s), RGB(48, 28, 16));
        else if (i % 2 == 0 && i > 0)
            draw_rect((int)veh_sx(tx[i]) - 5, (int)(veh_sy(ty[i]) + 30 * s), 10, 6,
                      col_lerp(RGB(150, 110, 70), veh_bg(), 60 + i * 13));
    }
}

static const float OUTLINE[16] = { -42, 38, -42, -6, -34, -48, 14, -52, 32, -14, 84, 3, 38, 26, 30, 38 };

const ModeDef MODE_DRILL_DEF = {
    .name = "drill", .label = "DRILL", .portal_name = "portal_drill",
    .corridor = 0, .half_w = PLAYER_S / 2.0f, .half_h = PLAYER_S / 2.0f, .boost = 1.0f, .inputs = IN_CROSS,
    .color = RGB(150, 95, 45),
    .step = drill_step, .turn = drill_turn, .draw = drill_draw, .trail = drill_trail,
    .noutline = 8, .outline = OUTLINE,
};

// --- Dirt: soft ground. Everything lands on its top (its underside when
// upside down) and passes through it otherwise; a digging drill passes
// through it all.

static void dirt_act(Run *r, const LevelObj *o)
{
    Body *b = &r->b;
    if (b->mode == MODE_DRILL && b->ms[0])
        return;
    float ox = 0, oy = 0;
    if (obj_grouped(o))
        run_group_offset(b, o->group, &ox, &oy);
    float y0 = (float)o->y * TILE + oy, y1 = y0 + TILE, hh = run_half_h(b);
    float tol = LAND_TOL + (b->vy < 0 ? -b->vy : b->vy) * (1.0f / SUB) + 1;
    if (b->grav > 0 && b->vy <= 0 && b->y - hh >= y1 - tol) {
        b->y = y1 + hh;
        b->vy = 0;
        b->on_ground = 1;
    } else if (b->grav < 0 && b->vy >= 0 && b->y + hh <= y0 + tol) {
        b->y = y0 - hh;
        b->vy = 0;
        b->on_ground = 1;
    }
}

static int is_dirt(const Level *lv, int x, int y)
{
    int i = level_find(lv, x, y);
    return i >= 0 && lv->objs[i].type == OBJ_DIRT;
}

// Soil with pebbles from a hash of its cell, and grass on top where no dirt
// is above it (in the editor every cell has grass).
static void dirt_draw(const LevelObj *o, float x, float y, int frame)
{
    (void)frame;
    const Run *r = draw_run();
    int l = (int)x - TILE / 2, t = (int)y - TILE / 2;
    color_t soil = RGB(124, 78, 44), dark = RGB(84, 50, 28), light = RGB(160, 108, 64);
    draw_rect(l, t, TILE, TILE, dark);
    draw_rect(l + 2, t + 2, TILE - 4, TILE - 4, soil);
    uint32_t h = (uint32_t)o->x * 73856093u ^ (uint32_t)o->y * 19349663u;
    for (int k = 0; k < 7; k++) {
        h = h * 1103515245u + 12345u;
        int px = (int)((h >> 8) % 62), py = (int)((h >> 16) % 58), sz = 4 + (int)((h >> 26) % 7);
        draw_rect(l + 8 + px, t + 12 + py, sz + 2, sz, k % 3 ? dark : light);
    }
    if (!r || !is_dirt(r->lv, o->x, o->y + 1)) {
        draw_rect(l, t, TILE, 12, RGB(60, 160, 60));
        draw_rect(l, t + 10, TILE, 4, RGB(40, 110, 40));
        for (int k = 0; k < 8; k++)
            draw_rect(l + 4 + k * 10, t - 6 + (k % 3) * 2, 3, 8 - (k % 3) * 2, RGB(90, 200, 80));
    }
}

static void dirt_info(const LevelObj *o, char *buf, int n)
{
    (void)o;
    snprintf(buf, (size_t)n, "SOFT GROUND: THE DRILL DIGS THROUGH IT");
}

const ObjDef OBJ_DIRT_DEF = {
    .name = "dirt", .label = "DIRT", .cat = PAL_MODES, .kind = K_TRIGGER,
    .x0 = 0, .y0 = 0, .x1 = 1, .y1 = 1,
    .act = dirt_act, .draw = dirt_draw, .info = dirt_info,
};
