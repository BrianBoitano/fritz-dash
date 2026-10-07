// Fritz Dash - the cannon: a cannonball that rolls (a press hops) until it
// touches a BARREL, which loads it. Loaded, a press fires it along the
// barrel's aim and it flies in an arc to the next barrel; the fuse fires it
// anyway after a while. A swinging barrel sweeps its aim from the moment it
// is loaded, so the press picks the shot; dots show where it will fly.
#include <stdio.h>

#include "input.h"
#include "mode.h"
#include "packb.h"

#define CAN_G      1.5f         // in flight from a barrel
#define ROLL_G     2.56f
#define HOP        22.0f
#define MAX_FALL   36.0f
#define FUSE       (150 * SUB)  // substeps loaded before it fires by itself
#define SWING_SUBS (96 * SUB)   // one sweep of a swinging barrel, there and back

static const float POWER[3] = { 24.0f, 32.0f, 40.0f };
static const char *const POWER_NAMES[3] = { "SHORT", "MEDIUM", "FAR" };
static const char *const SWING_NAMES[3] = { "FIXED", "SWINGS A LITTLE", "SWINGS A LOT" };

// A barrel's "c": aim + 10 * swing + 30 * power, stored so that 0 is the
// usual barrel (45 degrees, fixed, medium). Aims go -45..90 degrees by 15.
static int aim_index(const LevelObj *o)
{
    return (o->param % 10 + 6) % 10;
}

static int swing_of(const LevelObj *o)
{
    return o->param / 10 % 3;
}

static int power_of(const LevelObj *o)
{
    return (o->param / 30 % 3 + 1) % 3;
}

static void set_param(LevelObj *o, int aim, int swing, int power)
{
    o->param = (uint8_t)((aim + 4) % 10 + 10 * swing + 30 * ((power + 2) % 3));
}

// Its aim (radians, up from straight ahead) after `loaded` substeps in it:
// a swinging one starts at the top of its sweep and passes its set aim a
// quarter sweep later.
static float barrel_aim(const LevelObj *o, int loaded)
{
    float a = (float)(aim_index(o) * 15 - 45) * (PI_F / 180.0f);
    static const float AMP[3] = { 0, 0.35f, 0.7f };
    return a + AMP[swing_of(o)] * fcos((float)loaded * (2 * PI_F / SWING_SUBS));
}

static void barrel_centre(const Body *b, const LevelObj *o, float *x, float *y)
{
    float ox = 0, oy = 0;
    if (obj_grouped(o))
        run_group_offset(b, o->group, &ox, &oy);
    *x = ((float)o->x + 0.5f) * TILE + ox;
    *y = ((float)o->y + 0.5f) * TILE + oy;
}

// ms[0]: 1 + the barrel it is loaded in (0: rolling or flying); ms[1]:
// substeps it has been loaded; ms[2]: 1 + the barrel it was last fired
// from (that one cannot catch it again). mf[0]: its speed ahead while it
// flies from a barrel (0: the level's speed). fx_t: frames since the shot.
static void cannon_step(Run *r, Body *b, int held, int input)
{
    (void)held;
    (void)input;
    float g = (float)b->grav, k = b->mini ? MINI_JUMP : 1.0f, adv = run_speed_px(b->speed) / SUB;
    if (b->ms[0]) {
        const LevelObj *o = &r->lv->objs[b->ms[0] - 1];
        float cx, cy;
        barrel_centre(b, o, &cx, &cy);
        b->x = cx - adv;
        b->y = cy;
        b->vy = 0;
        b->ms[1]++;
        int press = r->buffered && !run_orb_waiting(r);
        if (press || b->ms[1] >= FUSE) {
            float a = barrel_aim(o, b->ms[1]), p = POWER[power_of(o)] * k;
            b->mf[0] = p * fcos(a);
            b->vy = p * fsin(a) * g;
            b->ms[2] = b->ms[0];
            b->ms[0] = 0;
            b->ms[1] = 0;
            b->fx_t = 0;
            if (press)
                run_jumped(r);
            r->sfx |= SFX_BOOM;
            float mx = cx + fcos(a) * 48, my = cy + fsin(a) * 48 * g;
            pb_sparks(r, mx, my, 10, RGB(255, 200, 80), fcos(a) * 6, 5, fsin(a) * 6 * g - 2, fsin(a) * 6 * g + 6, 22, 16);
            pb_sparks(r, mx, my, 8, RGB(170, 170, 185), fcos(a) * 3, 4, 0, 5, 40, 26);
        }
        return;
    }
    if (b->mf[0] > 0) {
        if (b->on_ground)
            b->mf[0] = 0;
        else
            b->x += b->mf[0] / SUB - adv;
    }
    if (b->on_ground && r->buffered && !run_orb_waiting(r)) {
        b->vy = HOP * k * g;
        b->on_ground = 0;
        run_jumped(r);
    }
    b->vy -= (b->mf[0] > 0 ? CAN_G : ROLL_G) * g * (1.0f / SUB);
    if (b->vy * g < -MAX_FALL)
        b->vy = -MAX_FALL * g;
}

