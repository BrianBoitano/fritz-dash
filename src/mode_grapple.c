// Fritz Dash - the grapple: he falls in a ten cell corridor, and holding X
// shoots a rope to the nearest anchor ring ahead of him and above. The rope
// reels him in to its length, he swings down and up round the ring, and
// letting go flies him off with the swing's speed (so does the rope running
// out at the far side).
#include <stdio.h>

#include "input.h"
#include "mode.h"

#define GRAP_G    1.7f
#define GRAP_FALL 16.0f
#define REACH     (5.5f * TILE)     // the rope catches rings this far away
#define ROPE      (3.5f * TILE)     // and reels him in to this length
#define SWING_MAX 24.0f
#define LET_GO    0.93f             // past this far round, the rope lets go

static float root(float v)
{
    if (v <= 0)
        return 0;
    float x = v > 1 ? v : 1;
    for (int i = 0; i < 24; i++)
        x = 0.5f * (x + v / x);
    return x;
}

static void ring_pos(const Body *b, const LevelObj *o, float *x, float *y)
{
    float ox = 0, oy = 0;
    if (obj_grouped(o))
        run_group_offset(b, o->group, &ox, &oy);
    *x = ((float)o->x + 0.5f) * TILE + ox;
    *y = ((float)o->y + 0.5f) * TILE + oy;
}

static void ring_try(const Body *b, const LevelObj *o, int i, float *best, int *found)
{
    float x, y;
    ring_pos(b, o, &x, &y);
    float dx = x - b->x, dy = (y - b->y) * (float)b->grav;
    if (dx > 0 && dy > 30 && dx * dx + dy * dy < *best) {
        *best = dx * dx + dy * dy;
        *found = i;
    }
}

// The nearest ring ahead of him and above him (against gravity) in reach: its index, or -1.
static int ring_ahead(const Run *r, const Body *b)
{
    const Level *lv = r->lv;
    float best = REACH * REACH;
    int found = -1, last = (int)((b->x + REACH) / TILE) + 1;
    for (int i = level_first_at(lv, (int)(b->x / TILE)); i < lv->nobj && lv->objs[i].x <= last; i++)
        if (lv->objs[i].type == OBJ_ANCHOR && !obj_grouped(&lv->objs[i]))
            ring_try(b, &lv->objs[i], i, &best, &found);
    for (int k = 0; k < lv->nmoving; k++)
        if (lv->objs[lv->moving[k]].type == OBJ_ANCHOR)
            ring_try(b, &lv->objs[lv->moving[k]], lv->moving[k], &best, &found);
    return found;
}

// ms[0]: 1 + the ring he hangs from (0: none). fx_t: frames since the rope
// caught (drawing).
static void grapple_step(Run *r, Body *b, int held, int input)
{
    (void)input;
    float g = (float)b->grav;
    if (!held) {
        b->ms[0] = 0;
    } else if (!b->ms[0]) {
        int i = ring_ahead(r, b);
        if (i >= 0) {
            b->ms[0] = i + 1;
            b->fx_t = 0;
            run_jumped(r);
        }
    }
    if (b->ms[0]) {
        float ax, ay;
        ring_pos(b, &r->lv->objs[b->ms[0] - 1], &ax, &ay);
        float dx = b->x + run_speed_px(b->speed) * (1.0f / SUB) - ax;
        if (dx < ROPE * LET_GO) {
            float to = dx <= -ROPE ? ay : ay - g * root(ROPE * ROPE - dx * dx), v = (to - b->y) * SUB;
            b->vy = v > SWING_MAX ? SWING_MAX : v < -SWING_MAX ? -SWING_MAX : v;
            b->on_ground = 0;
            return;
        }
        b->ms[0] = 0;
    }
    b->vy -= GRAP_G * g * (1.0f / SUB);
    if (b->vy * g < -GRAP_FALL)
        b->vy = -GRAP_FALL * g;
}

