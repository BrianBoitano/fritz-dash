// Fritz Dash - easter egg objects: the EGG pickup, FAKE WALL and FAKE FLOOR
// (look solid, pass straight through, and with "c" N hide egg N inside), and
// EGG ZONES that pay out an egg for crossing them a certain way. None of them changes how a level plays
// (OF_COSMETIC): taking an egg only sets Body.eggs and Run.egg_new, which the
// game saves and physics never reads. docs/EGGS.md.
#include <stdio.h>

#include "mode.h"

// Half the egg's width, row by row from the top (32 rows): pointed on top.
static const float EGG_W[32] = { 0.206f, 0.356f, 0.457f, 0.538f, 0.606f, 0.665f, 0.718f, 0.764f, 0.806f, 0.843f,
                                 0.875f, 0.904f, 0.929f, 0.950f, 0.968f, 0.982f, 0.992f, 0.998f, 1.000f, 0.998f,
                                 0.991f, 0.979f, 0.962f, 0.939f, 0.909f, 0.871f, 0.825f, 0.767f, 0.695f, 0.603f,
                                 0.479f, 0.284f };
static const color_t PAINT[12] = { RGB(255, 80, 120), RGB(255, 170, 40), RGB(255, 230, 60), RGB(120, 230, 70),
                                   RGB(40, 210, 200), RGB(70, 150, 255), RGB(170, 110, 255), RGB(255, 120, 220),
                                   RGB(255, 255, 255), RGB(255, 140, 90), RGB(150, 255, 200), RGB(110, 220, 255) };

// One painted row: c1 with the pattern's c2 parts, x0..x1 on screen row y.
static void egg_row(int row, int y, int h, float x0, float x1, int pat, color_t c1, color_t c2)
{
    int w = (int)(x1 - x0 + 0.5f);
    switch (pat) {
    case 0: // stripes
        draw_rect((int)x0, y, w, h, (row >= 8 && row < 11) || (row >= 15 && row < 18) || (row >= 22 && row < 25) ? c2 : c1);
        break;
    case 1: // two colours with a zigzag between
        if (row < 14 || row > 18) {
            draw_rect((int)x0, y, w, h, row < 14 ? c1 : c2);
        } else {
            int seg = w / 6 + 1;
            for (int k = 0; k * seg < w; k++) {
                int up = (k + row) % 2 == 0;
                draw_rect((int)x0 + k * seg, y, seg + 1 > w - k * seg ? w - k * seg : seg + 1, h, up ? c1 : c2);
            }
        }
        break;
    case 2: // a band of checks round the middle
        if (row >= 12 && row < 20) {
            int seg = w / 5 + 1;
            for (int k = 0; k * seg < w; k++)
                draw_rect((int)x0 + k * seg, y, seg + 1 > w - k * seg ? w - k * seg : seg + 1, h,
                          (k + row / 4) % 2 ? c2 : c1);
        } else {
            draw_rect((int)x0, y, w, h, c1);
        }
        break;
    default: // plain: dots or stars go on top
        draw_rect((int)x0, y, w, h, c1);
        break;
    }
}

void draw_egg(float x, float y, float w, float lean, uint32_t paint, int dim, color_t bg)
{
    draw_egg_rows(x, y, w, lean, paint, dim, bg, 0, 32);
}