static void cannon_turn(Body *b)
{
    if (b->ms[0])
        b->rot = 0;
    else if (b->mf[0] > 0)
        b->rot += 0.32f;
    else
        b->rot += run_speed_px(b->speed) / 38.0f * (b->on_ground ? 1.0f : 0.5f);
    run_wrap_rot(b);
}

// A point of a barrel turned by its aim a, centred on screen (x, y).
static void aimed_poly(float x, float y, float a, int n, const float *pts, color_t c)
{
    float xs[8], ys[8], cs = fcos(a), sn = fsin(a);
    for (int i = 0; i < n; i++) {
        xs[i] = x + pts[2 * i] * cs + pts[2 * i + 1] * sn;
        ys[i] = y - pts[2 * i] * sn + pts[2 * i + 1] * cs;
    }
    draw_poly(n, xs, ys, c);
}

static void aimed_rect(float x, float y, float a, float x0, float y0, float x1, float y1, color_t c)
{
    float p[8] = { x0, y0, x1, y0, x1, y1, x0, y1 };
    aimed_poly(x, y, a, 4, p, c);
}

// The cannonball: an iron ball with a band in his colour 2 and a porthole
// with his cube in it, rolling or tumbling through the air with a smoke
// trail. Loaded, only his head peeks out of the barrel's mouth, the fuse
// fizzes, and dots show the arc the shot will take.
static void cannon_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot, s = veh_scale();
    int flip = b->grav < 0;
    color_t c2 = veh_color(VC_2), iron = RGB(58, 60, 72), shine = RGB(140, 145, 165);
    const Run *r = draw_run();
    if (b->ms[0] && r) {
        const LevelObj *o = &r->lv->objs[b->ms[0] - 1];
        float a = barrel_aim(o, b->ms[1]), g = flip ? -1.0f : 1.0f, p = POWER[power_of(o)] * (b->mini ? MINI_JUMP : 1);
        float vx = p * fcos(a), vy = p * fsin(a) * g, px = 0, py = 0;
        for (int t = 1; t <= 44; t++) {
            px += vx;
            py += vy;
            vy -= CAN_G * g;
            if (t % 3 == 0) {
                int sz = (int)((float)(10 - t / 6) * s) + 2, blink = ((t / 3 + frame / 4) % 4) == 0;
                draw_rect((int)(x + px * s) - sz / 2, (int)(y - py * s) - sz / 2, sz, sz,
                          blink ? RGB(255, 255, 255) : col_lerp(RGB(255, 220, 90), veh_bg(), t * 3));
            }
        }
        float hx = fcos(a) * 26, hy = -fsin(a) * 26 * g;
        veh_pilot(x + hx * s, y + hy * s, -a * g, 0, 0, 0, 34);
        float bx = x - fcos(a) * 44 * s, by = y + fsin(a) * 44 * g * s;
        int left = FUSE - b->ms[1];
        for (int k = 0; k < 4; k++) {
            float sx = bx + (float)((frame * 5 + k * 7) % 13 - 6) * s, sy = by + (float)((frame * 3 + k * 11) % 13 - 6) * s;
            draw_rect((int)sx - 3, (int)sy - 3, 6, 6, k % 2 ? RGB(255, 240, 120) : RGB(255, 120, 30));
        }
        if (left < 60 * SUB && (frame / 4) % 2)
            draw_circle((int)bx, (int)by, (int)(9 * s), RGB(255, 80, 40));
        return;
    }
    if (b->mf[0] > 0) {
        float fl = 24.0f + (float)(frame % 3) * 8;
        float f1[8] = { -30, -22, -30, 22, -46 - fl, 10, -46 - fl, -10 };
        float f2[8] = { -30, -12, -30, 12, -38 - fl * 0.6f, 5, -38 - fl * 0.6f, -5 };
        veh_poly(x, y, 0, flip, 4, f1, RGB(255, 130, 30));
        veh_poly(x, y, 0, flip, 4, f2, RGB(255, 235, 110));
    }
    veh_circle(x, y, 0, flip, 0, 0, 42, col_scale(iron, 2, 3));
    veh_circle(x, y, 0, flip, 0, 0, 38, iron);
    veh_circle(x, y, 0, flip, -13, -15, 10, shine);
    float band[8] = { -36, -6, 36, -6, 36, 6, -36, 6 };
    veh_poly(x, y, rot, flip, 4, band, c2);
    for (int k = 0; k < 4; k++) {
        float a = rot + (float)k * HALF_PI_F + 0.785f;
        veh_circle(x, y, 0, flip, fcos(a) * 27, fsin(a) * 27, 3, shine);
    }
    veh_circle(x, y, 0, flip, 0, 0, 25, RGB(200, 210, 230));
    veh_pilot(x, y, rot * 0.25f, flip, 0, 0, 36);
    if (b->fx_t < 10)
        for (int k = 0; k < 6; k++) {
            float a = (float)k * 1.047f + (float)b->fx_t * 0.2f, rr = (20.0f + (float)b->fx_t * 6) * s;
            draw_circle((int)(x - fcos(a) * rr * 0.6f - (float)b->fx_t * 6 * s), (int)(y + fsin(a) * rr * 0.6f),
                        (int)((12 - b->fx_t) * s) + 2, col_lerp(RGB(255, 210, 120), veh_bg(), b->fx_t * 22));
        }
}

