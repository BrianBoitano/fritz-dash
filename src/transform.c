// Fritz Dash - level transformations: triggers that change the world while
// he plays. MIRROR, FLIP, ZOOM, SHAKE, RAINBOW, INVERT and THEME only change
// how it is drawn (Body.tf, eased a frame at a time, so a checkpoint brings
// them back); MORPH BLOCKS turns a group into spikes, platforms, orbs, air or
// blocks, once or back and forth on the beat, blinking a beat before.
#include <stdio.h>

#include "mode.h"
#include "transform.h"

const float ZOOMS[NUM_ZOOMS] = { 1.0f, 1.3f, 1.6f, 0.8f, 0.65f };
static const char *const ZOOM_NAMES[NUM_ZOOMS] = { "NORMAL", "IN", "CLOSE UP", "OUT", "FAR OUT" };
static const char *const SHAKE_NAMES[3] = { "SMALL", "MEDIUM", "BIG" };
static const char *const MB_NAMES[MB_COUNT] = { "", "SPIKES", "PLATFORMS", "ORBS", "AIR", "BLOCKS" };
static const uint8_t MB_TYPES[MB_COUNT] = { OBJ_NONE, OBJ_SPIKE, OBJ_PLATFORM, OBJ_ORB_YELLOW, OBJ_NONE, OBJ_BLOCK };

const Theme THEMES[NUM_THEMES] = {
    { "NEON", { RGB(10, 4, 40), RGB(62, 10, 92), RGB(20, 6, 52), RGB(0, 240, 255) } },
    { "DAY", { RGB(50, 140, 250), RGB(170, 220, 255), RGB(40, 120, 50), RGB(255, 255, 255) } },
    { "NIGHT", { RGB(2, 4, 24), RGB(18, 26, 78), RGB(6, 8, 28), RGB(140, 170, 255) } },
    { "SPACE", { RGB(0, 0, 0), RGB(24, 0, 44), RGB(10, 0, 22), RGB(210, 130, 255) } },
    { "SNOW", { RGB(120, 170, 220), RGB(225, 238, 255), RGB(190, 214, 240), RGB(40, 110, 210) } },
    { "LAVA", { RGB(36, 0, 0), RGB(150, 30, 0), RGB(56, 8, 0), RGB(255, 180, 0) } },
    { "OCEAN", { RGB(0, 34, 74), RGB(0, 110, 140), RGB(0, 46, 66), RGB(120, 255, 230) } },
    { "CANDY", { RGB(255, 150, 210), RGB(255, 222, 240), RGB(200, 80, 160), RGB(255, 255, 255) } },
};

static void ease(int16_t *t, int on)
{
    if (on && *t < TF_EASE)
        (*t)++;
    if (!on && *t > 0)
        (*t)--;
}

void transform_step(Body *b)
{
    Transform *f = &b->tf;
    ease(&f->mirror_t, f->mirror);
    ease(&f->flip_t, f->flip);
    ease(&f->rainbow_t, f->rainbow);
    ease(&f->invert_t, f->invert);
    if (f->zoom_t < ZOOM_FRAMES)
        f->zoom_t++;
    if (f->theme_t < SWEEP_FRAMES)
        f->theme_t++;
    if (f->shake_t > 0)
        f->shake_t--;
}

void transform_settle(Body *b)
{
    Transform *f = &b->tf;
    f->mirror_t = (int16_t)(f->mirror ? TF_EASE : 0);
    f->flip_t = (int16_t)(f->flip ? TF_EASE : 0);
    f->rainbow_t = (int16_t)(f->rainbow ? TF_EASE : 0);
    f->invert_t = (int16_t)(f->invert ? TF_EASE : 0);
    f->zoom_t = ZOOM_FRAMES;
    f->theme_t = SWEEP_FRAMES;
    f->shake_t = 0;
}

float transform_zoom(const Body *b)
{
    const Transform *f = &b->tf;
    float s = (float)f->zoom_t / ZOOM_FRAMES;
    s = s * s * (3 - 2 * s);
    return ZOOMS[f->zoom_from % NUM_ZOOMS] + (ZOOMS[f->zoom_to % NUM_ZOOMS] - ZOOMS[f->zoom_from % NUM_ZOOMS]) * s;
}

// --- Morph blocks ---------------------------------------------------------------

