// Fritz Dash - the blaster: X jumps like the cube, Square fires a bolt
// straight ahead. A bolt breaks the first CRATE or TARGET it hits (a target
// also blows up every crate in its group, "g"), and fizzles on anything
// else solid. One bolt at a time.
#include <stdio.h>
#include <string.h>

#include "input.h"
#include "mode.h"
#include "packb.h"

#define BL_G      2.56f
#define BL_JUMP   29.9f
#define MAX_FALL  36.0f
#define BOLT_V    46.0f         // px a frame
#define BOLT_LIFE (26 * SUB)
#define BOLT_WAIT (8 * SUB)     // substeps after a bolt ends before the next
#define BOLT_R    12.0f

static int broken(const Body *b, const LevelObj *o, int idx)
{
    if (o->group && (b->pb_groups >> ((o->group - 1) & 7) & 1))
        return 1;
    for (int k = 0; k < 6 && b->pb_broke[k]; k++)
        if (b->pb_broke[k] == idx + 1)
            return 1;
    return 0;
}

static void smash(Run *r, Body *b, int i, float x, float y)
{
    const LevelObj *o = &r->lv->objs[i];
    if (o->type == OBJ_TARGET && o->group) {
        b->pb_groups |= (uint8_t)(1 << ((o->group - 1) & 7));
        for (int k = 0; k < r->lv->nmoving; k++) {
            const LevelObj *c = &r->lv->objs[r->lv->moving[k]];
            float d = (float)c->x * TILE - b->x;
            if (c->group == o->group && c->type == OBJ_CRATE && d > -4 * TILE && d < 20 * TILE)
                pb_sparks(r, ((float)c->x + 0.5f) * TILE, ((float)c->y + 0.5f) * TILE, 3, RGB(200, 130, 60), 0, 8, 0, 14, 40, 22);
        }
    } else {
        int n = 0;
        while (n < 6 && b->pb_broke[n])
            n++;
        if (n == 6) {
            memmove(&b->pb_broke[0], &b->pb_broke[1], sizeof(b->pb_broke[0]) * 5);
            n = 5;
        }
        b->pb_broke[n] = (int16_t)(i + 1);
    }
    if (o->type == OBJ_TARGET) {
        r->sfx |= SFX_BREAK | SFX_COIN;
        pb_sparks(r, x, y, 12, RGB(255, 220, 60), 0, 10, 2, 16, 40, 14);
        pb_sparks(r, x, y, 8, RGB(255, 60, 60), 0, 8, 2, 14, 34, 12);
    } else {
        r->sfx |= SFX_BREAK;
        pb_sparks(r, x, y, 14, RGB(205, 135, 62), 4, 9, 0, 15, 44, 24);
        pb_sparks(r, x, y, 6, RGB(255, 230, 160), 4, 6, 2, 10, 20, 10);
    }
}

// The bolt flies on one substep: the first solid thing in its way stops
// it, breaking it if it is a crate or a target.
static void bolt_fly(Run *r, Body *b)
{
    float x0 = b->mf[0], x1 = x0 + BOLT_V / SUB, by = b->mf[1], best = x1;
    int near[96], n = pb_near(r, x0, x1, near, 96), hit = -1;
    for (int k = 0; k < n; k++) {
        float a, c, d, e;
        int t = r->lv->objs[near[k]].type;
        if (t == OBJ_RAIL || run_obj_now(r, near[k], &a, &c, &d, &e) != K_SOLID)
            continue;
        if (by + BOLT_R <= c || by - BOLT_R >= e || d <= x0 || a > best)
            continue;
        best = a > x0 ? a : x0;
        hit = near[k];
    }
    if (hit < 0) {
        b->mf[0] = x1;
        if (--b->ms[0] == 0)
            b->ms[1] = BOLT_WAIT;
        return;
    }
    int t = r->lv->objs[hit].type;
    if (t == OBJ_CRATE || t == OBJ_TARGET)
        smash(r, b, hit, best, by);
    else
        pb_sparks(r, best, by, 6, RGB(140, 240, 255), -3, 4, -2, 8, 16, 10);
    b->mf[0] = best;
    b->ms[0] = 0;
    b->ms[1] = BOLT_WAIT;
    b->fx_y = best;
    b->fx_t = 0;
}

