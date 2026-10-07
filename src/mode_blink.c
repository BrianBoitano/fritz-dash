// Fritz Dash - the blink: he runs like the cube but cannot jump; a press
// teleports him 3 cells straight ahead with a trail of sparkles, through
// spikes and thin walls. It never ends inside a block: the blink stops
// short. On the ground he can blink on every press, in the air once.
#include "input.h"
#include "mode.h"
#include "transform.h"

#define BLINK_G    2.56f
#define MAX_FALL   36.0f
#define BLINK_DIST (3.0f * TILE)

// Does box (x0, y0)-(x1, y1) overlap object o, moved by (ox, oy), if it is solid now?
static int solid_hit(const Run *r, const Body *b, const LevelObj *o, float ox, float oy, float x0, float y0, float x1,
                     float y1)
{
    int type = o->group && b->mb_to[(o->group - 1) % MAX_GROUPS] ? morph_type(r, b, o) : o->type;
    if (obj_kind(type) != K_SOLID)
        return 0;
    float ax = 0, ay = 0, bx = 1, by = 1;
    const ObjDef *d = obj_def(type);
    if (type == OBJ_PLATFORM) {
        cell_rotate(o->rot, 0, 0.5f, &ax, &ay);
        cell_rotate(o->rot, 1, 1, &bx, &by);
    } else if (d) {
        cell_rotate(o->rot, d->x0, d->y0, &ax, &ay);
        cell_rotate(o->rot, d->x1, d->y1, &bx, &by);
    }
    float sx0 = ((float)o->x + (ax < bx ? ax : bx)) * TILE + ox, sx1 = ((float)o->x + (ax < bx ? bx : ax)) * TILE + ox;
    float sy0 = ((float)o->y + (ay < by ? ay : by)) * TILE + oy, sy1 = ((float)o->y + (ay < by ? by : ay)) * TILE + oy;
    return x0 < sx1 && x1 > sx0 && y0 < sy1 && y1 > sy0;
}

// Would the body be inside a block with its centre at x?
static int blocked(const Run *r, const Body *b, float x)
{
    const Level *lv = r->lv;
    float hw = run_half_w(b) - 2, hh = run_half_h(b) - 2;
    float x0 = x - hw, x1 = x + hw, y0 = b->y - hh, y1 = b->y + hh;
    for (int i = level_first_at(lv, (int)(x0 / TILE) - 1); i < lv->nobj && lv->objs[i].x <= (int)(x1 / TILE); i++)
        if (!obj_grouped(&lv->objs[i]) && solid_hit(r, b, &lv->objs[i], 0, 0, x0, y0, x1, y1))
            return 1;
    for (int k = 0; k < lv->nmoving; k++) {
        const LevelObj *o = &lv->objs[lv->moving[k]];
        float ox, oy;
        run_group_offset(b, o->group, &ox, &oy);
        if (solid_hit(r, b, o, ox, oy, x0, y0, x1, y1))
            return 1;
    }
    return 0;
}

// ms[0]: 1 once the air blink is used. fx_y, fx_t: where (world x) and when
// the last blink left from (drawing).
static void blink_step(Run *r, Body *b, int held, int input)
{
    (void)held;
    (void)input;
    float g = (float)b->grav;
    if (b->on_ground)
        b->ms[0] = 0;
    if (r->buffered && !b->ms[0] && !run_orb_waiting(r)) {
        float to = b->x + BLINK_DIST;
        while (to > b->x && blocked(r, b, to))
            to -= 4;
        b->fx_y = b->x;
        b->fx_t = 0;
        b->x = to;
        if (!b->on_ground)
            b->ms[0] = 1;
        run_jumped(r);
    }
    b->vy -= BLINK_G * g * (1.0f / SUB);
    if (b->vy * g < -MAX_FALL)
        b->vy = -MAX_FALL * g;
}

static void blink_turn(Body *b)
{
    b->rot = b->on_ground ? 0 : fsin((float)b->t * 0.3f) * 0.08f;
}