// Beats since the group's trigger and the frames per beat; a change comes on
// beat 2 (so there is always a beat or more of warning), then every n beats.
static int mb_phase(const Run *r, const Body *b, int g, int *on, int *warn)
{
    int beat = run_beat_at(r, b->t) - b->mb_beat[g], n = b->mb_every[g];
    float frac = run_beat_frac(r, b->t);
    if (beat < 2) {
        *on = 0;
        *warn = 1;
    } else if (!n) {
        *on = 1;
        *warn = 0;
    } else {
        int k = (beat - 2) / n, into = (beat - 2) % n;
        *on = k % 2 == 0;
        *warn = n == 1 ? frac >= 0.5f : into == n - 1;
    }
    return beat;
}

int morph_type(const Run *r, const Body *b, const LevelObj *o)
{
    int g = o->group - 1, on, warn;
    if (g < 0 || g >= MAX_GROUPS || !b->mb_to[g] || !obj_grouped(o))
        return o->type;
    mb_phase(r, b, g, &on, &warn);
    return on ? MB_TYPES[b->mb_to[g] % MB_COUNT] : o->type;
}

int morph_view(const Run *r, const Body *b, const LevelObj *o, int *next)
{
    int g = o->group - 1, on, warn;
    *next = o->type;
    if (g < 0 || g >= MAX_GROUPS || !b->mb_to[g] || !obj_grouped(o))
        return 0;
    mb_phase(r, b, g, &on, &warn);
    int to = MB_TYPES[b->mb_to[g] % MB_COUNT];
    *next = on ? o->type : to;
    return warn && (b->mb_every[g] || !on);
}

// --- Trigger objects ---------------------------------------------------------------

static void mirror_act(Run *r, const LevelObj *o) { r->b.tf.mirror = o->param ? 1 : 0; }
static void flip_act(Run *r, const LevelObj *o) { r->b.tf.flip = o->param ? 1 : 0; }
static void rainbow_act(Run *r, const LevelObj *o) { r->b.tf.rainbow = o->param ? 1 : 0; }
static void invert_act(Run *r, const LevelObj *o) { r->b.tf.invert = o->param ? 1 : 0; }

static void zoom_act(Run *r, const LevelObj *o)
{
    Transform *f = &r->b.tf;
    int now = 0;
    float z = transform_zoom(&r->b), best = 9;
    for (int i = 0; i < NUM_ZOOMS; i++) {
        float d = ZOOMS[i] > z ? ZOOMS[i] - z : z - ZOOMS[i];
        if (d < best) {
            best = d;
            now = i;
        }
    }
    f->zoom_from = (uint8_t)now;
    f->zoom_to = (uint8_t)(o->param % NUM_ZOOMS);
    f->zoom_t = 0;
}

static void shake_act(Run *r, const LevelObj *o)
{
    static const int16_t FRAMES[3] = { 30, 60, 120 }, HARD[3] = { 6, 12, 20 };
    r->b.tf.shake_t = FRAMES[o->param % 3];
    r->b.tf.shake_n = HARD[o->param % 3];
}

static void theme_act(Run *r, const LevelObj *o)
{
    Transform *f = &r->b.tf;
    int k = o->param % NUM_THEMES;
    f->sweep_from = run_colors(r);
    f->theme_from = f->theme_to;
    f->theme_to = (uint8_t)k;
    f->theme_t = 0;
    r->b.col_from = r->b.col_to = THEMES[k].colors;
    r->b.col_t = 256;
}

static void morph_act(Run *r, const LevelObj *o)
{
    Body *b = &r->b;
    int g = o->group - 1;
    if (g < 0 || g >= MAX_GROUPS)
        return;
    b->mb_to[g] = (uint8_t)(o->param % MB_COUNT ? o->param % MB_COUNT : MB_SPIKES);
    b->mb_every[g] = (uint8_t)(o->time > 8 ? 8 : o->time);
    b->mb_beat[g] = (int16_t)run_beat_at(r, b->t);
}

// In the editor a trigger is a marked box with a line down to the ground;
// in play it is invisible.
static void mark(const char *text, color_t c, float x, float y)
{
    if (draw_run())
        return;
    int gy = (int)veh_sy(0);
    draw_rect((int)x - 2, 0, 4, gy, col_scale(c, 1, 2));
    draw_rect((int)x - 32, (int)y - 32, 64, 64, RGB(20, 8, 40));
    draw_frame((int)x - 32, (int)y - 32, 64, 64, 5, c);
    int s = text[1] && text[2] ? 3 : 4;
    draw_text((int)x - text_width(text, s) / 2, (int)y - 7 * s / 2, s, text, RGB(255, 255, 255));
}

