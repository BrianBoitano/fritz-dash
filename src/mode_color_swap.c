// Fritz Dash - colour swap: he runs and falls like the cube but cannot
// jump; a press swaps him between pink and blue. PINK and BLUE BLOCKS and
// GATES are only solid in his own colour: he stands on blocks of his colour,
// falls through the others, and a gate of his colour is a wall (in any
// other mode they are all solid).
#include <stdio.h>

#include "input.h"
#include "mode.h"
#include "packb.h"

#define SWAP_G   2.56f
#define MAX_FALL 36.0f

static const color_t PAINT[2] = { RGB(255, 60, 170), RGB(40, 150, 255) };

// ms[0]: 0 pink, 1 blue. fx_t: frames since the last swap.
static void swap_step(Run *r, Body *b, int held, int input)
{
    (void)held;
    (void)input;
    float g = (float)b->grav;
    if (r->buffered && !run_orb_waiting(r)) {
        b->ms[0] ^= 1;
        b->fx_t = 0;
        r->buffered = 0;
        r->jumps++;
        r->sfx |= SFX_SWAP;
        pb_sparks(r, b->x, b->y, 12, PAINT[b->ms[0]], 0, 9, -4, 12, 26, 14);
    }
    b->vy -= SWAP_G * g * (1.0f / SUB);
    if (b->vy * g < -MAX_FALL)
        b->vy = -MAX_FALL * g;
}

// A big jelly of paint in his colour of the moment round his cube: a dark
// rim, a glossy shine, drips that hang off its bottom, and a splash ring of
// the new colour on every swap.
static void swap_draw(const Body *b, float x, float y, int frame)
{
    float rot = b->rot, s = veh_scale();
    int flip = b->grav < 0, c = b->ms[0] & 1;
    color_t paint = PAINT[c], old = PAINT[!c], bg = veh_bg(), white = RGB(255, 255, 255);
    float t = b->fx_t < 8 ? (float)b->fx_t / 8 : 1.0f;
    color_t shell = col_lerp(old, paint, (int)(t * 256)), rim = col_scale(shell, 1, 2);
    float wob = b->fx_t < 10 ? fsin((float)b->fx_t * 1.4f) * (float)(10 - b->fx_t) * 0.8f : 0;
    static const float OCT[16] = { -36, -50, 36, -50, 50, -36, 50, 36, 36, 50, -36, 50, -50, 36, -50, -36 };
    float o1[16], o2[16];
    for (int i = 0; i < 16; i++) {
        float k = i % 2 ? 1.0f - wob * 0.01f : 1.0f + wob * 0.01f;
        o1[i] = OCT[i] * k * 1.06f;
        o2[i] = OCT[i] * k;
    }
    for (int i = 0; i < 2; i++) {
        float dx = i ? 22.0f : -18.0f, len = 10.0f + (float)((frame / 3 + i * 7) % 14);
        float drip[8] = { dx - 6, 46, dx + 6, 46, dx + 4, 50 + len, dx - 4, 50 + len };
        veh_poly(x, y, rot, flip, 4, drip, shell);
        veh_circle(x, y, rot, flip, dx, 50 + len, 5, shell);
    }
    veh_poly(x, y, rot, flip, 8, o1, rim);
    veh_poly(x, y, rot, flip, 8, o2, shell);
    veh_square(x, y, rot, flip, 0, 0, 74, col_lerp(shell, white, 70));
    veh_pilot(x, y, rot, flip, 0, 2, 62);
    veh_rect(x, y, rot, flip, -40, -42, -14, -36, white);
    veh_rect(x, y, rot, flip, -44, -38, -38, -14, white);
    veh_rect(x, y, rot, flip, 30, 30, 38, 38, col_lerp(paint, white, 150));
    if (b->fx_t < 12) {
        float rr = (52.0f + (float)b->fx_t * 10) * s;
        color_t rc = col_lerp(paint, bg, b->fx_t * 20);
        for (int k = 0; k < 16; k++) {
            float a = (float)k * 0.3927f;
            int sz = (int)((float)(14 - b->fx_t) * s) + 3;
            draw_rect((int)(x + fcos(a) * rr) - sz / 2, (int)(y + fsin(a) * rr) - sz / 2, sz, sz, rc);
        }
    }
}

// Paint drops behind him in his colour.
static void swap_trail(const float *tx, const float *ty)
{
    const Run *r = draw_run();
    color_t bg = veh_bg(), c = PAINT[r && r->b.mode == MODE_COLOR_SWAP ? r->b.ms[0] & 1 : 0];
    float s = veh_scale();
    for (int i = 1; i < TRAIL_N; i += 2) {
        int sz = (int)((float)(20 - i) * s);
        draw_rect((int)veh_sx(tx[i]) - sz / 2, (int)veh_sy(ty[i]) - sz / 2, sz, sz, col_lerp(c, bg, 60 + i * 13));
    }
}

static const float OUTLINE[16] = { -36, -50, 36, -50, 50, -36, 50, 36, 36, 50, -36, 50, -50, 36, -50, -36 };