// ms[0]: substeps the bolt has left (0: none flying); ms[1]: substeps
// until it can fire again; ms[2]: Square was down. mf[0], mf[1]: the bolt
// (world x, y). fx_t: frames since the last shot or hit, fx_y: the hit's x.
static void blaster_step(Run *r, Body *b, int held, int input)
{
    float g = (float)b->grav, k = b->mini ? MINI_JUMP : 1.0f;
    if (b->on_ground && held) {
        b->vy = BL_JUMP * k * g;
        b->on_ground = 0;
        run_jumped(r);
    }
    b->vy -= BL_G * g * (1.0f / SUB);
    if (b->vy * g < -MAX_FALL)
        b->vy = -MAX_FALL * g;
    int sq = (input & IN_SQUARE) != 0;
    if (b->ms[1] > 0)
        b->ms[1]--;
    if (sq && !b->ms[2] && !b->ms[0] && !b->ms[1]) {
        b->ms[0] = BOLT_LIFE;
        b->mf[0] = b->x + run_half_w(b);
        b->mf[1] = b->y + 4 * g;
        b->fx_y = -1;
        b->fx_t = 0;
        r->sfx |= SFX_BLAST;
    }
    b->ms[2] = sq;
    if (b->ms[0])
        bolt_fly(r, b);
}

static void blaster_turn(Body *b)
{
    float a = b->on_ground ? 0 : -b->vy * 0.006f * (float)b->grav;
    b->rot = a > 0.22f ? 0.22f : a < -0.22f ? -0.22f : a;
}