static void mirror_draw(const LevelObj *o, float x, float y, int frame) { (void)frame; mark(o->param ? "MIR" : "M-", RGB(80, 220, 255), x, y); }
static void flip_draw(const LevelObj *o, float x, float y, int frame) { (void)frame; mark(o->param ? "FLP" : "F-", RGB(255, 200, 60), x, y); }
static void zoom_draw(const LevelObj *o, float x, float y, int frame) { (void)frame; (void)o; mark("ZM", RGB(120, 255, 120), x, y); }
static void shake_draw(const LevelObj *o, float x, float y, int frame) { (void)frame; (void)o; mark("SHK", RGB(255, 120, 60), x, y); }
static void rainbow_draw(const LevelObj *o, float x, float y, int frame)
{
    static const color_t RAINBOW[6] = { RGB(255, 60, 60), RGB(255, 160, 0), RGB(255, 240, 0),
                                        RGB(60, 255, 90), RGB(40, 160, 255), RGB(200, 80, 255) };
    mark(o->param ? "RB" : "R-", RAINBOW[(frame / 8) % 6], x, y);
}
static void invert_draw(const LevelObj *o, float x, float y, int frame) { (void)frame; mark(o->param ? "INV" : "I-", RGB(230, 230, 230), x, y); }
static void theme_draw(const LevelObj *o, float x, float y, int frame)
{
    (void)frame;
    mark("TH", THEMES[o->param % NUM_THEMES].colors.line, x, y);
}
static void morph_draw(const LevelObj *o, float x, float y, int frame)
{
    char t[8];
    (void)frame;
    snprintf(t, sizeof(t), "MB%d", o->group);
    mark(t, RGB(255, 80, 200), x, y);
}

static void on_init(LevelObj *o) { o->param = 1; }
static void on_step(LevelObj *o, int k, int step) { (void)k; (void)step; o->param = !o->param; }
static void on_text(const LevelObj *o, int k, char *buf, int n) { (void)k; snprintf(buf, (size_t)n, "%s", o->param ? "ON" : "OFF"); }

static void zoom_step(LevelObj *o, int k, int step) { (void)k; o->param = (uint8_t)((o->param + NUM_ZOOMS + step) % NUM_ZOOMS); }
static void zoom_text(const LevelObj *o, int k, char *buf, int n) { (void)k; snprintf(buf, (size_t)n, "%s", ZOOM_NAMES[o->param % NUM_ZOOMS]); }
static void shake_step(LevelObj *o, int k, int step) { (void)k; o->param = (uint8_t)((o->param + 3 + step) % 3); }
static void shake_text(const LevelObj *o, int k, char *buf, int n) { (void)k; snprintf(buf, (size_t)n, "%s", SHAKE_NAMES[o->param % 3]); }
static void theme_init(LevelObj *o) { o->param = 2; }
static void theme_step(LevelObj *o, int k, int step) { (void)k; o->param = (uint8_t)((o->param + NUM_THEMES + step) % NUM_THEMES); }
static void theme_text(const LevelObj *o, int k, char *buf, int n) { (void)k; snprintf(buf, (size_t)n, "%s", THEMES[o->param % NUM_THEMES].name); }

static void morph_init(LevelObj *o)
{
    o->group = 1;
    o->param = MB_SPIKES;
    o->time = 0;
}

static void morph_step(LevelObj *o, int k, int step)
{
    if (k == 0)
        o->group = (uint8_t)((o->group + MAX_GROUPS - 1 + step) % MAX_GROUPS + 1);
    else if (k == 1)
        o->param = (uint8_t)((o->param + MB_COUNT - 2 + step) % (MB_COUNT - 1) + 1);
    else
        o->time = (uint16_t)(o->time == 0 ? (step > 0 ? 1 : 2) : o->time == 1 ? (step > 0 ? 2 : 0) : (step > 0 ? 0 : 1));
}

static void morph_text(const LevelObj *o, int k, char *buf, int n)
{
    if (k == 0)
        snprintf(buf, (size_t)n, "%d", o->group);
    else if (k == 1)
        snprintf(buf, (size_t)n, "%s", MB_NAMES[o->param % MB_COUNT]);
    else if (o->time)
        snprintf(buf, (size_t)n, "BACK AND FORTH EVERY %d BEAT%s", o->time, o->time > 1 ? "S" : "");
    else
        snprintf(buf, (size_t)n, "ONCE");
}