// Smoke puffs behind it while it flies from a barrel.
static void cannon_trail(const float *tx, const float *ty)
{
    const Run *r = draw_run();
    color_t bg = veh_bg();
    float s = veh_scale();
    if (!r || r->b.mf[0] <= 0 || r->b.ms[0])
        return;
    for (int i = TRAIL_N - 1; i >= 2; i -= 2) {
        int rr = (int)((8.0f + (float)i * 1.8f) * s);
        draw_circle((int)veh_sx(tx[i]), (int)veh_sy(ty[i]), rr, col_lerp(i < 5 ? RGB(255, 190, 90) : RGB(190, 190, 200), bg,
                                                                         70 + i * 12));
    }
}

static const float OUTLINE[16] = { -38, 0, -27, -27, 0, -38, 27, -27, 38, 0, 27, 27, 0, 38, -27, 27 };

const ModeDef MODE_CANNON_DEF = {
    .name = "cannon", .label = "CANNON", .portal_name = "portal_cannon",
    .corridor = 0, .half_w = 34.0f, .half_h = 34.0f, .boost = 1.0f, .inputs = IN_CROSS,
    .color = RGB(190, 120, 60),
    .step = cannon_step, .turn = cannon_turn, .draw = cannon_draw, .trail = cannon_trail,
    .noutline = 8, .outline = OUTLINE,
};

// --- The barrel -------------------------------------------------------------

static void barrel_act(Run *r, const LevelObj *o)
{
    Body *b = &r->b;
    int i = pb_index(r, o);
    if (b->mode != MODE_CANNON || b->ms[0] || i < 0 || b->ms[2] == i + 1)
        return;
    barrel_centre(b, o, &b->x, &b->y);
    b->vy = 0;
    b->on_ground = 0;
    b->ms[0] = i + 1;
    b->ms[1] = 0;
    b->mf[0] = 0;
    b->fx_t = 20;
    r->sfx |= SFX_SWAP;
    pb_sparks(r, b->x, b->y, 6, RGB(255, 230, 150), 0, 5, 0, 6, 16, 10);
}

