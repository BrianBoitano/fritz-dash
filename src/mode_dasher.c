// Fritz Dash - the dasher: X jumps like the cube (holding keeps jumping),
// and a press in the air dashes straight ahead at double speed for a short
// burst with no gravity, then he falls. One dash per jump; landing or a
// dash crystal (also here) gives it back.
#include <stdio.h>

#include "input.h"
#include "mode.h"

#define DASH_G      2.56f
#define DASH_JUMP   29.9f
#define MAX_FALL    36.0f
#define DASH_FRAMES 13
#define DASH_EXTRA  1.2f        // a dash adds this much of his speed again

// ms[0]: dash substeps left; ms[1]: 1 once the dash is used. fx_y, fx_t:
// where and when the dash began (drawing).
static void dasher_step(Run *r, Body *b, int held, int input)
{
    (void)input;
    float g = (float)b->grav, k = b->mini ? MINI_JUMP : 1.0f;
    if (b->on_ground && !b->ms[0])
        b->ms[1] = 0;
    if (b->ms[0] > 0) {
        b->ms[0]--;
        b->vy = 0;
        b->x += run_speed_px(b->speed) * DASH_EXTRA * (1.0f / SUB);
        return;
    }
    if (b->on_ground && held) {
        b->vy = DASH_JUMP * k * g;
        b->on_ground = 0;
        run_jumped(r);
    } else if (!b->on_ground && r->buffered && !b->ms[1] && !run_orb_waiting(r)) {
        b->ms[0] = DASH_FRAMES * SUB;
        b->ms[1] = 1;
        b->vy = 0;
        b->fx_y = b->x;
        b->fx_t = 0;
        run_jumped(r);
        return;
    }
    b->vy -= DASH_G * g * (1.0f / SUB);
    if (b->vy * g < -MAX_FALL)
        b->vy = -MAX_FALL * g;
}

static void dasher_turn(Body *b)
{
    if (b->ms[0] > 0) {
        b->rot = 0;
        return;
    }
    float a = b->on_ground ? 0 : -b->vy * 0.01f * (float)b->grav;
    b->rot = a > 0.3f ? 0.3f : a < -0.3f ? -0.3f : a;
}

// His cube in a racing helmet with a long scarf and a booster on its back.
// The booster's gem glows while the dash is ready; dashing leaves after-
// images and speed lines, with a ring where it began.
static void dasher_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot, s = veh_scale();
    int flip = b->grav < 0, dashing = b->ms[0] > 0, ready = !b->ms[1];
    color_t c1 = veh_color(VC_1), c2 = veh_color(VC_2), white = veh_color(VC_WHITE), bg = veh_bg();
    color_t gem = ready ? ((frame / 4) % 2 ? RGB(120, 255, 255) : RGB(220, 255, 255)) : RGB(60, 70, 90);
    if (b->fx_t < 10) {
        float px = veh_sx(b->fx_y), r0 = (20.0f + (float)b->fx_t * 9) * s;
        color_t c = col_lerp(RGB(150, 255, 255), bg, 40 + b->fx_t * 20);
        int sz = (int)(9 * s) + 2;
        for (int k = 0; k < 16; k++) {
            float a = (float)k * (2 * PI_F / 16);
            draw_rect((int)(px + fcos(a) * r0) - sz / 2, (int)(y + fsin(a) * r0) - sz / 2, sz, sz, c);
        }
    }
    if (dashing) {
        for (int k = 3; k >= 1; k--) {
            int h = (int)(38 * s * (1.0f - (float)k * 0.1f));
            draw_frame((int)(x - (float)k * 30 * s) - h, (int)y - h, 2 * h, 2 * h, (int)(5 * s) + 1,
                       col_lerp(c1, bg, 70 + k * 45));
        }
        for (int k = 0; k < 6; k++) {
            float ly = (float)(((k * 37 + frame * 13) % 90) - 45) * s;
            float lx = x - (60.0f + (float)((k * 53 + frame * 29) % 120)) * s;
            draw_rect((int)lx, (int)(y + ly), (int)(50 * s), 3, col_lerp(white, bg, 60 + k * 20));
        }
    }
    color_t scarf = RGB(255, 60, 70), scarf2 = RGB(200, 30, 50);
    float lift = dashing ? -12 : b->on_ground ? 8 : 0, ph = (float)frame * 0.5f;
    float sx[5] = { -30, -56, -80, -102, -122 }, sy[5];
    for (int k = 0; k < 5; k++)
        sy[k] = -24 + (k ? fsin(ph - (float)k * 0.9f) * (float)k * 3.5f + lift * (float)k * 0.35f : 0);
    for (int k = 0; k < 4; k++) {
        float w0 = 9 - (float)k * 1.2f, w1 = 9 - (float)(k + 1) * 1.2f;
        float q[8] = { sx[k], sy[k] - w0, sx[k + 1], sy[k + 1] - w1, sx[k + 1], sy[k + 1] + w1, sx[k], sy[k] + w0 };
        veh_poly(x, y, rot, flip, 4, q, k % 2 ? scarf2 : scarf);
    }
    veh_rect(x, y, rot, flip, -54, -20, -36, 22, col_scale(c2, 1, 2));
    veh_rect(x, y, rot, flip, -52, -18, -38, 20, c2);
    veh_circle(x, y, rot, flip, -45, -2, 5, gem);
    if (dashing || !b->on_ground) {
        float fl = dashing ? 34.0f + (float)(frame % 3) * 8 : 12.0f + (float)(frame % 3) * 4;
        float fire[6] = { -54, -12, -54, 14, -54 - fl, 1 }, core[6] = { -54, -6, -54, 8, -54 - fl * 0.6f, 1 };
        veh_poly(x, y, rot, flip, 3, fire, dashing ? RGB(90, 220, 255) : RGB(255, 140, 30));
        veh_poly(x, y, rot, flip, 3, core, RGB(255, 255, 255));
    }
    veh_pilot(x, y, rot, flip, 0, -8, 64);
    for (int k = 0; k < 2; k++) {
        float bx = -16.0f + (float)k * 26, kick = b->on_ground ? fsin((float)frame * 0.6f + (float)k * PI_F) * 4 : 0;
        if (!b->on_ground || dashing) {
            float fl = dashing ? 22.0f + (float)(frame % 3) * 6 : 8.0f + (float)(frame % 3) * 3;
            float jet[6] = { bx - 7 + kick, 38, bx + 7 + kick, 38, bx + kick, 38 + fl };
            veh_poly(x, y, rot, flip, 3, jet, dashing ? RGB(120, 230, 255) : RGB(255, 170, 40));
        }
        veh_rect(x, y, rot, flip, bx - 11 + kick, 20, bx + 11 + kick, 35, RGB(230, 40, 60));
        veh_rect(x, y, rot, flip, bx - 13 + kick, 33, bx + 13 + kick, 38, RGB(245, 245, 250));
        veh_rect(x, y, rot, flip, bx - 3 + kick, 24, bx + 9 + kick, 28, RGB(255, 210, 60));
    }
    float helm[8] = { -40, -30, 26, -46, 44, -30, -40, -24 };
    veh_poly(x, y, rot, flip, 4, helm, c2);
    float visor[8] = { 6, -36, 40, -42, 46, -30, 6, -28 };
    veh_poly(x, y, rot, flip, 4, visor, col_lerp(RGB(120, 230, 255), white, (frame / 3) % 6 == 0 ? 160 : 0));
    float fin[6] = { -30, -36, -14, -40, -40, -58 };
    veh_poly(x, y, rot, flip, 3, fin, c1);
}