// His cube in a wizard's hat with four crystal shards circling it. A blink
// leaves a burst of light where he was and a line of stars to where he is.
static void blink_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot, s = veh_scale();
    int flip = b->grav < 0;
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2), white = veh_color(VC_WHITE), bg = veh_bg();
    color_t shard = b->ms[0] ? col_lerp(c2, bg, 150) : c2;
    if (b->fx_t < 18) {
        float px = veh_sx(b->fx_y), t = (float)b->fx_t;
        for (int k = 0; k < 10; k++) {
            float u = (float)k / 10.0f, ax = px + (x - px) * u;
            float ay = y + fsin(u * 2 * PI_F + t * 0.4f) * 18 * s;
            float rr = (16.0f - t * 0.8f) * (0.5f + u * 0.5f) * s;
            if (rr > 1)
                ui_star(ax, ay, rr, col_lerp(k % 2 ? RGB(255, 240, 120) : white, bg, 40 + b->fx_t * 11));
        }
        float ring = (16.0f + t * 7) * s;
        for (int k = 0; k < 12; k++) {
            float a = (float)k * (2 * PI_F / 12) + t * 0.05f;
            int sz = (int)((10.0f - t * 0.5f) * s) + 2;
            draw_rect((int)(px + fcos(a) * ring) - sz / 2, (int)(y + fsin(a) * ring) - sz / 2, sz, sz,
                      col_lerp(c2, bg, 30 + b->fx_t * 12));
        }
        if (b->fx_t < 8)
            for (int k = 0; k < 8; k++) {
                float a = (float)k * (PI_F / 4) + 0.4f, d = (30.0f + t * 9) * s;
                ui_star(x + fcos(a) * d, y + fsin(a) * d, (12.0f - t) * s, col_lerp(white, bg, b->fx_t * 22));
            }
    }
    float spin = (float)frame * 0.09f;
    for (int pass = 0; pass < 2; pass++) {
        for (int k = 0; k < 4; k++) {
            float a = spin + (float)k * HALF_PI_F, depth = fsin(a);
            if ((depth > 0) != pass)
                continue;
            float cx = fcos(a) * 54, cy = depth * 16 + 4, h = 12 + depth * 3;
            float d[8] = { cx, cy - h, cx + 7, cy, cx, cy + h, cx - 7, cy };
            veh_poly(x, y, 0, flip, 4, d, pass ? shard : col_scale(shard, 1, 2));
            if (pass)
                veh_rect(x, y, 0, flip, cx - 1, cy - h + 4, cx + 1, cy - 2, white);
        }
        if (pass == 0) {
            veh_pilot(x, y, rot, flip, 0, 0, PLAYER_S);
            float hat[6] = { -32, -36, 30, -36, 4, -86 };
            veh_poly(x, y, rot, flip, 3, hat, c2);
            float band[8] = { -26, -48, 25, -48, 23, -42, -28, -42 };
            veh_poly(x, y, rot, flip, 4, band, c1);
            veh_rect(x, y, rot, flip, -40, -40, 40, -34, col_scale(c2, 2, 3));
            veh_circle(x, y, rot, flip, -6, -62, 4, RGB(255, 235, 80));
            veh_circle(x, y, rot, flip, 10, -56, 3, RGB(255, 235, 80));
            veh_circle(x, y, rot, flip, 4, -88, 7, (frame / 5) % 4 ? RGB(255, 235, 80) : white);
        }
    }
}

static const float OUTLINE[16] = { -54, 0, -40, -34, 4, -88, 40, -34, 54, 0, 38, 38, 0, 24, -38, 38 };

const ModeDef MODE_BLINK_DEF = {
    .name = "blink", .label = "BLINK", .portal_name = "portal_blink",
    .corridor = 0, .half_w = PLAYER_S / 2.0f, .half_h = PLAYER_S / 2.0f, .boost = 1.0f, .inputs = IN_CROSS,
    .color = RGB(225, 160, 255),
    .step = blink_step, .turn = blink_turn, .draw = blink_draw,
    .noutline = 8, .outline = OUTLINE,
};