// A bronze barrel turned to its aim (sweeping when it swings), with iron
// bands, a bright mouth and a fuse; empty in a run it shows where it points.
static void barrel_draw(const LevelObj *o, float x, float y, int frame)
{
    const Run *r = draw_run();
    const LevelObj *in = r && r->b.mode == MODE_CANNON && r->b.ms[0] ? &r->lv->objs[r->b.ms[0] - 1] : NULL;
    int loaded = in && in->x == o->x && in->y == o->y;
    int t = loaded ? r->b.ms[1] : swing_of(o) ? frame * SUB : 0;
    float a = barrel_aim(o, t);
    if (r && r->b.grav < 0 && loaded)
        a = -a;
    color_t bronze = RGB(196, 128, 58), dark = RGB(110, 66, 30), band = RGB(70, 72, 84), mouth = RGB(255, 214, 120);
    static const float BODY[16] = { -44, -16, -36, -26, 30, -24, 46, -28, 46, 28, 30, 24, -36, 26, -44, 16 };
    static const float CORE[8] = { -38, -18, 30, -17, 30, 17, -38, 18 };
    aimed_poly(x, y, a, 8, BODY, dark);
    aimed_poly(x, y, a, 4, CORE, bronze);
    aimed_rect(x, y, a, -38, -18, 30, -12, col_lerp(bronze, RGB(255, 255, 255), 70));
    aimed_rect(x, y, a, -18, -25, -10, 25, band);
    aimed_rect(x, y, a, 14, -25, 22, 25, band);
    aimed_rect(x, y, a, 42, -28, 50, 28, band);
    aimed_rect(x, y, a, 44, -20, 48, 20, mouth);
    aimed_rect(x, y, a, -50, -6, -44, 6, RGB(90, 70, 50));
    if (!loaded) {
        float tip = -54.0f - (float)((frame / 3) % 3) * 2;
        aimed_rect(x, y, a, tip - 6, -3, tip, 3, (frame / 4) % 2 ? RGB(255, 230, 120) : RGB(255, 120, 40));
    }
    if (swing_of(o) && !loaded)
        for (int k = -1; k <= 1; k += 2) {
            float e = barrel_aim(o, k > 0 ? 0 : SWING_SUBS / 2);
            for (int d = 0; d < 3; d++)
                aimed_rect(x, y, e, 58 + (float)d * 12, -2, 64 + (float)d * 12, 2, RGB(255, 220, 140));
        }
}

static void barrel_init(LevelObj *o)
{
    o->param = 0;
}

static void barrel_set_step(LevelObj *o, int k, int step)
{
    int aim = aim_index(o), swing = swing_of(o), power = power_of(o);
    if (k == 0)
        aim = (aim + step + 10) % 10;
    else if (k == 1)
        swing = (swing + step + 3) % 3;
    else
        power = (power + step + 3) % 3;
    set_param(o, aim, swing, power);
}

static void barrel_set_text(const LevelObj *o, int k, char *buf, int n)
{
    if (k == 0)
        snprintf(buf, (size_t)n, "%d DEGREES", aim_index(o) * 15 - 45);
    else if (k == 1)
        snprintf(buf, (size_t)n, "%s", SWING_NAMES[swing_of(o)]);
    else
        snprintf(buf, (size_t)n, "%s", POWER_NAMES[power_of(o)]);
}

static void barrel_info(const LevelObj *o, char *buf, int n)
{
    snprintf(buf, (size_t)n, "AIMS %d DEGREES, %s%s", aim_index(o) * 15 - 45, POWER_NAMES[power_of(o)],
             swing_of(o) ? ", SWINGS" : "");
}

const ObjDef OBJ_BARREL_DEF = {
    .name = "barrel", .label = "CANNON BARREL", .cat = PAL_MODES, .kind = K_TRIGGER,
    .x0 = -0.3f, .y0 = -0.3f, .x1 = 1.3f, .y1 = 1.3f,
    .act = barrel_act, .draw = barrel_draw, .init = barrel_init,
    .nset = 3, .set_names = { "AIM", "SWING", "POWER" },
    .set_step = barrel_set_step, .set_text = barrel_set_text, .info = barrel_info,
};