// His cube in a little mech: a visor band, a shoulder pad, legs, and a big
// arm blaster with a Square button on it that lights up when it is ready
// (and kicks back when it fires); the bolt is a streak of plasma with a
// fading tail, with a flash at the muzzle and where it hits.
static void blaster_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot, s = veh_scale();
    int flip = b->grav < 0, ready = !b->ms[0] && !b->ms[1];
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2), dark = veh_color(VC_DARK), bg = veh_bg();
    color_t glow = ready ? ((frame / 8) % 2 ? RGB(90, 255, 200) : RGB(160, 255, 230)) : RGB(255, 150, 40);
    if (b->ms[0]) {
        float bx = veh_sx(b->mf[0]), by = veh_sy(b->mf[1]);
        for (int k = 3; k >= 1; k--) {
            int w = (int)((26.0f - (float)k * 5) * s), h = (int)((18.0f - (float)k * 4) * s);
            draw_rect((int)(bx - (float)k * 26 * s) - w / 2, (int)by - h / 2, w, h, col_lerp(RGB(120, 230, 255), bg, k * 60));
        }
        draw_rect((int)(bx - 34 * s), (int)(by - 11 * s), (int)(56 * s), (int)(22 * s), RGB(90, 200, 255));
        draw_rect((int)(bx - 28 * s), (int)(by - 6 * s), (int)(48 * s), (int)(12 * s), RGB(255, 255, 255));
    }
    if (b->fx_t < 6) {
        float fx = b->fx_y < 0 ? x + 70 * s : veh_sx(b->fx_y);
        float fy = b->fx_y < 0 ? y + (flip ? -4.0f : 4.0f) * s : veh_sy(b->mf[1]);
        int rr = (int)((float)(16 + b->fx_t * 6) * s);
        color_t c = col_lerp(RGB(255, 255, 210), bg, b->fx_t * 36);
        for (int k = 0; k < 8; k++) {
            float a = (float)k * 0.785f + 0.39f;
            draw_rect((int)(fx + fcos(a) * (float)rr) - 4, (int)(fy + fsin(a) * (float)rr) - 4, 8, 8, c);
        }
        if (b->fx_t < 3)
            draw_circle((int)fx, (int)fy, (int)((float)(14 - b->fx_t * 3) * s), RGB(255, 255, 255));
    }
    float step = b->on_ground ? fsin((float)frame * 0.5f) * 5 : 0;
    float kick = b->fx_y < 0 && b->fx_t < 5 ? (float)(5 - b->fx_t) * 3 : 0;
    veh_rect(x, y, rot, flip, -30, 24, -12, 38 + step, col_scale(c2, 1, 2));
    veh_rect(x, y, rot, flip, 8, 24, 26, 38 - step, col_scale(c2, 1, 2));
    veh_rect(x, y, rot, flip, -34, 36 + step, -10, 42 + step, c2);
    veh_rect(x, y, rot, flip, 6, 36 - step, 30, 42 - step, c2);
    veh_pilot(x, y, rot, flip, -8, -4, 64);
    veh_rect(x, y, rot, flip, -42, -40, 26, -31, c2);
    veh_rect(x, y, rot, flip, -26, -38, 16, -33, col_lerp(RGB(90, 220, 255), RGB(255, 255, 255), (frame / 6) % 2 * 90));
    veh_rect(x, y, rot, flip, -38, -48, -18, -40, col_scale(c2, 1, 2));
    veh_rect(x, y, rot, flip, 6 - kick, -10, 46 - kick, 22, col_scale(c2, 2, 5));
    veh_rect(x, y, rot, flip, 10 - kick, -6, 74 - kick, 16, c2);
    veh_rect(x, y, rot, flip, 10 - kick, 0, 74 - kick, 5, col_lerp(c2, RGB(255, 255, 255), 100));
    veh_rect(x, y, rot, flip, 68 - kick, -10, 80 - kick, 20, dark);
    veh_rect(x, y, rot, flip, 70 - kick, -6, 78 - kick, 16, glow);
    veh_rect(x, y, rot, flip, 20 - kick, 16, 32 - kick, 28, dark);
    color_t sqc = ready ? RGB(255, 120, 200) : RGB(120, 70, 100);
    veh_rect(x, y, rot, flip, 34 - kick, -4, 54 - kick, 14, dark);
    veh_rect(x, y, rot, flip, 37 - kick, -1, 51 - kick, 2, sqc);
    veh_rect(x, y, rot, flip, 37 - kick, 8, 51 - kick, 11, sqc);
    veh_rect(x, y, rot, flip, 37 - kick, -1, 40 - kick, 11, sqc);
    veh_rect(x, y, rot, flip, 48 - kick, -1, 51 - kick, 11, sqc);
    veh_rect(x, y, rot, flip, 2, -12, 20, 6, c1);
}

static const float OUTLINE[16] = { -40, -46, 28, -40, 76, -6, 76, 18, 34, 24, 30, 40, -30, 40, -40, 0 };

const ModeDef MODE_BLASTER_DEF = {
    .name = "blaster", .label = "BLASTER", .portal_name = "portal_blaster",
    .corridor = 0, .half_w = PLAYER_S / 2.0f, .half_h = PLAYER_S / 2.0f, .boost = 1.0f, .inputs = IN_CROSS | IN_SQUARE,
    .color = RGB(255, 70, 120),
    .step = blaster_step, .turn = blaster_turn, .draw = blaster_draw,
    .noutline = 8, .outline = OUTLINE,
};

// --- Crates and targets ---------------------------------------------------------

static int crate_now(const Run *r, const LevelObj *o)
{
    const Body *b = &r->b;
    if (!b->pb_broke[0] && !b->pb_groups)
        return K_SOLID;
    return broken(b, o, pb_index(r, o)) ? K_NONE : K_SOLID;
}

static int is_broken(const LevelObj *o)
{
    const Run *r = draw_run();
    return r && crate_now(r, o) == K_NONE;
}