const ModeDef MODE_COLOR_SWAP_DEF = {
    .name = "color_swap", .label = "COLOR SWAP", .portal_name = "portal_color_swap",
    .corridor = 0, .half_w = PLAYER_S / 2.0f, .half_h = PLAYER_S / 2.0f, .boost = 1.0f, .inputs = IN_CROSS,
    .color = RGB(150, 90, 255),
    .step = swap_step, .turn = run_turn_cube, .draw = swap_draw, .trail = swap_trail,
    .noutline = 8, .outline = OUTLINE,
};

// --- Pink and blue blocks and gates ------------------------------------------

static int paint_of(const LevelObj *o)
{
    return o->type == OBJ_BLUE_BLOCK || o->type == OBJ_BLUE_GATE;
}

// Solid for colour swap only in his colour, for every other mode always.
static int solid_for(const Run *r, const LevelObj *o)
{
    return r->b.mode != MODE_COLOR_SWAP || (r->b.ms[0] & 1) == paint_of(o);
}

static int paint_now(const Run *r, const LevelObj *o)
{
    return solid_for(r, o) ? K_SOLID : K_NONE;
}

static void dashed_box(int x0, int y0, int w, int h, color_t c)
{
    for (int k = 0; k + 10 <= w; k += 20) {
        draw_rect(x0 + k, y0, 10, 4, c);
        draw_rect(x0 + k, y0 + h - 4, 10, 4, c);
    }
    for (int k = 0; k + 10 <= h; k += 20) {
        draw_rect(x0, y0 + k, 4, 10, c);
        draw_rect(x0 + w - 4, y0 + k, 4, 10, c);
    }
}

// Glossy paint in its colour while it is solid for him; a dashed outline
// while he would fall through it.
static void block_draw(const LevelObj *o, float x, float y, int frame)
{
    const Run *r = draw_run();
    color_t c = PAINT[paint_of(o)];
    int x0 = (int)x - 40, y0 = (int)y - 40;
    (void)frame;
    if (r && !solid_for(r, o)) {
        dashed_box(x0 + 2, y0 + 2, 76, 76, col_lerp(c, veh_bg(), 110));
        return;
    }
    draw_rect(x0, y0, 80, 80, col_scale(c, 1, 2));
    draw_rect(x0 + 5, y0 + 5, 70, 70, c);
    draw_rect(x0 + 12, y0 + 12, 56, 56, col_lerp(c, RGB(255, 255, 255), 60));
    draw_rect(x0 + 10, y0 + 10, 18, 6, RGB(255, 255, 255));
    draw_rect(x0 + 10, y0 + 10, 6, 18, RGB(255, 255, 255));
}

// A bar of light between two posts, crackling; a faint dotted line while
// it lets him through.
static void gate_draw(const LevelObj *o, float x, float y, int frame)
{
    const Run *r = draw_run();
    color_t c = PAINT[paint_of(o)];
    int open = r && !solid_for(r, o);
    if (open) {
        for (int k = 0; k < 80; k += 16)
            draw_rect((int)x - 3, (int)y - 40 + k, 6, 8, col_lerp(c, veh_bg(), 120));
        return;
    }
    int j = (frame + o->y * 3) % 6;
    draw_rect((int)x - 16, (int)y - 40, 32, 80, col_scale(c, 1, 3));
    draw_rect((int)x - 10, (int)y - 40, 20, 80, c);
    draw_rect((int)x - 4 + (j < 3 ? -2 : 2), (int)y - 40, 8, 80, col_lerp(c, RGB(255, 255, 255), 190));
    for (int k = 0; k < 3; k++) {
        int py = (int)y - 36 + ((frame * 5 + k * 27 + o->x * 13) % 72);
        draw_rect((int)x - 14, py, 28, 4, RGB(255, 255, 255));
    }
}

static void paint_info(const LevelObj *o, char *buf, int n)
{
    snprintf(buf, (size_t)n, "SOLID WHEN YOU ARE %s", paint_of(o) ? "BLUE" : "PINK");
}

const ObjDef OBJ_PINK_BLOCK_DEF = {
    .name = "pink_block", .label = "PINK BLOCK", .cat = PAL_MODES, .kind = K_SOLID,
    .x0 = 0, .y0 = 0, .x1 = 1, .y1 = 1, .draw = block_draw, .info = paint_info, .kind_now = paint_now,
};

const ObjDef OBJ_BLUE_BLOCK_DEF = {
    .name = "blue_block", .label = "BLUE BLOCK", .cat = PAL_MODES, .kind = K_SOLID,
    .x0 = 0, .y0 = 0, .x1 = 1, .y1 = 1, .draw = block_draw, .info = paint_info, .kind_now = paint_now,
};

const ObjDef OBJ_PINK_GATE_DEF = {
    .name = "pink_gate", .label = "PINK GATE", .cat = PAL_MODES, .kind = K_SOLID,
    .x0 = 0.3f, .y0 = 0, .x1 = 0.7f, .y1 = 1, .draw = gate_draw, .info = paint_info, .kind_now = paint_now,
};

const ObjDef OBJ_BLUE_GATE_DEF = {
    .name = "blue_gate", .label = "BLUE GATE", .cat = PAL_MODES, .kind = K_SOLID,
    .x0 = 0.3f, .y0 = 0, .x1 = 0.7f, .y1 = 1, .draw = gate_draw, .info = paint_info, .kind_now = paint_now,
};