static void morph_info(const LevelObj *o, char *buf, int n)
{
    char when[48];
    morph_text(o, 2, when, sizeof(when));
    snprintf(buf, (size_t)n, "GROUP %d TURNS INTO %s, %s", o->group, MB_NAMES[o->param % MB_COUNT], when);
}

static void say(const LevelObj *o, char *buf, int n, const char *on, const char *off)
{
    snprintf(buf, (size_t)n, "%s", o->param ? on : off);
}
static void mirror_info(const LevelObj *o, char *buf, int n) { say(o, buf, n, "THE SCREEN TURNS LEFT TO RIGHT", "THE SCREEN TURNS BACK"); }
static void flip_info(const LevelObj *o, char *buf, int n) { say(o, buf, n, "THE WORLD GOES UPSIDE DOWN", "THE WORLD TURNS BACK UP"); }
static void rainbow_info(const LevelObj *o, char *buf, int n) { say(o, buf, n, "RAINBOW COLOURS ON", "RAINBOW COLOURS OFF"); }
static void invert_info(const LevelObj *o, char *buf, int n) { say(o, buf, n, "COLOURS TURN INSIDE OUT", "COLOURS BACK TO NORMAL"); }
static void zoom_info(const LevelObj *o, char *buf, int n) { snprintf(buf, (size_t)n, "ZOOM %s", ZOOM_NAMES[o->param % NUM_ZOOMS]); }
static void shake_info(const LevelObj *o, char *buf, int n) { snprintf(buf, (size_t)n, "A %s SHAKE", SHAKE_NAMES[o->param % 3]); }
static void theme_info(const LevelObj *o, char *buf, int n) { snprintf(buf, (size_t)n, "THE WORLD SWEEPS TO %s", THEMES[o->param % NUM_THEMES].name); }

#define TRIGGER .cat = PAL_TRANSFORM, .kind = K_PASS

const ObjDef OBJ_MIRROR_DEF = { .name = "mirror", .label = "MIRROR", TRIGGER, .act = mirror_act, .draw = mirror_draw,
    .init = on_init, .nset = 1, .set_names = { "MIRROR" }, .set_step = on_step, .set_text = on_text, .info = mirror_info };
const ObjDef OBJ_FLIP_DEF = { .name = "flip", .label = "FLIP", TRIGGER, .act = flip_act, .draw = flip_draw,
    .init = on_init, .nset = 1, .set_names = { "UPSIDE DOWN" }, .set_step = on_step, .set_text = on_text, .info = flip_info };
const ObjDef OBJ_ZOOM_DEF = { .name = "zoom", .label = "ZOOM", TRIGGER, .act = zoom_act, .draw = zoom_draw,
    .init = on_init, .nset = 1, .set_names = { "ZOOM" }, .set_step = zoom_step, .set_text = zoom_text, .info = zoom_info };
const ObjDef OBJ_SHAKE_DEF = { .name = "shake", .label = "SHAKE", TRIGGER, .act = shake_act, .draw = shake_draw,
    .init = on_init, .nset = 1, .set_names = { "SHAKE" }, .set_step = shake_step, .set_text = shake_text, .info = shake_info };
const ObjDef OBJ_RAINBOW_DEF = { .name = "rainbow", .label = "RAINBOW", TRIGGER, .act = rainbow_act, .draw = rainbow_draw,
    .init = on_init, .nset = 1, .set_names = { "RAINBOW" }, .set_step = on_step, .set_text = on_text, .info = rainbow_info };
const ObjDef OBJ_INVERT_DEF = { .name = "invert", .label = "INVERT", TRIGGER, .act = invert_act, .draw = invert_draw,
    .init = on_init, .nset = 1, .set_names = { "INVERT" }, .set_step = on_step, .set_text = on_text, .info = invert_info };
const ObjDef OBJ_MORPH_BLOCKS_DEF = { .name = "morph_blocks", .label = "MORPH BLOCKS", TRIGGER, .flags = OF_TIMED,
    .act = morph_act, .draw = morph_draw, .init = morph_init, .nset = 3, .set_names = { "GROUP", "TURNS INTO", "WHEN" },
    .set_step = morph_step, .set_text = morph_text, .info = morph_info };
const ObjDef OBJ_THEME_DEF = { .name = "theme_morph", .label = "THEME MORPH", TRIGGER, .act = theme_act, .draw = theme_draw,
    .init = theme_init, .nset = 1, .set_names = { "THEME" }, .set_step = theme_step, .set_text = theme_text,
    .info = theme_info };