static void grapple_turn(Body *b)
{
    float a = -b->vy * 0.022f * (float)b->grav;
    b->rot = a > 0.5f ? 0.5f : a < -0.5f ? -0.5f : a;
}

// A dotted line between two screen points, the first frac of it.
static void rope(float x0, float y0, float x1, float y1, float frac, int size, color_t a, color_t b)
{
    float dx = x1 - x0, dy = y1 - y0, len = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
    int n = (int)(len / 7) + 1;
    for (int k = 0; k <= n; k++) {
        float t = (float)k / (float)n * frac;
        draw_rect((int)(x0 + dx * t) - size / 2, (int)(y0 + dy * t) - size / 2, size, size, k % 3 ? a : b);
    }
}

// His cube in a harness with a grappling gun on top. On the rope, the rope
// runs from the gun to the ring; off it, a faint aiming line shows the ring
// the rope would catch.
static void grapple_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot, s = veh_scale(), cs = fcos(rot) * s, sn = fsin(rot) * s;
    int flip = b->grav < 0;
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2), white = veh_color(VC_WHITE), dark = veh_color(VC_DARK);
    const Run *r = draw_run();
    float gy = flip ? 49 : -49, tipx = x + 50 * cs - gy * sn, tipy = y + 50 * sn + gy * cs;
    if (r && b->ms[0] && b->ms[0] - 1 < r->lv->nobj) {
        float ax, ay;
        ring_pos(b, &r->lv->objs[b->ms[0] - 1], &ax, &ay);
        float f = b->fx_t < 5 ? (float)(b->fx_t + 1) / 6.0f : 1.0f;
        rope(tipx, tipy, veh_sx(ax), veh_sy(ay), f, (int)(6 * s) + 2, RGB(230, 200, 140), RGB(150, 110, 60));
    } else if (r && b == &r->b) {
        int i = ring_ahead(r, b);
        if (i >= 0) {
            float ax, ay;
            ring_pos(b, &r->lv->objs[i], &ax, &ay);
            float sx = veh_sx(ax), sy = veh_sy(ay), dx = sx - tipx, dy = sy - tipy;
            for (int k = 1; k < 12; k += 2)
                draw_rect((int)(tipx + dx * (float)k / 12) - 2, (int)(tipy + dy * (float)k / 12) - 2, 4, 4,
                          col_lerp(white, veh_bg(), 150));
        }
    }
    veh_rect(x, y, rot, flip, -40, 6, 40, 14, col_scale(c2, 1, 2));
    veh_circle(x, y, rot, flip, -40, -6, 16, col_scale(c2, 1, 2));
    veh_circle(x, y, rot, flip, -40, -6, 12, c2);
    float spin = (float)frame * (b->ms[0] ? 0.5f : 0.1f);
    veh_rect(x, y, rot, flip, -40 + fcos(spin) * 8 - 2, -6 + fsin(spin) * 8 - 2, -40 + fcos(spin) * 8 + 2,
             -6 + fsin(spin) * 8 + 2, dark);
    veh_circle(x, y, rot, flip, -40, -6, 4, RGB(230, 200, 140));
    veh_pilot(x, y, rot, flip, 0, 0, PLAYER_S);
    veh_rect(x, y, rot, flip, -40, 8, 40, 16, c2);
    veh_rect(x, y, rot, flip, -8, 6, 8, 18, RGB(255, 210, 60));
    veh_rect(x, y, rot, flip, -16, -42, -8, 8, c2);
    veh_rect(x, y, rot, flip, -28, -58, 26, -38, dark);
    veh_rect(x, y, rot, flip, -26, -56, 24, -40, c2);
    float barrel[8] = { 20, -56, 46, -56, 50, -42, 20, -42 };
    veh_poly(x, y, rot, flip, 4, barrel, RGB(170, 175, 195));
    veh_rect(x, y, rot, flip, -22, -53, -6, -45, c1);
    veh_rect(x, y, rot, flip, 22, -54, 46, -52, RGB(230, 232, 245));
    if (!b->ms[0]) {
        float claw1[6] = { 46, -58, 60, -66, 54, -50 }, claw2[6] = { 46, -40, 60, -32, 54, -48 };
        veh_poly(x, y, rot, flip, 3, claw1, RGB(220, 225, 240));
        veh_poly(x, y, rot, flip, 3, claw2, RGB(220, 225, 240));
    }
    if (b->ms[0] && b->fx_t < 8 && frame % 2 == 0)
        veh_circle(x, y, rot, flip, 52, -49, 12 - b->fx_t, white);
}