void draw_egg_rows(float x, float y, float w, float lean, uint32_t paint, int dim, color_t bg, int r0, int r1)
{
    float hgt = w * 1.3f, rh = hgt / 32;
    uint32_t p = paint * 2654435761u;
    p ^= p >> 15;
    color_t c1 = PAINT[p % 12], c2 = PAINT[(p / 12 + 1 + p % 11) % 12];
    int pat = (int)((p >> 20) % 6);
    if (!paint)
        c1 = RGB(255, 236, 170), c2 = RGB(255, 200, 60), pat = 0;
    if (c2 == c1)
        c2 = RGB(255, 255, 255);
    c1 = col_lerp(c1, bg, dim);
    c2 = col_lerp(c2, bg, dim);
    color_t rim = col_lerp(col_scale(c1, 1, 2), bg, dim);
    float top = y - hgt / 2;
    for (int r = r0; r < r1; r++) {
        int y0 = (int)(top + (float)r * rh), y1 = (int)(top + (float)(r + 1) * rh);
        if (y1 <= y0)
            continue;
        float half = EGG_W[r] * w / 2, sh = lean * (16.0f - (float)r) / 16.0f;
        draw_rect((int)(x + sh - half) - 3, y0, (int)(half * 2) + 6, y1 - y0, rim);
    }
    if (r0 == 0)
        draw_rect((int)(x - EGG_W[0] * w / 2 + lean), (int)top - 3, (int)(EGG_W[0] * w) + 1, 4, rim);
    if (r1 == 32)
        draw_rect((int)(x - EGG_W[31] * w / 2 - lean), (int)(top + hgt) - 1, (int)(EGG_W[31] * w) + 1, 4, rim);
    for (int r = r0; r < r1; r++) {
        int y0 = (int)(top + (float)r * rh), y1 = (int)(top + (float)(r + 1) * rh);
        if (y1 <= y0)
            continue;
        float half = EGG_W[r] * w / 2, sh = lean * (16.0f - (float)r) / 16.0f;
        egg_row(r, y0, y1 - y0, x + sh - half, x + sh + half, pat, c1, c2);
    }
    if ((pat == 3 || pat == 4) && r0 == 0 && r1 == 32) {
        static const float SPOTS[6][3] = { { -0.22f, -0.18f, 0.10f }, { 0.20f, -0.05f, 0.12f }, { -0.12f, 0.20f, 0.11f },
                                           { 0.18f, 0.28f, 0.08f }, { -0.30f, 0.06f, 0.07f }, { 0.02f, -0.34f, 0.07f } };
        for (int k = 0; k < 6; k++) {
            float sx = x + SPOTS[k][0] * w - lean * SPOTS[k][1] * 1.6f, sy = y + SPOTS[k][1] * hgt;
            if (pat == 3)
                draw_circle((int)sx, (int)sy, (int)(SPOTS[k][2] * w) + 1, c2);
            else
                ui_star(sx, sy, SPOTS[k][2] * w * 1.5f + 1, c2);
        }
    }
    if (r0 > 8)
        return;
    float hx = x - w * 0.2f + lean * 0.5f, hy = y - hgt * 0.24f;
    draw_rect((int)hx - (int)(w * 0.05f), (int)hy - (int)(w * 0.09f), (int)(w * 0.1f) + 1, (int)(w * 0.18f) + 1,
              col_lerp(RGB(255, 255, 255), bg, dim));
}

// The paint of egg n in a level called name: the same in the level and in
// the SECRETS book.
uint32_t egg_paint(const char *name, int n)
{
    uint32_t h = 2166136261u;
    for (; *name; name++)
        h = (h ^ (uint8_t)*name) * 16777619u;
    return ((h ^ (uint32_t)n) * 16777619u) | 1;
}

void egg_take(Run *r, int n, float x, float y)
{
    if (n < 1 || n > MAX_EGGS || (r->b.eggs >> (n - 1) & 1))
        return;
    r->b.eggs |= (uint8_t)(1 << (n - 1));
    r->egg_new |= 1 << (n - 1);
    r->sfx |= SFX_EGG;
    fx_sparkle(r->parts, x, y, &r->rng);
}

static int is_zone(int type)
{
    return type >= OBJ_EGG_NOJUMP && type <= OBJ_EGG_ORBS;
}

// A zone starts as his centre passes its first column and pays out as it
// passes the end of its last (dx cells long) if he kept to it all the way:
// NO JUMP (no jump and no orb), MINI, UPSIDE DOWN; ORBS pays out on the
// third orb hit inside it.
void egg_zones(Run *r, float x0, float x1, int jumped)
{
    Body *b = &r->b;
    const Level *lv = r->lv;
    for (int i = level_first_at(lv, (int)(x0 / TILE)); i < lv->nobj && lv->objs[i].x * TILE <= x1; i++) {
        const LevelObj *o = &lv->objs[i];
        float tx = (float)o->x * TILE;
        if (is_zone(o->type) && tx > x0 && tx <= x1 && o->param >= 1 && o->param <= MAX_EGGS &&
            !(b->eggs >> (o->param - 1) & 1)) {
            b->zone = (int16_t)(i + 1);
            b->zone_ok = 1;
            b->zone_n = 0;
        }
    }
    if (!b->zone)
        return;
    const LevelObj *o = &lv->objs[b->zone - 1];
    int orb = (r->sfx & SFX_ORB) != 0;
    if (o->type == OBJ_EGG_NOJUMP)
        b->zone_ok &= !jumped && !orb;
    else if (o->type == OBJ_EGG_MINI)
        b->zone_ok &= b->mini != 0;
    else if (o->type == OBJ_EGG_UPSIDE)
        b->zone_ok &= b->grav < 0;
    else
        b->zone_n = (uint8_t)(b->zone_n + orb);
    float end = (float)(o->x + (o->dx > 0 ? o->dx : 1)) * TILE;
    if (b->zone_ok && (o->type == OBJ_EGG_ORBS ? b->zone_n >= 3 : x1 >= end)) {
        b->zone = 0;
        egg_take(r, o->param, b->x, b->y);
    } else if (!b->zone_ok || x1 >= end) {
        b->zone = 0;
    }
}