// A wooden crate: planks, a cross brace and nails; gone once broken.
static void crate_draw(const LevelObj *o, float x, float y, int frame)
{
    (void)frame;
    if (is_broken(o))
        return;
    int x0 = (int)x - 40, y0 = (int)y - 40;
    color_t wood = RGB(196, 128, 60), dark = RGB(110, 64, 26), light = RGB(232, 172, 96);
    draw_rect(x0, y0, 80, 80, dark);
    draw_rect(x0 + 5, y0 + 5, 70, 70, wood);
    for (int k = 1; k < 4; k++)
        draw_rect(x0 + 5, y0 + 5 + k * 18, 70, 3, col_scale(wood, 3, 4));
    float bx[4] = { x - 32, x - 22, x + 32, x + 22 }, by[4] = { y - 32, y - 32, y + 32, y + 32 };
    draw_poly(4, bx, by, light);
    float cx[4] = { x + 22, x + 32, x - 22, x - 32 }, cy[4] = { y - 32, y - 32, y + 32, y + 32 };
    draw_poly(4, cx, cy, col_scale(light, 5, 6));
    draw_rect(x0 + 5, y0 + 5, 70, 8, light);
    draw_rect(x0 + 5, y0 + 67, 70, 8, light);
    for (int k = 0; k < 4; k++)
        draw_rect(x0 + (k % 2 ? 66 : 9), y0 + (k / 2 ? 67 : 7), 5, 5, RGB(80, 80, 90));
    if (o->group)
        draw_rect(x0 + 34, y0 + 34, 12, 12, RGB(255, 220, 60));
}

// A bullseye on a steel block that blinks; gone once broken.
static void target_draw(const LevelObj *o, float x, float y, int frame)
{
    if (is_broken(o))
        return;
    int x0 = (int)x - 40, y0 = (int)y - 40, blink = (frame / 10 + o->x) % 4 == 0;
    draw_rect(x0, y0, 80, 80, RGB(70, 74, 90));
    draw_rect(x0 + 4, y0 + 4, 72, 72, RGB(120, 126, 145));
    draw_circle((int)x, (int)y, 33, RGB(255, 255, 255));
    draw_circle((int)x, (int)y, 27, blink ? RGB(255, 120, 60) : RGB(235, 40, 50));
    draw_circle((int)x, (int)y, 19, RGB(255, 255, 255));
    draw_circle((int)x, (int)y, 11, blink ? RGB(255, 120, 60) : RGB(235, 40, 50));
    draw_circle((int)x, (int)y, 4, RGB(255, 255, 255));
    if (o->group)
        for (int k = 0; k < o->group && k < 8; k++)
            draw_rect(x0 + 8 + k * 8, y0 + 68, 5, 5, RGB(255, 220, 60));
}

static void crate_info(const LevelObj *o, char *buf, int n)
{
    if (o->group)
        snprintf(buf, (size_t)n, "A BOLT OR GROUP %d'S TARGET BREAKS IT", o->group);
    else
        snprintf(buf, (size_t)n, "A BLASTER BOLT BREAKS IT");
}

static void target_info(const LevelObj *o, char *buf, int n)
{
    if (o->group)
        snprintf(buf, (size_t)n, "SHOOT IT: GROUP %d'S CRATES BLOW UP", o->group);
    else
        snprintf(buf, (size_t)n, "SHOOT IT (GIVE IT A GROUP FOR CRATES)");
}

const ObjDef OBJ_CRATE_DEF = {
    .name = "crate", .label = "CRATE", .cat = PAL_MODES, .kind = K_SOLID,
    .x0 = 0, .y0 = 0, .x1 = 1, .y1 = 1, .draw = crate_draw, .info = crate_info, .kind_now = crate_now,
};

const ObjDef OBJ_TARGET_DEF = {
    .name = "target", .label = "TARGET", .cat = PAL_MODES, .kind = K_SOLID,
    .x0 = 0, .y0 = 0, .x1 = 1, .y1 = 1, .draw = target_draw, .info = target_info, .kind_now = crate_now,
};