static const float OUTLINE[16] = { -56, -6, -28, -58, 60, -66, 60, -32, 40, 0, 40, 38, -40, 38, -56, 10 };

const ModeDef MODE_GRAPPLE_DEF = {
    .name = "grapple", .label = "GRAPPLE", .portal_name = "portal_grapple", .flags = MF_SLIDE,
    .corridor = 10, .half_w = PLAYER_S / 2.0f, .half_h = PLAYER_S / 2.0f, .boost = 0.6f, .inputs = IN_CROSS,
    .color = RGB(230, 180, 60),
    .step = grapple_step, .turn = grapple_turn, .draw = grapple_draw,
    .noutline = 8, .outline = OUTLINE,
};

// --- Anchor ring: what the grapple's rope catches. It never collides.

static void anchor_draw(const LevelObj *o, float x, float y, int frame)
{
    const Run *r = draw_run();
    int i = r ? (int)(o - r->lv->objs) : -1;
    int held = r && i >= 0 && i < r->lv->nobj && r->b.mode == MODE_GRAPPLE && r->b.ms[0] == i + 1;
    int aim = r && !held && i >= 0 && i < r->lv->nobj && r->b.mode == MODE_GRAPPLE && !r->b.ms[0] && ring_ahead(r, &r->b) == i;
    color_t gold = RGB(255, 200, 50), deep = RGB(170, 110, 20), bg = veh_bg();
    if (held || aim) {
        float rr = 44.0f + fsin((float)frame * 0.3f) * 5;
        for (int k = 0; k < 16; k++) {
            float a = (float)k * (2 * PI_F / 16) + (float)frame * 0.05f;
            draw_rect((int)(x + fcos(a) * rr) - 3, (int)(y + fsin(a) * rr) - 3, 6, 6,
                      col_lerp(held ? RGB(255, 255, 255) : gold, bg, held ? 40 : 120));
        }
    }
    for (int k = 0; k < 18; k++) {
        float a = (float)k * (2 * PI_F / 18);
        float px = x + fcos(a) * 28, py = y + fsin(a) * 28;
        draw_circle((int)px, (int)py + 2, 7, deep);
    }
    for (int k = 0; k < 18; k++) {
        float a = (float)k * (2 * PI_F / 18);
        draw_circle((int)(x + fcos(a) * 28), (int)(y + fsin(a) * 28), 6, k % 6 == 1 ? RGB(255, 245, 200) : gold);
    }
    draw_rect((int)x - 3, (int)y - 40, 6, 12, deep);
    float gx[4] = { x, x + 9, x, x - 9 }, gy[4] = { y - 11, y, y + 11, y };
    draw_poly(4, gx, gy, held ? RGB(255, 255, 255) : RGB(90, 220, 255));
}

static void anchor_info(const LevelObj *o, char *buf, int n)
{
    (void)o;
    snprintf(buf, (size_t)n, "THE GRAPPLE'S ROPE CATCHES IT: HOLD X");
}

const ObjDef OBJ_ANCHOR_DEF = {
    .name = "anchor_ring", .label = "ANCHOR RING", .cat = PAL_JUMPS, .kind = K_NONE,
    .x0 = 0, .y0 = 0, .x1 = 1, .y1 = 1,
    .draw = anchor_draw, .info = anchor_info,
};