// --- Drawing --------------------------------------------------------------------

static void egg_draw(const LevelObj *o, float x, float y, int frame)
{
    const Run *r = draw_run();
    int bit = o->param >= 1 && o->param <= MAX_EGGS ? 1 << (o->param - 1) : 0;
    if (r && (r->b.eggs & bit))
        return;
    int had = r && (r->eggs_had & bit);
    float t = (float)frame * 0.11f + (float)o->x * 0.9f, lean = fsin(t) * 7.0f, bob = fsin(t * 0.5f) * 5.0f;
    uint32_t paint = egg_paint(r ? r->lv->name : "", o->param);
    color_t bg = veh_bg();
    if (!had) {
        int pulse = 80 + (int)(fsin(t * 0.7f) * 40.0f);
        draw_egg(x, y + bob, 86, lean, paint, 256 - pulse, bg);
        for (int k = 0; k < 3; k++) {
            int ph = (frame + k * 23 + o->x * 5) % 70;
            if (ph < 24) {
                float a = (float)(k * 2 + o->y) * 1.3f;
                ui_star(x + fcos(a) * 46, y + bob + fsin(a) * 52, (float)(ph < 12 ? ph : 24 - ph) * 0.9f + 2,
                        RGB(255, 255, 230));
            }
        }
    }
    draw_egg(x, y + bob, 60, lean, paint, had ? 170 : 0, bg);
}

static void fake_draw(const LevelObj *o, float x, float y, int frame, int floor)
{
    draw_cell_block(o, floor);
    if (!draw_run()) {
        // The editor marks it: a dashed frame and an F (and the egg it hides).
        for (int k = -36; k < 36; k += 16)
            draw_rect((int)x + k, (int)y - 40, 8, 4, RGB(255, 210, 60));
        draw_text((int)x - 9, (int)y - 10, 3, "F", RGB(255, 210, 60));
        if (o->param)
            draw_egg(x + 22, y + 16, 22, 0, 0, 0, RGB(0, 0, 0));
        return;
    }
    // In play only a glint now and then gives it away.
    int ph = (frame + o->x * 37 + o->y * 11) % 200;
    if (ph < 14) {
        float gx = x - 40 + (float)ph * 6;
        float xs[4] = { gx, gx + 8, gx - 12, gx - 20 }, ys[4] = { y - 38, y - 38, y + 38, y + 38 };
        draw_poly(4, xs, ys, RGB(255, 255, 255));
    }
}

static void wall_draw(const LevelObj *o, float x, float y, int frame) { fake_draw(o, x, y, frame, 0); }
static void floor_draw(const LevelObj *o, float x, float y, int frame) { fake_draw(o, x, y, frame, 1); }

static const char *const ZONE_WORDS[4] = { "NO JUMP", "MINI", "UPSIDE DOWN", "3 ORBS" };

// Zones only show in the editor: a dashed band along their length.
static void zone_draw(const LevelObj *o, float x, float y, int frame)
{
    if (draw_run())
        return;
    color_t c = RGB(255, 200, 60);
    int len = (o->dx > 0 ? o->dx : 1) * TILE, x0 = (int)x - TILE / 2, y0 = (int)y - TILE / 2;
    for (int k = 0; k < len; k += 24) {
        draw_rect(x0 + k, y0, 12, 4, c);
        draw_rect(x0 + k, y0 + TILE - 4, 12, 4, c);
    }
    draw_rect(x0, y0, 4, TILE, c);
    draw_rect(x0 + len - 4, y0, 4, TILE, c);
    draw_egg(x, y - 6, 34, fsin((float)frame * 0.1f) * 4, 0, 0, RGB(0, 0, 0));
    const char *w = ZONE_WORDS[(o->type - OBJ_EGG_NOJUMP) & 3];
    draw_text(x0 + 8, y0 + TILE + 6, 3, w, c);
}

static void number_init(LevelObj *o) { o->param = 1; }
static void zone_init(LevelObj *o)
{
    o->param = 1;
    o->dx = 6;
}