static const float OUTLINE[16] = { -40, -40, 26, -46, 44, -30, 38, 38, -38, 38, -54, 22, -122, -24, -54, -20 };

const ModeDef MODE_DASHER_DEF = {
    .name = "dasher", .label = "DASHER", .portal_name = "portal_dasher",
    .corridor = 0, .half_w = PLAYER_S / 2.0f, .half_h = PLAYER_S / 2.0f, .boost = 1.0f, .inputs = IN_CROSS,
    .color = RGB(60, 90, 255),
    .step = dasher_step, .turn = dasher_turn, .draw = dasher_draw,
    .noutline = 8, .outline = OUTLINE,
};

// --- Dash crystal: touching it gives the dasher his dash back.

static void crystal_act(Run *r, const LevelObj *o)
{
    (void)o;
    Body *b = &r->b;
    if (b->mode == MODE_DASHER && b->ms[1]) {
        b->ms[1] = 0;
        r->sfx |= SFX_ORB;
    }
}

// A floating diamond with facets and a twinkle. In a run it shines bright
// while the dasher has used his dash (it would give it back) and rests
// dimmer otherwise.
static void crystal_draw(const LevelObj *o, float x, float y, int frame)
{
    (void)o;
    const Run *r = draw_run();
    int hungry = r && r->b.mode == MODE_DASHER && r->b.ms[1];
    float bob = fsin((float)frame * 0.08f + (float)o->x) * 6, cy = y + bob;
    color_t edge = hungry ? RGB(160, 255, 255) : RGB(90, 200, 230), bg = veh_bg();
    float glow = 34.0f + fsin((float)frame * 0.2f) * 4;
    float gx[4] = { x, x + glow * 0.75f, x, x - glow * 0.75f }, gy[4] = { cy - glow, cy, cy + glow, cy };
    draw_poly(4, gx, gy, col_lerp(edge, bg, hungry ? 150 : 200));
    float ox[4] = { x, x + 22, x, x - 22 }, oy[4] = { cy - 30, cy, cy + 30, cy };
    draw_poly(4, ox, oy, edge);
    float lx[4] = { x, x, x - 14, x - 22 }, ly[4] = { cy - 30, cy + 30, cy + 6, cy };
    draw_poly(4, lx, ly, col_lerp(edge, RGB(255, 255, 255), 150));
    float ix[4] = { x, x + 10, x, x - 10 }, iy[4] = { cy - 14, cy, cy + 14, cy };
    draw_poly(4, ix, iy, RGB(255, 255, 255));
    int tw = (frame + o->x * 13) % 40;
    if (tw < 10) {
        int len = 6 + (tw < 5 ? tw : 10 - tw) * 3;
        draw_rect((int)x + 12 - len, (int)cy - 18, 2 * len, 3, RGB(255, 255, 255));
        draw_rect((int)x + 12, (int)cy - 18 - len, 3, 2 * len, RGB(255, 255, 255));
    }
}

static void crystal_info(const LevelObj *o, char *buf, int n)
{
    (void)o;
    snprintf(buf, (size_t)n, "GIVES THE DASHER HIS DASH BACK");
}

const ObjDef OBJ_DASH_CRYSTAL_DEF = {
    .name = "dash_crystal", .label = "DASH CRYSTAL", .cat = PAL_JUMPS, .kind = K_TRIGGER,
    .x0 = -0.1f, .y0 = -0.1f, .x1 = 1.1f, .y1 = 1.1f,
    .act = crystal_act, .draw = crystal_draw, .info = crystal_info,
};
