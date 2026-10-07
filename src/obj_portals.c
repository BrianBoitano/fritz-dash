// Fritz Dash - transforming portals. A SHIFTER cycles through its modes on
// the beat of the level's song and turns the player into the one lit as he
// enters; a MYSTERY shows "?" and picks one of its modes by the frame clock.
// Both act on the attempt's frame clock, so ghosts and solvers see the same.
#include <stdio.h>

#include "mode.h"

int obj_list_len(const LevelObj *o)
{
    int n = 0;
    while (n < MAX_LIST && o->list[n])
        n++;
    return n;
}

// Entry k moves to the next (step 1) or last (step -1) built mode; entries
// after the first can also be empty, and the list closes up behind them.
void obj_list_step(LevelObj *o, int k, int step)
{
    int n = obj_list_len(o);
    if (k > n)
        k = n;
    int m = k < n ? o->list[k] - 1 : -1;
    for (int tries = 0; tries <= MODE_COUNT; tries++) {
        m += step;
        if (m >= MODE_COUNT)
            m = k ? -1 : 0;
        else if (m < (k ? -1 : 0))
            m = MODE_COUNT - 1;
        if (m < 0 || MODES[m])
            break;
    }
    o->list[k] = (uint8_t)(m + 1);
    int w = 0;
    for (int i = 0; i < MAX_LIST; i++)
        if (o->list[i])
            o->list[w++] = o->list[i];
    while (w < MAX_LIST)
        o->list[w++] = 0;
}

void obj_list_text(const LevelObj *o, int k, char *buf, int n)
{
    snprintf(buf, (size_t)n, "%s", k < obj_list_len(o) ? mode_def(o->list[k] - 1)->label : "NONE");
}

static int step_beats(const LevelObj *o)
{
    return o->param ? o->param : 1;
}

// The entry lit at frame t of the attempt (-1 for an empty list).
static int shifter_lit(const Run *r, const LevelObj *o, int t)
{
    int n = obj_list_len(o);
    return n ? (run_beat_at(r, t) / step_beats(o)) % n : -1;
}

static int mystery_pick(const LevelObj *o, int t)
{
    int n = obj_list_len(o);
    uint32_t h = (uint32_t)t * 2654435761u ^ (uint32_t)o->x * 73856093u ^ (uint32_t)o->y * 19349663u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return n ? (int)(h % (uint32_t)n) : -1;
}

static void shifter_act(Run *r, const LevelObj *o)
{
    int k = shifter_lit(r, o, r->b.t);
    if (k >= 0)
        run_set_mode(r, o->list[k] - 1, o);
}

static void mystery_act(Run *r, const LevelObj *o)
{
    int k = mystery_pick(o, r->b.t);
    if (k >= 0)
        run_set_mode(r, o->list[k] - 1, o);
}

// The lit mode inside, the list as dots down the rim (the lit one big),
// and a flash on each beat. In a run it shows what the next frame gives.
static void shifter_draw(const LevelObj *o, float x, float y, int frame)
{
    const Run *r = draw_run();
    int n = obj_list_len(o), k = r ? shifter_lit(r, o, r->b.t + 1) : n ? (frame / 30 / step_beats(o)) % n : -1;
    float ph = r ? run_beat_frac(r, r->b.t + 1) : (float)(frame % 30) / 30.0f;
    int m = k >= 0 ? o->list[k] - 1 : MODE_CUBE;
    color_t c = mode_def(m)->color;
    draw_portal_frame(x, y, col_lerp(RGB(255, 255, 255), c, ph < 0.25f ? (int)(ph * 1024) : 256), frame);
    draw_mode_icon(m, x, y, 0.38f, frame);
    for (int i = 0; i < n; i++) {
        float dy = ((float)i - (float)(n - 1) / 2) * 34.0f;
        int on = i == k, s = on ? 12 : 7;
        draw_rect((int)(x + 44) - s, (int)(y + dy) - s, 2 * s, 2 * s, on ? RGB(255, 255, 255) : mode_def(o->list[i] - 1)->color);
    }
}

static void mystery_draw(const LevelObj *o, float x, float y, int frame)
{
    static const color_t RAINBOW[6] = { RGB(255, 60, 60), RGB(255, 160, 0), RGB(255, 240, 0),
                                        RGB(60, 255, 90), RGB(40, 160, 255), RGB(200, 80, 255) };
    (void)o;
    draw_portal_frame(x, y, RAINBOW[(frame / 8) % 6], frame);
    int bob = (frame / 6) % 4 < 2 ? 0 : 4;
    draw_text_glow((int)x - 14, (int)y - 26 + bob, 8, "?", RGB(255, 255, 255), RAINBOW[(frame / 8 + 3) % 6]);
}

static void list_init(LevelObj *o)
{
    o->list[0] = MODE_SHIP + 1;
    o->list[1] = MODE_UFO + 1;
    o->list[2] = MODE_CUBE + 1;
    o->param = 1;
}

static void portal_set_step(LevelObj *o, int k, int step)
{
    if (k < MAX_LIST)
        obj_list_step(o, k, step);
    else
        o->param = (uint8_t)(step_beats(o) == 1 ? (step > 0 ? 2 : 4) : step_beats(o) == 2 ? (step > 0 ? 4 : 1) :
                             (step > 0 ? 1 : 2));
}

static void portal_set_text(const LevelObj *o, int k, char *buf, int n)
{
    if (k < MAX_LIST)
        obj_list_text(o, k, buf, n);
    else
        snprintf(buf, (size_t)n, "EVERY %d BEAT%s", step_beats(o), step_beats(o) > 1 ? "S" : "");
}

static void list_info(const LevelObj *o, char *buf, int n, const char *what)
{
    int len = snprintf(buf, (size_t)n, "%s", what);
    for (int i = 0; i < obj_list_len(o) && len < n; i++)
        len += snprintf(buf + len, (size_t)(n - len), "%s%s", i ? " " : ": ", mode_def(o->list[i] - 1)->label);
}

static void shifter_info(const LevelObj *o, char *buf, int n)
{
    char modes[64];
    list_info(o, modes, sizeof(modes), "CYCLES");
    snprintf(buf, (size_t)n, "%s  EVERY %d BEAT%s", modes, step_beats(o), step_beats(o) > 1 ? "S" : "");
}

static void mystery_info(const LevelObj *o, char *buf, int n)
{
    list_info(o, buf, n, "SURPRISE");
}

const ObjDef OBJ_SHIFTER_DEF = {
    .name = "shifter", .label = "SHIFTER PORTAL", .cat = PAL_MODES, .kind = K_PORTAL,
    .x0 = 0.1f, .y0 = -1, .x1 = 0.9f, .y1 = 2, .flags = OF_TIMED,
    .act = shifter_act, .draw = shifter_draw, .init = list_init,
    .nset = 5, .set_names = { "MODE 1", "MODE 2", "MODE 3", "MODE 4", "CHANGES" },
    .set_step = portal_set_step, .set_text = portal_set_text, .info = shifter_info,
};

const ObjDef OBJ_MYSTERY_DEF = {
    .name = "mystery", .label = "MYSTERY PORTAL", .cat = PAL_MODES, .kind = K_PORTAL,
    .x0 = 0.1f, .y0 = -1, .x1 = 0.9f, .y1 = 2, .flags = OF_TIMED,
    .act = mystery_act, .draw = mystery_draw, .init = list_init,
    .nset = 4, .set_names = { "MODE 1", "MODE 2", "MODE 3", "MODE 4" },
    .set_step = portal_set_step, .set_text = portal_set_text, .info = mystery_info,
};