static void hide_step(LevelObj *o, int k, int step)
{
    (void)k;
    o->param = (uint8_t)((o->param + step + MAX_EGGS + 1) % (MAX_EGGS + 1));
}

static void hide_text(const LevelObj *o, int k, char *buf, int n)
{
    (void)k;
    if (o->param)
        snprintf(buf, (size_t)n, "EGG %d", o->param);
    else
        snprintf(buf, (size_t)n, "NONE");
}

static void number_step(LevelObj *o, int k, int step)
{
    if (k == 0)
        o->param = (uint8_t)((o->param - 1 + step + MAX_EGGS) % MAX_EGGS + 1);
    else
        o->dx = (int8_t)((o->dx + step + 39) % 40 + 1);
}

static void number_text(const LevelObj *o, int k, char *buf, int n)
{
    if (k == 0)
        snprintf(buf, (size_t)n, "%d", o->param);
    else
        snprintf(buf, (size_t)n, "%d CELLS", o->dx > 0 ? o->dx : 1);
}

static void egg_info(const LevelObj *o, char *buf, int n) { snprintf(buf, (size_t)n, "EASTER EGG %d: TOUCH IT TO FIND IT", o->param); }
static void wall_info(const LevelObj *o, char *buf, int n)
{
    snprintf(buf, (size_t)n, "LOOKS LIKE A BLOCK - YOU GO STRAIGHT THROUGH%s", o->param ? " (AN EGG INSIDE)" : "");
}
static void floor_info(const LevelObj *o, char *buf, int n)
{
    snprintf(buf, (size_t)n, "LOOKS LIKE A PLATFORM - YOU FALL THROUGH%s", o->param ? " (AN EGG INSIDE)" : "");
}
static void zone_info(const LevelObj *o, char *buf, int n)
{
    snprintf(buf, (size_t)n, "EGG %d FOR CROSSING %d CELLS: %s", o->param, o->dx > 0 ? o->dx : 1,
             ZONE_WORDS[(o->type - OBJ_EGG_NOJUMP) & 3]);
}

#define ZONE .cat = PAL_SPECIAL, .kind = K_NONE, .x0 = 0, .y0 = 0, .x1 = 1, .y1 = 1, .flags = OF_COSMETIC, \
    .draw = zone_draw, .init = zone_init, .nset = 2, .set_names = { "EGG NUMBER", "LENGTH" }, \
    .set_step = number_step, .set_text = number_text, .info = zone_info

const ObjDef OBJ_EGG_DEF = { .name = "egg", .label = "EASTER EGG", .cat = PAL_SPECIAL, .kind = K_NONE,
    .x0 = -0.1f, .y0 = -0.15f, .x1 = 1.1f, .y1 = 1.15f, .flags = OF_COSMETIC, .draw = egg_draw, .init = number_init,
    .nset = 1, .set_names = { "EGG NUMBER" }, .set_step = number_step, .set_text = number_text, .info = egg_info };
const ObjDef OBJ_FAKE_WALL_DEF = { .name = "fake_wall", .label = "FAKE WALL", .cat = PAL_SPECIAL, .kind = K_NONE,
    .x0 = 0, .y0 = 0, .x1 = 1, .y1 = 1, .flags = OF_COSMETIC, .draw = wall_draw, .nset = 1, .set_names = { "HIDES" },
    .set_step = hide_step, .set_text = hide_text, .info = wall_info };
const ObjDef OBJ_FAKE_FLOOR_DEF = { .name = "fake_floor", .label = "FAKE FLOOR", .cat = PAL_SPECIAL, .kind = K_NONE,
    .x0 = 0, .y0 = 0.5f, .x1 = 1, .y1 = 1, .flags = OF_COSMETIC, .draw = floor_draw, .nset = 1, .set_names = { "HIDES" },
    .set_step = hide_step, .set_text = hide_text, .info = floor_info };
const ObjDef OBJ_EGG_NOJUMP_DEF = { .name = "egg_nojump", .label = "EGG ZONE: NO JUMP", ZONE };
const ObjDef OBJ_EGG_MINI_DEF = { .name = "egg_mini", .label = "EGG ZONE: MINI", ZONE };
const ObjDef OBJ_EGG_UPSIDE_DEF = { .name = "egg_upside", .label = "EGG ZONE: UPSIDE DOWN", ZONE };
const ObjDef OBJ_EGG_ORBS_DEF = { .name = "egg_orbs", .label = "EGG ZONE: 3 ORBS", ZONE };
