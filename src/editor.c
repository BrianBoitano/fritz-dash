// Fritz Dash - level editor.
#include <stdio.h>
#include <string.h>

#include "draw.h"
#include "editor.h"
#include "game.h"
#include "platform.h"
#include "songs.h"
#include "mode.h"
#include "synth.h"

#define COL_WHITE  RGB(255, 255, 255)
#define COL_YELLOW RGB(255, 235, 40)
#define COL_CYAN   RGB(0, 230, 255)
#define COL_GREEN  RGB(90, 255, 40)
#define COL_PINK   RGB(255, 50, 200)
#define COL_DIM    RGB(150, 150, 190)
#define COL_PANEL  RGB(10, 6, 26)

#define UNDO_N     24
#define NAME_MAX   20

// The palette, in groups that L2 + L1/R1 jump between: 3.0's objects here,
// then every mode's portal (MODES) and every 3.2 object (OBJECTS) in its
// group, put together the first time the editor opens.
static const struct {
    uint8_t type, cat;
    const char *name;
} BASE[] = {
    { OBJ_BLOCK, PAL_BLOCKS, "BLOCK" }, { OBJ_PLATFORM, PAL_BLOCKS, "PLATFORM" },
    { OBJ_SPIKE, PAL_DANGER, "SPIKE" }, { OBJ_HALF_SPIKE, PAL_DANGER, "HALF SPIKE" }, { OBJ_SAW, PAL_DANGER, "SAW" },
    { OBJ_SAW_BIG, PAL_DANGER, "BIG SAW" },
    { OBJ_PAD_YELLOW, PAL_JUMPS, "YELLOW PAD" }, { OBJ_PAD_PINK, PAL_JUMPS, "PINK PAD" },
    { OBJ_ORB_YELLOW, PAL_JUMPS, "YELLOW ORB" }, { OBJ_ORB_PINK, PAL_JUMPS, "PINK ORB" },
    { OBJ_GRAVITY_UP, PAL_GRAVITY, "GRAVITY UP" }, { OBJ_GRAVITY_DOWN, PAL_GRAVITY, "GRAVITY DOWN" },
    { OBJ_SPEED_05, PAL_GRAVITY, "SPEED 0.5X" }, { OBJ_SPEED_1, PAL_GRAVITY, "SPEED 1X" },
    { OBJ_SPEED_2, PAL_GRAVITY, "SPEED 2X" }, { OBJ_SPEED_3, PAL_GRAVITY, "SPEED 3X" },
    { OBJ_CUBE_PORTAL, PAL_MODES, "CUBE PORTAL" }, { OBJ_SHIP_PORTAL, PAL_MODES, "SHIP PORTAL" },
    { OBJ_BALL_PORTAL, PAL_MODES, "BALL PORTAL" }, { OBJ_UFO_PORTAL, PAL_MODES, "UFO PORTAL" },
    { OBJ_WAVE_PORTAL, PAL_MODES, "WAVE PORTAL" }, { OBJ_MINI_PORTAL, PAL_MODES, "MINI PORTAL" },
    { OBJ_BIG_PORTAL, PAL_MODES, "BIG PORTAL" },
    { OBJ_MOVE, PAL_MOVING, "MOVE TRIGGER" }, { OBJ_PULSE, PAL_MOVING, "PULSE" }, { OBJ_PULSE_BARS, PAL_MOVING, "PULSE BARS" },
    { OBJ_DECO_GLOW, PAL_DECO, "GLOW LINE" }, { OBJ_DECO_STAR, PAL_DECO, "SPARKLE" }, { OBJ_DECO_ARROW, PAL_DECO, "ARROW" },
    { OBJ_DECO_BLOCK, PAL_DECO, "BACK BLOCK" }, { OBJ_DECO_SPIKES, PAL_DECO, "GRASS" },
    { OBJ_COIN, PAL_SPECIAL, "SECRET COIN" }, { OBJ_SIGN, PAL_SPECIAL, "SIGN" }, { OBJ_COLOR, PAL_SPECIAL, "COLOUR CHANGE" },
    { OBJ_END, PAL_SPECIAL, "END WALL" },
};
static const char *const CAT_NAMES[PAL_COUNT] = { "BLOCKS", "DANGER", "JUMPS", "GRAVITY AND SPEED", "MODES", "TRANSFORM",
                                                  "MOVING", "DECORATION", "SPECIAL" };

#define MAX_BRUSH 160
static struct {
    uint8_t type;
    const char *name;
} brushes[MAX_BRUSH];
static int nbrush;
static struct {
    const char *name;
    int first;
} cats[PAL_COUNT];
static int ncats;
static char portal_names[MODE_COUNT][24];

static void add_brush(int type, const char *name)
{
    if (nbrush < MAX_BRUSH) {
        brushes[nbrush].type = (uint8_t)type;
        brushes[nbrush++].name = name;
    }
}

// Mode portals and 3.2 objects of a group (mode portals after the wave's).
static void add_new(int cat)
{
    for (int m = MODE_ROBOT; cat == PAL_MODES && m < MODE_COUNT; m++)
        if (MODES[m]) {
            snprintf(portal_names[m], sizeof(portal_names[m]), "%s PORTAL", MODES[m]->label);
            add_brush(run_mode_portal(m), portal_names[m]);
        }
    for (int t = 0; t < OBJ_COUNT; t++)
        if (obj_def(t) && obj_def(t)->cat == cat)
            add_brush(t, obj_def(t)->label);
}

static void palette_init(void)
{
    if (nbrush)
        return;
    for (int c = 0; c < PAL_COUNT; c++) {
        int first = nbrush;
        for (int i = 0; i < (int)(sizeof(BASE) / sizeof(BASE[0])); i++)
            if (BASE[i].cat == c) {
                add_brush(BASE[i].type, BASE[i].name);
                if (BASE[i].type == OBJ_WAVE_PORTAL)
                    add_new(c);
            }
        if (c != PAL_MODES)
            add_new(c);
        if (nbrush > first) {
            cats[ncats].name = CAT_NAMES[c];
            cats[ncats++].first = first;
        }
    }
}

// Ready-made pieces, rows top first, inserted with their bottom-left corner
// at the cursor (map characters as in tools/gen_levels.py).
static const struct {
    const char *name;
    int h;
    const char *rows[10];
} TEMPLATES[] = {
    { "STAIRCASE", 3, { "..........##", "......######", "..##########" } },
    { "SPIKE PIT", 1, { "###^^^###" } },
    { "ORB CHAIN", 4, { "................", "....o...o...o...", "................", "..^^^^^^^^^^^^.." } },
    { "SHIP TUNNEL", 10, { "........#.....#.....#.....", "........#.....#.....#.....", "........#...........#.....",
                           "........#...........#.....", "........#.................", "........#.................",
                           "..............#...........", "..............#...........", ".S............#.....#...C.",
                           "..............#.....#....." } },
    { "PAD JUMP", 3, { "......#...", "......#...", "..y...#..." } },
    { "UPSIDE DOWN", 5, { "################", "......v.....g...", "................", ".G..............",
                          "................" } },
};
#define NTEMPLATES ((int)(sizeof(TEMPLATES) / sizeof(TEMPLATES[0])))

enum { EM_RESUME, EM_TEST_START, EM_COPY, EM_PASTE, EM_DELETE_SEL, EM_NAME, EM_COLORS, EM_SONG, EM_SPEED,
       EM_GROUPS, EM_SAVE, EM_VERIFY, EM_MAP, EM_TEMPLATES, EM_MUSIC, EM_SEL_GROUP, EM_SAVE_EXIT, EM_COUNT };
enum { EG_BRUSH, EG_MOVE_GROUP, EG_DX, EG_DY, EG_TIME, EG_BACK, EG_COUNT };

static const char *const SPEED_NAMES[4] = { "0.5X", "1X", "2X", "3X" };

// Undo keeps whole copies of the object list; a copy is taken before each
// edit (one per paint stroke), so undo steps back one action at a time.
static struct {
    int nobj;
    LevelObj objs[MAX_LEVEL_OBJS];
} undo_buf[UNDO_N];
static int undo_head;
static char text_buf[512 * 1024];
static Level scratch;

static void toast(Editor *e, const char *m)
{
    snprintf(e->msg, sizeof(e->msg), "%s", m);
    e->msg_frames = 90;
}

static void slot_path(int slot, char *out, size_t n)
{
    snprintf(out, n, "%s/slot%02d.json", LEVELS_DIR, slot + 1);
}

static int read_slot(int slot, Level *lv)
{
    char path[160];
    slot_path(slot, path, sizeof(path));
    int n = plat_read_file(path, text_buf, (int)sizeof(text_buf));
    return n > 0 && n < (int)sizeof(text_buf) && level_parse(lv, text_buf, n) == LEVEL_OK;
}

static void scan_slots(Editor *e, Level *lv)
{
    for (int i = 0; i < EDIT_SLOTS; i++) {
        e->slot_used[i] = read_slot(i, lv);
        snprintf(e->slot_names[i], sizeof(e->slot_names[i]), "%s", e->slot_used[i] ? lv->name : "");
    }
}

void editor_open(Editor *e, Level *lv)
{
    palette_init();
    int sel = e->slot_sel, rate_slot = e->rate_slot, rate_saved = e->rate_saved, nclip = e->nclip;
    Rate rate = e->rate;
    static LevelObj clip[MAX_CLIP];
    memcpy(clip, e->clip, sizeof(clip));
    memset(e, 0, sizeof(*e));
    e->slot_sel = sel;
    e->nclip = nclip;
    memcpy(e->clip, clip, sizeof(clip));
    e->rate = rate;
    e->rate_slot = rate_slot;
    e->rate_saved = rate_saved;
    e->screen = ED_SLOTS;
    e->move_group = 1;
    e->move_dy = 2;
    e->move_time = 60;
    scan_slots(e, lv);
}

static int match_palette(const Level *lv)
{
    for (int i = 0; i < NUM_PALETTES; i++)
        if (!memcmp(&lv->colors, &LEVEL_PALETTES[i], sizeof(LevelColors)))
            return i;
    return -1;
}

static void open_slot(Editor *e, Level *lv, int slot)
{
    if (!read_slot(slot, lv)) {
        level_parse(lv, "{}", 2);
        snprintf(lv->name, sizeof(lv->name), "MY LEVEL %d", slot + 1);
    }
    snprintf(lv->author, sizeof(lv->author), "FRITZ");
    lv->stars = 0;
    e->slot = slot;
    e->screen = ED_EDIT;
    e->cx = 8;
    e->cy = 0;
    e->cam_x = -400;
    e->cam_y = 0;
    e->palette = match_palette(lv);
    e->undo_count = 0;
    e->has_sel = 0;
    e->dirty = 0;
    e->hold_dir = -1;
}

static int save_level(Editor *e, Level *lv);

int editor_remix(Editor *e, Level *lv)
{
    int slot = -1;
    for (int i = EDIT_SLOTS - 1; i >= 0; i--)
        if (!e->slot_used[i])
            slot = i;
    if (slot < 0)
        return -1;
    char name[40];
    snprintf(name, sizeof(name), "%s REMIX", lv->name);
    name[31] = 0;
    snprintf(lv->name, sizeof(lv->name), "%s", name);
    snprintf(lv->author, sizeof(lv->author), "FRITZ");
    lv->stars = 0;
    lv->verified = lv->rated = 0;
    e->slot = slot;
    if (save_level(e, lv) != 0)
        return -1;
    open_slot(e, lv, slot);
    e->slot_sel = slot;
    return slot;
}

static int save_level(Editor *e, Level *lv)
{
    char path[160];
    level_update_end(lv);
    slot_path(e->slot, path, sizeof(path));
    int n = level_write(lv, text_buf, (int)sizeof(text_buf));
    if (n < 0 || plat_write_file(path, text_buf, n) != 0) {
        toast(e, "COULD NOT SAVE!");
        return -1;
    }
    e->dirty = 0;
    e->last_saved = e->slot + 1;
    e->slot_used[e->slot] = 1;
    snprintf(e->slot_names[e->slot], sizeof(e->slot_names[0]), "%s", lv->name);
    // A changed level gets a new suggested difficulty (the search runs in the background).
    uint32_t h = level_hash(lv);
    if (level_rating(lv) < 0 && !(e->rate.state != RATE_IDLE && e->rate.hash == h && e->rate_slot == e->slot)) {
        rate_start(&e->rate, lv);
        e->rate_slot = e->slot;
        e->rate_saved = 0;
    }
    return 0;
}

void editor_verified(Editor *e, Level *lv)
{
    lv->verified = level_hash(lv);
    save_level(e, lv);
}

void editor_tick(Editor *e, Level *lv)
{
    if (e->rate.state != RATE_BUSY || rate_step(&e->rate, RATE_STEPS) != RATE_DONE || e->rate_saved)
        return;
    e->rate_saved = 1;
    if (lv && e->slot == e->rate_slot && level_hash(lv) == e->rate.hash) {
        lv->rating = e->rate.rating;
        lv->rated = e->rate.hash;
    }
    // Into the slot file too, if it still holds the level that was rated.
    if (!read_slot(e->rate_slot, &scratch) || level_hash(&scratch) != e->rate.hash)
        return;
    scratch.rating = e->rate.rating;
    scratch.rated = e->rate.hash;
    char path[160];
    slot_path(e->rate_slot, path, sizeof(path));
    int n = level_write(&scratch, text_buf, (int)sizeof(text_buf));
    if (n > 0)
        plat_write_file(path, text_buf, n);
}

static void push_undo(Editor *e, const Level *lv)
{
    undo_buf[undo_head].nobj = lv->nobj;
    memcpy(undo_buf[undo_head].objs, lv->objs, sizeof(LevelObj) * (size_t)lv->nobj);
    undo_head = (undo_head + 1) % UNDO_N;
    if (e->undo_count < UNDO_N)
        e->undo_count++;
}

static void pop_undo(Editor *e, Level *lv)
{
    if (!e->undo_count) {
        toast(e, "NOTHING TO UNDO");
        return;
    }
    undo_head = (undo_head + UNDO_N - 1) % UNDO_N;
    e->undo_count--;
    lv->nobj = undo_buf[undo_head].nobj;
    memcpy(lv->objs, undo_buf[undo_head].objs, sizeof(LevelObj) * (size_t)lv->nobj);
    e->dirty = 1;
    toast(e, "UNDO");
}

static int is_portal(int type)
{
    return obj_kind(type) == K_PORTAL;
}

static void place(Editor *e, Level *lv)
{
    LevelObj o = { (int16_t)e->cx, (int16_t)e->cy, brushes[e->brush].type, (uint8_t)e->brush_rot, 0, 0, 0, 0, 0, { 0 } };
    o.group = (uint8_t)e->brush_group;
    if (o.type == OBJ_COLOR) {
        o.rot = 0;
        o.param = (uint8_t)e->brush_color;
    }
    if (o.type == OBJ_SIGN)
        o.param = (uint8_t)e->brush_sign;
    if (o.type == OBJ_COIN && level_find(lv, o.x, o.y) < 0) {
        int coins = 0;
        for (int i = 0; i < lv->nobj; i++)
            coins += lv->objs[i].type == OBJ_COIN;
        if (coins >= MAX_COINS) {
            toast(e, "8 COINS IS THE MOST");
            return;
        }
    }
    const ObjDef *d = obj_def(o.type);
    if (d && e->tmpl.type == o.type) {
        o.rot = e->tmpl.rot;
        o.param = e->tmpl.param;
        o.group = e->tmpl.group;
        o.dx = e->tmpl.dx;
        o.dy = e->tmpl.dy;
        o.time = e->tmpl.time;
        memcpy(o.list, e->tmpl.list, sizeof(o.list));
    } else if (d && d->init) {
        d->init(&o);
    }
    if (o.type == OBJ_MOVE) {
        o.group = (uint8_t)e->move_group;
        o.dx = (int8_t)e->move_dx;
        o.dy = (int8_t)e->move_dy;
        o.time = (uint16_t)e->move_time;
    }
    if ((is_portal(o.type) && !d) || o.type == OBJ_END || o.type == OBJ_BLOCK || o.type == OBJ_MOVE ||
        o.type == OBJ_SIGN || o.type == OBJ_COIN)
        o.rot = 0;
    if (o.type == OBJ_COLOR || o.type == OBJ_END)
        o.group = 0;
    if (o.type == OBJ_END)
        for (int i = lv->nobj - 1; i >= 0; i--)
            if (lv->objs[i].type == OBJ_END)
                level_remove(lv, i);
    if (level_put(lv, o) < 0)
        toast(e, "THE LEVEL IS FULL!");
    e->dirty = 1;
}

static void erase(Editor *e, Level *lv)
{
    int i = level_find(lv, e->cx, e->cy);
    if (i >= 0) {
        level_remove(lv, i);
        e->dirty = 1;
    }
}

static void rotate(Editor *e, Level *lv)
{
    int i = level_find(lv, e->cx, e->cy);
    if (i >= 0) {
        LevelObj *o = &lv->objs[i];
        push_undo(e, lv);
        if (o->type == OBJ_COLOR) {
            o->param = (uint8_t)((o->param + 1) % NUM_PALETTES);
            e->brush_color = o->param;
        } else if (o->type == OBJ_SIGN) {
            o->param = (uint8_t)((o->param + 1) % NUM_SIGNS);
            e->brush_sign = o->param;
        } else if (!is_portal(o->type) && o->type != OBJ_END && o->type != OBJ_BLOCK && o->type != OBJ_MOVE) {
            o->rot = (uint8_t)((o->rot + 1) & 3);
            e->brush_rot = o->rot;
        }
        e->dirty = 1;
    } else if (brushes[e->brush].type == OBJ_COLOR) {
        e->brush_color = (e->brush_color + 1) % NUM_PALETTES;
    } else if (brushes[e->brush].type == OBJ_SIGN) {
        e->brush_sign = (e->brush_sign + 1) % NUM_SIGNS;
    } else {
        e->brush_rot = (e->brush_rot + 1) & 3;
    }
}

static int in_sel(const Editor *e, int x, int y)
{
    return x >= e->sel_x0 && x <= e->sel_x1 && y >= e->sel_y0 && y <= e->sel_y1;
}

static void copy_sel(Editor *e, const Level *lv)
{
    e->nclip = 0;
    for (int i = level_first_at(lv, e->sel_x0); i < lv->nobj && lv->objs[i].x <= e->sel_x1; i++) {
        LevelObj o = lv->objs[i];
        if (!in_sel(e, o.x, o.y) || e->nclip >= MAX_CLIP)
            continue;
        o.x = (int16_t)(o.x - e->sel_x0);
        o.y = (int16_t)(o.y - e->sel_y0);
        e->clip[e->nclip++] = o;
    }
}

static void delete_sel(Editor *e, Level *lv)
{
    for (int i = lv->nobj - 1; i >= 0; i--)
        if (in_sel(e, lv->objs[i].x, lv->objs[i].y))
            level_remove(lv, i);
    e->dirty = 1;
}

static void paste(Editor *e, Level *lv)
{
    for (int i = 0; i < e->nclip; i++) {
        LevelObj o = e->clip[i];
        o.x = (int16_t)(o.x + e->cx);
        o.y = (int16_t)(o.y + e->cy);
        if (o.x > LEVEL_MAX_X || o.y > 39)
            continue;
        if (level_put(lv, o) < 0) {
            toast(e, "THE LEVEL IS FULL!");
            break;
        }
    }
    e->dirty = 1;
}

// Moves the cursor: a press moves at once, holding repeats. The right
// stick moves four cells at a time. Holding X paints, holding Square erases.
static void move_cursor(Editor *e, Level *lv, int input)
{
    const int dirs = IN_LEFT | IN_RIGHT | IN_UP | IN_DOWN | IN_RS_LEFT | IN_RS_RIGHT | IN_RS_UP | IN_RS_DOWN;
    int dir = input & dirs, move = 0;
    if (dir != e->hold_dir) {
        e->hold_dir = dir;
        e->hold_timer = 14;
        move = dir != 0;
    } else if (dir && --e->hold_timer <= 0) {
        e->hold_timer = 3;
        move = 1;
    }
    if (!move)
        return;
    int step = (dir & (IN_RS_LEFT | IN_RS_RIGHT | IN_RS_UP | IN_RS_DOWN)) ? 4 : 1;
    if (dir & (IN_LEFT | IN_RS_LEFT)) e->cx -= step;
    if (dir & (IN_RIGHT | IN_RS_RIGHT)) e->cx += step;
    if (dir & (IN_UP | IN_RS_UP)) e->cy += step;
    if (dir & (IN_DOWN | IN_RS_DOWN)) e->cy -= step;
    e->cx = e->cx < 0 ? 0 : e->cx > LEVEL_MAX_X ? LEVEL_MAX_X : e->cx;
    e->cy = e->cy < 0 ? 0 : e->cy > 39 ? 39 : e->cy;
    if (input & IN_CROSS)
        place(e, lv);
    else if (input & IN_SQUARE)
        erase(e, lv);
}

static void follow_cursor(Editor *e)
{
    float sx = (float)e->cx * TILE - e->cam_x, sy = (float)GROUND_Y - ((float)(e->cy + 1) * TILE - e->cam_y);
    float tx = e->cam_x, ty = e->cam_y;
    if (sx < 360) tx = (float)e->cx * TILE - 360;
    if (sx > SCREEN_W - 440) tx = (float)e->cx * TILE - (SCREEN_W - 440);
    if (sy < 150) ty = (float)(e->cy + 1) * TILE - (GROUND_Y - 150);
    if (sy > GROUND_Y - TILE) ty = (float)(e->cy + 1) * TILE - TILE;
    if (ty < 0) ty = 0;
    e->cam_x += (tx - e->cam_x) * 0.35f;
    e->cam_y += (ty - e->cam_y) * 0.35f;
}

static int playtest(Editor *e, Level *lv, int x)
{
    level_update_end(lv);
    if (save_level(e, lv) != 0)
        return EDR_NONE;
    e->test_x = x;
    return EDR_PLAYTEST;
}

static const char *rating_text(const Editor *e, const Level *lv, char *buf, size_t n)
{
    int r = level_rating(lv);
    if (r >= 0)
        snprintf(buf, n, "DIFFICULTY: %s (THE COMPUTER THINKS)", RATING_NAMES[r]);
    else if (e->rate.state == RATE_BUSY && e->rate_slot == e->slot && e->rate.hash == level_hash(lv))
        snprintf(buf, n, "DIFFICULTY: THINKING... %d%%", rate_percent(&e->rate));
    else
        snprintf(buf, n, "DIFFICULTY: SAVE TO FIND OUT");
    return buf;
}

static int map_char(char c, int *rot)
{
    static const char CH[] = "#=^voySCGg";
    static const uint8_t TY[] = { OBJ_BLOCK, OBJ_PLATFORM, OBJ_SPIKE, OBJ_SPIKE, OBJ_ORB_YELLOW, OBJ_PAD_YELLOW,
                                  OBJ_SHIP_PORTAL, OBJ_CUBE_PORTAL, OBJ_GRAVITY_UP, OBJ_GRAVITY_DOWN };
    const char *p = strchr(CH, c);
    *rot = c == 'v' ? 2 : 0;
    return p && c ? TY[p - CH] : OBJ_NONE;
}

int editor_templates(void)
{
    return NTEMPLATES;
}

int editor_template_put(Level *lv, int k, int x, int y)
{
    int h = TEMPLATES[k].h;
    for (int r = 0; r < h; r++)
        for (int c = 0; TEMPLATES[k].rows[r][c]; c++) {
            int rot, t = map_char(TEMPLATES[k].rows[r][c], &rot);
            LevelObj o = { (int16_t)(x + c), (int16_t)(y + h - 1 - r), (uint8_t)t, (uint8_t)rot, 0, 0, 0, 0, 0, { 0 } };
            if (t == OBJ_NONE || o.x > LEVEL_MAX_X || o.y > 39)
                continue;
            if (level_put(lv, o) < 0)
                return -1;
        }
    level_update_end(lv);
    return 0;
}

static void insert_template(Editor *e, Level *lv, int k)
{
    if (editor_template_put(lv, k, e->cx, e->cy) < 0)
        toast(e, "THE LEVEL IS FULL!");
    e->dirty = 1;
    char buf[48];
    snprintf(buf, sizeof(buf), "ADDED A %s", TEMPLATES[k].name);
    toast(e, buf);
}

int editor_frames_to(const Level *lv, int x)
{
    float at = PLAYER_S / 2.0f, f = 0;
    int speed = lv->speed;
    for (int i = 0; i < lv->nobj && lv->objs[i].x < x; i++) {
        int t = lv->objs[i].type;
        if (t < OBJ_SPEED_05 || t > OBJ_SPEED_3)
            continue;
        float px = ((float)lv->objs[i].x + 0.5f) * TILE;
        f += (px - at) / run_speed_px(speed);
        at = px;
        speed = t - OBJ_SPEED_05;
    }
    f += ((float)x * TILE - at) / run_speed_px(speed);
    return f > 0 ? (int)f : 0;
}

static int level_song_number(const Level *lv)
{
    return lv->user_song ? SONG_USER + (lv->user_song - 1) % USER_SONGS : lv->song % NUM_SONGS;
}

static void music_from_cursor(Editor *e, const Level *lv)
{
    synth_song_at(level_song_number(lv), editor_frames_to(lv, e->cx));
    e->music_on = 1;
}

static void music_stop(Editor *e)
{
    if (e->music_on)
        synth_song(-1);
    e->music_on = 0;
}

static int cat_of(int brush)
{
    int c = 0;
    for (int i = 0; i < ncats; i++)
        if (brush >= cats[i].first)
            c = i;
    return c;
}

static int step_edit(Editor *e, Level *lv, int input, int edges)
{
    if (edges & (IN_R2 | IN_TOUCH)) {
        e->screen = ED_MENU;
        e->menu_sel = EM_RESUME;
        return EDR_NONE;
    }
    if (edges & IN_START) {
        music_stop(e);
        return playtest(e, lv, e->cx);
    }
    if (edges & IN_L3) {
        e->screen = ED_MAP;
        e->map_x = e->cx - 4 < 0 ? 0 : e->cx - 4;
        return EDR_NONE;
    }
    // L2 held with L1 or R1 jumps a group of objects in the palette instead of selecting.
    if ((input & IN_L2) && (edges & (IN_L1 | IN_R1))) {
        int c = (cat_of(e->brush) + ((edges & IN_R1) ? 1 : ncats - 1)) % ncats;
        e->brush = cats[c].first;
        e->selecting = 0;
        e->has_sel = e->l2_had_sel;
        synth_sfx(SND_MOVE);
        return EDR_NONE;
    }
    if (edges & IN_L2) {
        e->l2_had_sel = e->has_sel;
        e->anchor_x = e->cx;
        e->anchor_y = e->cy;
        e->selecting = 1;
    }
    move_cursor(e, lv, input);
    if (e->selecting) {
        e->has_sel = 1;
        e->sel_x0 = e->anchor_x < e->cx ? e->anchor_x : e->cx;
        e->sel_x1 = e->anchor_x < e->cx ? e->cx : e->anchor_x;
        e->sel_y0 = e->anchor_y < e->cy ? e->anchor_y : e->cy;
        e->sel_y1 = e->anchor_y < e->cy ? e->cy : e->anchor_y;
        if (!(input & IN_L2))
            e->selecting = 0;
    }

    if (edges & IN_CROSS) {
        push_undo(e, lv);
        place(e, lv);
        synth_sfx(SND_PLACE);
    } else if (edges & IN_SQUARE) {
        push_undo(e, lv);
        erase(e, lv);
        synth_sfx(SND_DELETE);
    }
    int here = level_find(lv, e->cx, e->cy);
    if ((edges & IN_TRIANGLE) && here >= 0 && lv->objs[here].type == OBJ_MOVE) {
        // Triangle on a move trigger changes what it does.
        const LevelObj *m = &lv->objs[here];
        e->move_group = m->group ? m->group : 1;
        e->move_dx = m->dx;
        e->move_dy = m->dy;
        e->move_time = m->time ? m->time : 60;
        e->edit_trigger = here + 1;
        e->group_sel = 1;
        e->screen = ED_GROUPS;
        push_undo(e, lv);
        return EDR_NONE;
    }
    // Triangle on a 3.2 object with settings (or with its brush) opens them.
    const ObjDef *od = obj_def(here >= 0 ? lv->objs[here].type : brushes[e->brush].type);
    if ((edges & IN_TRIANGLE) && od && od->nset) {
        if (here >= 0) {
            push_undo(e, lv);
            e->obj_edit = here + 1;
        } else {
            if (e->tmpl.type != brushes[e->brush].type) {
                memset(&e->tmpl, 0, sizeof(e->tmpl));
                e->tmpl.type = brushes[e->brush].type;
                if (od->init)
                    od->init(&e->tmpl);
            }
            e->obj_edit = -1;
        }
        e->obj_row = 0;
        e->screen = ED_OBJSET;
        return EDR_NONE;
    }
    if (edges & IN_TRIANGLE)
        rotate(e, lv);
    if (edges & IN_L1)
        e->brush = (e->brush + nbrush - 1) % nbrush;
    if (edges & IN_R1)
        e->brush = (e->brush + 1) % nbrush;
    if (edges & (IN_L1 | IN_R1))
        synth_sfx(SND_MOVE);
    if (edges & IN_CIRCLE)
        pop_undo(e, lv);
    return EDR_NONE;
}

static int step_menu(Editor *e, Level *lv, int edges)
{
    if (edges & (IN_CIRCLE | IN_R2 | IN_TOUCH)) {
        e->screen = ED_EDIT;
        return EDR_NONE;
    }
    if (edges & IN_UP)
        e->menu_sel = (e->menu_sel + EM_COUNT - 1) % EM_COUNT;
    if (edges & IN_DOWN)
        e->menu_sel = (e->menu_sel + 1) % EM_COUNT;
    int step = (edges & (IN_CROSS | IN_RIGHT)) ? 1 : (edges & IN_LEFT) ? -1 : 0;
    if (!step)
        return EDR_NONE;
    int pick = edges & IN_CROSS;
    switch (e->menu_sel) {
    case EM_RESUME:
        if (pick)
            e->screen = ED_EDIT;
        break;
    case EM_TEST_START:
        if (pick) {
            music_stop(e);
            return playtest(e, lv, 0);
        }
        break;
    case EM_COPY:
        if (!pick)
            break;
        if (!e->has_sel) {
            toast(e, "HOLD L2 AND MOVE TO SELECT");
            break;
        }
        copy_sel(e, lv);
        snprintf(e->msg, sizeof(e->msg), "COPIED %d", e->nclip);
        e->msg_frames = 90;
        e->screen = ED_EDIT;
        break;
    case EM_PASTE:
        if (!pick)
            break;
        if (!e->nclip) {
            toast(e, "COPY SOMETHING FIRST");
            break;
        }
        push_undo(e, lv);
        paste(e, lv);
        toast(e, "PASTED");
        e->screen = ED_EDIT;
        break;
    case EM_DELETE_SEL:
        if (!pick)
            break;
        if (!e->has_sel) {
            toast(e, "HOLD L2 AND MOVE TO SELECT");
            break;
        }
        push_undo(e, lv);
        delete_sel(e, lv);
        e->has_sel = 0;
        e->screen = ED_EDIT;
        break;
    case EM_NAME:
        if (!pick)
            break;
        kb_open(&e->kb, lv->name, NAME_MAX);
        e->screen = ED_NAME;
        break;
    case EM_COLORS:
        e->palette = ((e->palette < 0 ? 0 : e->palette) + step + NUM_PALETTES) % NUM_PALETTES;
        lv->colors = LEVEL_PALETTES[e->palette];
        e->dirty = 1;
        break;
    case EM_SONG: {
        // The built-in songs, then his own saved ones.
        int n = NUM_SONGS + USER_SONGS, i = lv->user_song ? NUM_SONGS + lv->user_song - 1 : lv->song % NUM_SONGS;
        do
            i = (i + step + n) % n;
        while (i >= NUM_SONGS && !songs_used(i - NUM_SONGS));
        lv->user_song = i >= NUM_SONGS ? i - NUM_SONGS + 1 : 0;
        if (i < NUM_SONGS)
            lv->song = i;
        synth_song(i < NUM_SONGS ? i : SONG_USER + i - NUM_SONGS);
        e->music_on = 1;
        e->dirty = 1;
        break;
    }
    case EM_SPEED:
        lv->speed = (lv->speed + step + 4) % 4;
        e->dirty = 1;
        break;
    case EM_GROUPS:
        if (pick) {
            e->group_sel = 0;
            e->screen = ED_GROUPS;
        }
        break;
    case EM_SAVE:
        if (pick && save_level(e, lv) == 0)
            toast(e, "SAVED!");
        break;
    case EM_VERIFY:
        music_stop(e);
        if (pick && playtest(e, lv, 0) == EDR_PLAYTEST)
            return EDR_VERIFY;
        break;
    case EM_MAP:
        if (pick) {
            e->screen = ED_MAP;
            e->map_x = e->cx - 4 < 0 ? 0 : e->cx - 4;
        }
        break;
    case EM_TEMPLATES:
        if (pick)
            e->screen = ED_TEMPLATES;
        break;
    case EM_MUSIC:
        if (!pick)
            break;
        if (e->music_on) {
            music_stop(e);
        } else {
            music_from_cursor(e, lv);
            e->screen = ED_EDIT;
            toast(e, "MUSIC FROM HERE - R2 MENU STOPS IT");
        }
        break;
    case EM_SEL_GROUP:
        if (!pick) {
            e->sel_group = (e->sel_group + step + MAX_GROUPS + 1) % (MAX_GROUPS + 1);
        } else if (!e->has_sel) {
            toast(e, "HOLD L2 AND MOVE TO SELECT");
        } else {
            push_undo(e, lv);
            int n = 0;
            for (int i = 0; i < lv->nobj; i++) {
                LevelObj *o = &lv->objs[i];
                if (!in_sel(e, o->x, o->y) || o->type == OBJ_MOVE || o->type == OBJ_COLOR || o->type == OBJ_END)
                    continue;
                o->group = (uint8_t)e->sel_group;
                n++;
            }
            level_update_end(lv);
            e->dirty = 1;
            char buf[48];
            if (e->sel_group)
                snprintf(buf, sizeof(buf), "%d OBJECTS IN GROUP %d", n, e->sel_group);
            else
                snprintf(buf, sizeof(buf), "%d OBJECTS IN NO GROUP", n);
            toast(e, buf);
            e->screen = ED_EDIT;
        }
        break;
    case EM_SAVE_EXIT:
        if (pick && save_level(e, lv) == 0) {
            synth_song(-1);
            e->music_on = 0;
            e->screen = ED_SLOTS;
        }
        break;
    }
    return EDR_NONE;
}

static void step_name(Editor *e, Level *lv, int edges)
{
    int r = kb_step(&e->kb, edges);
    if (r == KB_DONE && e->kb.text[0]) {
        snprintf(lv->name, sizeof(lv->name), "%s", e->kb.text);
        e->dirty = 1;
    }
    if (r != KB_TYPING)
        e->screen = ED_MENU;
}

// A 3.2 object's settings (its ObjDef's rows), changed straight in the
// object, or in the brush's settings for new ones.
static void step_objset(Editor *e, Level *lv, int edges)
{
    LevelObj *o = e->obj_edit > 0 ? &lv->objs[e->obj_edit - 1] : &e->tmpl;
    const ObjDef *d = obj_def(o->type);
    int rows = d->nset + 1;
    if (edges & IN_UP)
        e->obj_row = (e->obj_row + rows - 1) % rows;
    if (edges & IN_DOWN)
        e->obj_row = (e->obj_row + 1) % rows;
    if ((edges & IN_CIRCLE) || ((edges & IN_CROSS) && e->obj_row == d->nset)) {
        e->screen = ED_EDIT;
        e->obj_edit = 0;
        return;
    }
    int step = (edges & (IN_RIGHT | IN_CROSS)) ? 1 : (edges & IN_LEFT) ? -1 : 0;
    if (step && e->obj_row < d->nset) {
        d->set_step(o, e->obj_row, step);
        e->tmpl = *o;
        if (e->obj_edit > 0)
            e->dirty = 1;
        synth_sfx(SND_MOVE);
    }
}

// Groups: objects placed go in the brush group; a move trigger moves its
// group by dx, dy cells over move_time frames.
static void step_groups(Editor *e, Level *lv, int edges)
{
    if (edges & IN_UP)
        e->group_sel = (e->group_sel + EG_COUNT - 1) % EG_COUNT;
    if (edges & IN_DOWN)
        e->group_sel = (e->group_sel + 1) % EG_COUNT;
    if ((edges & IN_CIRCLE) || ((edges & IN_CROSS) && e->group_sel == EG_BACK)) {
        e->screen = e->edit_trigger ? ED_EDIT : ED_MENU;
        e->edit_trigger = 0;
        return;
    }
    int step = (edges & (IN_RIGHT | IN_CROSS)) ? 1 : (edges & IN_LEFT) ? -1 : 0;
    switch (e->group_sel) {
    case EG_BRUSH: e->brush_group = (e->brush_group + step + MAX_GROUPS + 1) % (MAX_GROUPS + 1); break;
    case EG_MOVE_GROUP: e->move_group = (e->move_group - 1 + step + MAX_GROUPS) % MAX_GROUPS + 1; break;
    case EG_DX: e->move_dx = e->move_dx + step < -20 ? 20 : e->move_dx + step > 20 ? -20 : e->move_dx + step; break;
    case EG_DY: e->move_dy = e->move_dy + step < -10 ? 10 : e->move_dy + step > 10 ? -10 : e->move_dy + step; break;
    case EG_TIME: e->move_time = (e->move_time / 15 - 1 + step + 20) % 20 * 15 + 15; break;
    }
    if (e->edit_trigger && step) {
        LevelObj *m = &lv->objs[e->edit_trigger - 1];
        m->group = (uint8_t)e->move_group;
        m->dx = (int8_t)e->move_dx;
        m->dy = (int8_t)e->move_dy;
        m->time = (uint16_t)e->move_time;
        e->dirty = 1;
    }
}

// The level map: the whole level small, a box to move, X jumps there.
static void map_layout(const Level *lv, int *cell, int *per_row, int *rows)
{
    int cells = lv->end_x / TILE + 12;
    *cell = 8;
    while (*cell > 2 && (cells + (1800 / *cell) - 1) / (1800 / *cell) > 6)
        (*cell)--;
    *per_row = 1800 / *cell;
    *rows = (cells + *per_row - 1) / *per_row;
}

static void step_map(Editor *e, const Level *lv, int input, int edges)
{
    int cell, per_row, rows;
    map_layout(lv, &cell, &per_row, &rows);
    if (edges & IN_CIRCLE) {
        e->screen = ED_EDIT;
        return;
    }
    if (edges & IN_CROSS) {
        e->cx = e->map_x + 4;
        e->cam_x = (float)e->cx * TILE - 360;
        e->screen = ED_EDIT;
        synth_sfx(SND_SELECT);
        return;
    }
    int dir = input & (IN_LEFT | IN_RIGHT | IN_RS_LEFT | IN_RS_RIGHT), move = 0;
    if (dir != e->hold_dir) {
        e->hold_dir = dir;
        e->hold_timer = 12;
        move = dir != 0;
    } else if (dir && --e->hold_timer <= 0) {
        e->hold_timer = 1;
        move = 1;
    }
    int step = (dir & (IN_RS_LEFT | IN_RS_RIGHT)) ? 8 : 2;
    if (move && (dir & (IN_LEFT | IN_RS_LEFT)))
        e->map_x -= step;
    if (move && (dir & (IN_RIGHT | IN_RS_RIGHT)))
        e->map_x += step;
    if (edges & IN_UP)
        e->map_x -= per_row;
    if (edges & IN_DOWN)
        e->map_x += per_row;
    int last = rows * per_row - 24;
    e->map_x = e->map_x < 0 ? 0 : e->map_x > last ? (last < 0 ? 0 : last) : e->map_x;
}

static void step_templates(Editor *e, Level *lv, int edges)
{
    if (edges & IN_CIRCLE) {
        e->screen = ED_MENU;
        return;
    }
    int sel = e->tmpl_sel;
    if (edges & IN_LEFT) sel = (sel + NTEMPLATES - 1) % NTEMPLATES;
    if (edges & IN_RIGHT) sel = (sel + 1) % NTEMPLATES;
    if (edges & (IN_UP | IN_DOWN)) sel = (sel + NTEMPLATES / 2) % NTEMPLATES;
    if (sel != e->tmpl_sel)
        synth_sfx(SND_MOVE);
    e->tmpl_sel = sel;
    if (edges & IN_CROSS) {
        push_undo(e, lv);
        insert_template(e, lv, sel);
        synth_sfx(SND_PLACE);
        e->screen = ED_EDIT;
    }
}

static int step_slots(Editor *e, Level *lv, int edges)
{
    int sel = e->slot_sel;
    if (edges & IN_LEFT) sel = (sel + EDIT_SLOTS - 1) % EDIT_SLOTS;
    if (edges & IN_RIGHT) sel = (sel + 1) % EDIT_SLOTS;
    if (edges & IN_UP) sel = (sel + EDIT_SLOTS - 5) % EDIT_SLOTS;
    if (edges & IN_DOWN) sel = (sel + 5) % EDIT_SLOTS;
    if (sel != e->slot_sel) {
        e->slot_sel = sel;
        e->slot_delete = 0;
    }
    if (edges & IN_CIRCLE)
        return EDR_EXIT;
    if (edges & IN_CROSS) {
        open_slot(e, lv, sel);
    } else if ((edges & IN_SQUARE) && e->slot_used[sel]) {
        if (e->slot_delete == sel + 1) {
            char path[160];
            slot_path(sel, path, sizeof(path));
            plat_remove_file(path);
            scan_slots(e, lv);
            e->slot_delete = 0;
            toast(e, "DELETED");
        } else {
            e->slot_delete = sel + 1;
            toast(e, "PRESS SQUARE AGAIN TO DELETE IT");
        }
    }
    return EDR_NONE;
}

int editor_step(Editor *e, Level *lv, int input, int edges)
{
    palette_init();
    int r = EDR_NONE;
    if (e->msg_frames > 0)
        e->msg_frames--;
    switch (e->screen) {
    case ED_SLOTS:
        r = step_slots(e, lv, edges);
        break;
    case ED_EDIT:
        r = step_edit(e, lv, input, edges);
        if (e->dirty && ++e->autosave >= 3600) {
            e->autosave = 0;
            save_level(e, lv);
        }
        break;
    case ED_MENU:
        r = step_menu(e, lv, edges);
        break;
    case ED_NAME:
        step_name(e, lv, edges);
        break;
    case ED_GROUPS:
        step_groups(e, lv, edges);
        break;
    case ED_MAP:
        step_map(e, lv, input, edges);
        break;
    case ED_TEMPLATES:
        step_templates(e, lv, edges);
        break;
    case ED_OBJSET:
        step_objset(e, lv, edges);
        break;
    }
    if (e->screen != ED_SLOTS)
        follow_cursor(e);
    return r;
}

void editor_resume(Editor *e)
{
    e->screen = ED_EDIT;
    e->hold_dir = -1;
}

// ---------------------------------------------------------------------------
// Drawing

static int cell_sx(const Editor *e, int x) { return (int)((float)x * TILE - e->cam_x); }
static int cell_sy(const Editor *e, int y) { return (int)((float)GROUND_Y - ((float)(y + 1) * TILE - e->cam_y)); }

static void draw_grid(const Editor *e, const Level *lv)
{
    color_t c = col_lerp(lv->colors.bottom, lv->colors.line, 70);
    int ground = (int)((float)GROUND_Y + e->cam_y);
    int x0 = (int)(e->cam_x / TILE) - 1;
    for (int x = x0; x < x0 + 27; x++)
        if (x >= 0)
            draw_rect(cell_sx(e, x), 0, 2, ground, c);
    for (int y = 0; y < 40; y++) {
        int sy = cell_sy(e, y) + TILE;
        if (sy >= 0 && sy <= ground)
            draw_rect(0, sy, SCREEN_W, 2, c);
    }
    int sx = cell_sx(e, 0);
    if (sx > -60 && sx < SCREEN_W)
        draw_text_glow(sx + 8, ground - 260, 4, "START", COL_GREEN, RGB(30, 140, 10));
}

static void move_text(char *buf, size_t n, int g, int dx, int dy, int time)
{
    snprintf(buf, n, "MOVES GROUP %d  %s %d  %s %d  %d.%d SEC", g, dx < 0 ? "LEFT" : "RIGHT", dx < 0 ? -dx : dx,
             dy < 0 ? "DOWN" : "UP", dy < 0 ? -dy : dy, time / 60, time % 60 / 6);
}

// Triggers are invisible in play, so the editor marks them; objects in a
// group show its number.
static void draw_marked_colors(const Editor *e, const Level *lv)
{
    int x0 = (int)(e->cam_x / TILE) - 1;
    for (int i = level_first_at(lv, x0); i < lv->nobj && lv->objs[i].x < x0 + 27; i++) {
        const LevelObj *o = &lv->objs[i];
        int sx = cell_sx(e, o->x), sy = cell_sy(e, o->y);
        char num[4];
        snprintf(num, sizeof(num), "%d", o->group);
        if (o->type == OBJ_MOVE) {
            color_t c = RGB(170, 70, 255);
            draw_rect(sx + TILE / 2 - 2, 0, 4, (int)((float)GROUND_Y + e->cam_y), col_scale(c, 1, 2));
            draw_rect(sx + 8, sy + 8, TILE - 16, TILE - 16, RGB(40, 10, 70));
            draw_frame(sx + 8, sy + 8, TILE - 16, TILE - 16, 5, c);
            draw_text(sx + 18, sy + 26, 4, "M", COL_WHITE);
            draw_text(sx + 44, sy + 26, 4, num, COL_YELLOW);
            int len = (o->dx || o->dy) ? 30 : 0, ax = o->dx > 0 ? len : o->dx < 0 ? -len : 0, ay = o->dy > 0 ? -len : o->dy < 0 ? len : 0;
            draw_rect(sx + TILE / 2 + ax - 5, sy + TILE / 2 + ay - 5, 10, 10, COL_YELLOW);
            continue;
        }
        if (obj_grouped(o)) {
            draw_rect(sx + 2, sy + 2, 22, 30, RGB(40, 10, 70));
            draw_text(sx + 6, sy + 6, 3, num, COL_YELLOW);
        }
        if (o->type != OBJ_COLOR)
            continue;
        const LevelColors *p = &LEVEL_PALETTES[o->param % NUM_PALETTES];
        draw_rect(sx + TILE / 2 - 2, 0, 4, (int)((float)GROUND_Y + e->cam_y), col_scale(p->line, 1, 2));
        draw_rect(sx + 8, sy + 8, TILE - 16, TILE - 16, p->bottom);
        draw_frame(sx + 8, sy + 8, TILE - 16, TILE - 16, 5, p->line);
        draw_text(sx + 30, sy + 26, 4, "C", COL_WHITE);
    }
}

static void obj_info(const LevelObj *o, char *buf, size_t n);

static void draw_bar(const Editor *e, const Level *lv)
{
    char buf[96];
    draw_rect(0, 0, SCREEN_W, 60, COL_PANEL);
    snprintf(buf, sizeof(buf), "SLOT %d  %s%s", e->slot + 1, lv->name, e->dirty ? " *" : "");
    draw_text_glow(24, 14, 4, buf, COL_WHITE, COL_CYAN);
    snprintf(buf, sizeof(buf), "X %d  Y %d   %d / %d OBJECTS", e->cx, e->cy, lv->nobj, MAX_LEVEL_OBJS);
    draw_text(SCREEN_W - 24 - text_width(buf, 4), 16, 4, buf, COL_DIM);
    draw_text(24, 70, 3, "X PLACE  SQUARE DELETE  TRIANGLE TURN  L1 R1 OBJECT  CIRCLE UNDO  "
                         "L2 SELECT  OPTIONS TEST HERE  R2 MENU", RGB(220, 220, 255));
    draw_text(24, 96, 3, "L2 + L1 R1: NEXT GROUP OF OBJECTS   L3: LEVEL MAP", RGB(170, 170, 220));

    // What is under the cursor.
    int here = level_find(lv, e->cx, e->cy);
    char info[96];
    obj_info(here >= 0 ? &lv->objs[here] : NULL, info, sizeof(info));
    draw_rect(0, SCREEN_H - 172, SCREEN_W, 52, RGB(6, 4, 18));
    draw_text_glow(24, SCREEN_H - 160, 4, info, here >= 0 ? COL_WHITE : COL_DIM, RGB(40, 40, 90));

    int y = SCREEN_H - 120;
    draw_rect(0, y, SCREEN_W, 120, COL_PANEL);
    draw_rect(0, y, SCREEN_W, 4, COL_CYAN);
    // The brush in the middle and two each side. Long names shrink and are
    // pushed out past the one before them; one with no room left is not drawn.
    static const int OFFSET[3] = { 0, 400, 700 }, SCALE[3] = { 6, 4, 3 };
    int cx = SCREEN_W / 2, limit = SCREEN_W / 2 - 140;
    int s0 = ui_fit_scale(brushes[e->brush].name, 900, SCALE[0]), tw = text_width(brushes[e->brush].name, s0);
    draw_frame(cx - tw / 2 - 20, y + 16, tw + 40, 64, 5, COL_GREEN);
    draw_text_glow(cx - tw / 2, y + 49 - 7 * s0 / 2, s0, brushes[e->brush].name, COL_WHITE, COL_CYAN);
    for (int side = -1; side <= 1; side += 2) {
        int edge = tw / 2 + 50;
        for (int k = 1; k <= 2; k++) {
            const char *name = brushes[(e->brush + side * k + nbrush) % nbrush].name;
            int room = limit - edge, s = SCALE[k];
            while (s > 2 && text_width(name, s) > room)
                s--;
            int w = text_width(name, s), mid = OFFSET[k];
            if (w > room)
                break;
            if (mid - w / 2 < edge)
                mid = edge + w / 2;
            if (mid + w / 2 > limit)
                mid = limit - w / 2;
            draw_text(cx + side * mid - w / 2, y + 48 - 3 * s, s, name, COL_DIM);
            edge = mid + w / 2 + 30;
        }
    }
    const char *cat = cats[cat_of(e->brush)].name;
    draw_text_glow(SCREEN_W - 24 - text_width(cat, 4), SCREEN_H - 160, 4, cat, COL_GREEN, RGB(30, 140, 10));
    draw_text(24, y + 40, 4, "< L1", COL_YELLOW);
    draw_text(SCREEN_W - 24 - text_width("R1 >", 4), y + 40, 4, "R1 >", COL_YELLOW);
    if (brushes[e->brush].type == OBJ_COLOR)
        snprintf(buf, sizeof(buf), "FADES TO %s", PALETTE_NAMES[e->brush_color]);
    else if (brushes[e->brush].type == OBJ_MOVE)
        move_text(buf, sizeof(buf), e->move_group, e->move_dx, e->move_dy, e->move_time);
    else if (brushes[e->brush].type == OBJ_SIGN)
        snprintf(buf, sizeof(buf), "SAYS: %s   (TRIANGLE CHANGES IT)", SIGN_TEXTS[e->brush_sign]);
    else if (obj_def(brushes[e->brush].type) && obj_def(brushes[e->brush].type)->nset)
        snprintf(buf, sizeof(buf), "TRIANGLE: ITS SETTINGS");
    else if (e->brush_group)
        snprintf(buf, sizeof(buf), "TURNED %d   IN GROUP %d", e->brush_rot * 90, e->brush_group);
    else
        snprintf(buf, sizeof(buf), "TURNED %d", e->brush_rot * 90);
    draw_text((SCREEN_W - text_width(buf, 3)) / 2, y + 92, 3, buf, COL_YELLOW);
}

static const char *brush_name(int type)
{
    for (int i = 0; i < nbrush; i++)
        if (brushes[i].type == type)
            return brushes[i].name;
    return "?";
}

static void obj_info(const LevelObj *o, char *buf, size_t n)
{
    if (!o) {
        snprintf(buf, n, "HERE: NOTHING");
        return;
    }
    char extra[64] = "";
    const ObjDef *d = obj_def(o->type);
    if (d && d->info)
        d->info(o, extra, sizeof(extra));
    else if (o->type == OBJ_MOVE)
        move_text(extra, sizeof(extra), o->group, o->dx, o->dy, o->time);
    else if (o->type == OBJ_COLOR)
        snprintf(extra, sizeof(extra), "FADES TO %s", PALETTE_NAMES[o->param % NUM_PALETTES]);
    else if (o->type == OBJ_SIGN)
        snprintf(extra, sizeof(extra), "SAYS %s", SIGN_TEXTS[o->param % NUM_SIGNS]);
    else if (o->group)
        snprintf(extra, sizeof(extra), "TURNED %d   IN GROUP %d", o->rot * 90, o->group);
    else if (o->rot)
        snprintf(extra, sizeof(extra), "TURNED %d", o->rot * 90);
    snprintf(buf, n, "HERE: %s   %s", brush_name(o->type), extra);
}

// One object as a small coloured square (level map and ready-made pieces).
static void draw_mini(int x, int y, int s, const LevelObj *o, const LevelColors *c)
{
    color_t col;
    int t = o->type, h = s;
    switch (t) {
    case OBJ_BLOCK: col = c->line; break;
    case OBJ_PLATFORM: col = c->line; h = s / 2 > 0 ? s / 2 : 1; break;
    case OBJ_SPIKE: case OBJ_HALF_SPIKE: case OBJ_SAW: case OBJ_SAW_BIG: col = RGB(255, 70, 60); break;
    case OBJ_PAD_YELLOW: case OBJ_ORB_YELLOW: col = COL_YELLOW; break;
    case OBJ_PAD_PINK: case OBJ_ORB_PINK: col = COL_PINK; break;
    case OBJ_COIN: col = RGB(255, 200, 0); break;
    case OBJ_END: col = COL_WHITE; break;
    case OBJ_MOVE: col = RGB(170, 70, 255); break;
    case OBJ_COLOR: col = LEVEL_PALETTES[o->param % NUM_PALETTES].line; break;
    case OBJ_SIGN: col = RGB(255, 220, 120); break;
    default:
        if (obj_kind(t) == K_PORTAL) {
            col = t == OBJ_GRAVITY_UP ? COL_YELLOW : t == OBJ_GRAVITY_DOWN ? RGB(40, 150, 255) :
                  t >= OBJ_SPEED_05 && t <= OBJ_SPEED_3 ? RGB(255, 150, 30) :
                  run_portal_mode(t) >= MODE_ROBOT ? mode_def(run_portal_mode(t))->color : COL_PINK;
            draw_rect(x, y - s, s, s * 3, col);
            return;
        }
        col = col_lerp(c->bottom, c->line, 90);
        break;
    }
    draw_rect(x, y + (s - h), s, h, col);
}

static void draw_map(const Editor *e, const Level *lv, int frame)
{
    int cell, per_row, rows;
    map_layout(lv, &cell, &per_row, &rows);
    int rh = 16 * cell + 24, top = 150;
    draw_gradient(lv->colors.top, lv->colors.bottom);
    draw_text_glow_centered(40, 8, "LEVEL MAP", COL_WHITE, COL_CYAN);
    for (int r = 0; r < rows; r++) {
        int y0 = top + r * (rh + 14);
        draw_rect(60 - 4, y0 - 4, per_row * cell + 8, rh + 8, col_scale(lv->colors.line, 1, 2));
        draw_rect(60, y0, per_row * cell, rh, RGB(6, 4, 18));
        draw_rect(60, y0 + rh - 4, per_row * cell, 4, lv->colors.line);
    }
    for (int i = 0; i < lv->nobj; i++) {
        const LevelObj *o = &lv->objs[i];
        if (o->y >= 16)
            continue;
        int r = o->x / per_row, x = 60 + (o->x % per_row) * cell, y = top + r * (rh + 14) + rh - 4 - (o->y + 1) * cell;
        if (r < rows)
            draw_mini(x, y, cell, o, &lv->colors);
    }
    // The box: what the editor shows from there, and the cursor's column.
    int r = e->map_x / per_row, bx = 60 + (e->map_x % per_row) * cell, by = top + r * (rh + 14);
    color_t bc = (frame / 8) % 2 ? COL_YELLOW : COL_WHITE;
    draw_frame(bx - 4, by - 8, 24 * cell + 8, rh + 16, 4, bc);
    int ex = lv->end_x / TILE, er = ex / per_row;
    if (er < rows)
        draw_text(60 + (ex % per_row) * cell - 20, top + er * (rh + 14) + rh + 4, 2, "END", COL_WHITE);
    char buf[64];
    snprintf(buf, sizeof(buf), "COLUMN %d OF %d", e->map_x + 4, ex);
    draw_text_glow_centered(SCREEN_H - 150, 5, buf, COL_YELLOW, RGB(200, 120, 0));
    draw_text_glow_centered(SCREEN_H - 80, 4, "LEFT RIGHT MOVE (RIGHT STICK FASTER)   UP DOWN ROW   X GO THERE   CIRCLE BACK",
                            RGB(220, 220, 255), RGB(90, 90, 200));
}

static void draw_templates(const Editor *e, const Level *lv)
{
    int w = 1720, h = 900, x = (SCREEN_W - w) / 2, y = 90;
    ui_panel(x, y, w, h, COL_GREEN);
    draw_text_glow_centered(y + 24, 7, "READY-MADE PIECES", COL_WHITE, COL_GREEN);
    for (int k = 0; k < NTEMPLATES; k++) {
        int px = x + 40 + (k % (NTEMPLATES / 2)) * 560, py = y + 130 + (k / (NTEMPLATES / 2)) * 330, on = k == e->tmpl_sel;
        ui_panel(px, py, 520, 300, on ? COL_YELLOW : RGB(80, 60, 140));
        draw_text_glow(px + (520 - text_width(TEMPLATES[k].name, 5)) / 2, py + 18, 5, TEMPLATES[k].name,
                       on ? COL_WHITE : COL_DIM, on ? COL_GREEN : RGB(40, 30, 80));
        int hh = TEMPLATES[k].h, ww = (int)strlen(TEMPLATES[k].rows[0]), s = 440 / (ww > 22 ? ww : 22);
        if (s * hh > 190)
            s = 190 / hh;
        int ox = px + (520 - ww * s) / 2, base = py + 270;
        draw_rect(px + 20, base, 480, 4, lv->colors.line);
        for (int r = 0; r < hh; r++)
            for (int c = 0; c < ww; c++) {
                int rot, t = map_char(TEMPLATES[k].rows[r][c], &rot);
                LevelObj o = { 0, 0, (uint8_t)t, (uint8_t)rot, 0, 0, 0, 0, 0, { 0 } };
                if (t != OBJ_NONE)
                    draw_mini(ox + c * s, base - (hh - r) * s, s, &o, &lv->colors);
            }
    }
    draw_text_glow_centered(y + h - 90, 4, "IT GOES IN WITH ITS BOTTOM LEFT CORNER ON THE CURSOR", COL_YELLOW, RGB(200, 120, 0));
    draw_text_glow_centered(y + h - 40, 3, "D-PAD PICK   X PUT IT IN   CIRCLE BACK (CIRCLE IN THE EDITOR UNDOES IT)",
                            RGB(220, 220, 255), RGB(90, 90, 200));
}

static void draw_menu_panel(const Editor *e, const Level *lv)
{
    char items[EM_COUNT][48];
    snprintf(items[EM_RESUME], 48, "BACK TO BUILDING");
    snprintf(items[EM_TEST_START], 48, "PLAY FROM THE START");
    snprintf(items[EM_COPY], 48, "COPY SELECTION");
    snprintf(items[EM_PASTE], 48, "PASTE AT CURSOR");
    snprintf(items[EM_DELETE_SEL], 48, "DELETE SELECTION");
    snprintf(items[EM_NAME], 48, "NAME: %s", lv->name);
    snprintf(items[EM_COLORS], 48, "COLOURS: %s", e->palette < 0 ? "FROM FILE" : PALETTE_NAMES[e->palette]);
    if (lv->user_song)
        snprintf(items[EM_SONG], 48, "SONG: %s (YOURS)", songs_used(lv->user_song - 1) ? songs_name(lv->user_song - 1) : "EMPTY");
    else
        snprintf(items[EM_SONG], 48, "SONG: %s", SONG_NAMES[lv->song % NUM_SONGS]);
    snprintf(items[EM_SPEED], 48, "START SPEED: %s", SPEED_NAMES[lv->speed & 3]);
    snprintf(items[EM_GROUPS], 48, "GROUPS AND MOVES");
    snprintf(items[EM_SAVE], 48, "SAVE");
    snprintf(items[EM_VERIFY], 48, level_verified(lv) ? "VERIFIED! BEAT IT AGAIN" : "VERIFY: BEAT IT FROM THE START");
    snprintf(items[EM_MAP], 48, "LEVEL MAP (L3)");
    snprintf(items[EM_TEMPLATES], 48, "READY-MADE PIECES");
    snprintf(items[EM_MUSIC], 48, e->music_on ? "STOP THE MUSIC" : "HEAR THE MUSIC FROM HERE");
    if (e->sel_group)
        snprintf(items[EM_SEL_GROUP], 48, "SELECTION TO GROUP: < %d >", e->sel_group);
    else
        snprintf(items[EM_SEL_GROUP], 48, "SELECTION TO GROUP: < NONE >");
    snprintf(items[EM_SAVE_EXIT], 48, "SAVE AND EXIT");
    // Two columns, read down the left one and then the right one (Up and Down
    // go through them in that order).
    int w = 1720, h = 940, x = (SCREEN_W - w) / 2, y = 70, half = (EM_COUNT + 1) / 2;
    ui_panel(x, y, w, h, COL_CYAN);
    draw_text_glow_centered(y + 22, 7, "EDITOR MENU", COL_WHITE, COL_CYAN);
    for (int i = 0; i < EM_COUNT; i++) {
        int col = i >= half, cx = x + w / 4 + col * (w / 2), yy = y + 120 + (i - col * half) * 72;
        int scale = ui_fit_scale(items[i], w / 2 - 80, 5);
        if (i == e->menu_sel) {
            int tw = text_width(items[i], scale) + 40;
            draw_frame(cx - tw / 2, yy - 12, tw, 7 * scale + 24, 4, COL_GREEN);
            draw_text_glow(cx - text_width(items[i], scale) / 2, yy, scale, items[i], COL_WHITE, COL_CYAN);
        } else {
            draw_text(cx - text_width(items[i], scale) / 2, yy, scale, items[i], COL_DIM);
        }
    }
    char buf[64];
    draw_text_glow_centered(y + h - 170, 5, rating_text(e, lv, buf, sizeof(buf)), COL_YELLOW, RGB(200, 120, 0));
    draw_text_glow_centered(y + h - 110, 4, level_verified(lv) ? "VERIFIED: YOU BEAT IT FROM THE START" :
                            "NOT VERIFIED YET: BEAT IT FROM THE START TO VERIFY IT",
                            level_verified(lv) ? COL_GREEN : COL_DIM, level_verified(lv) ? RGB(30, 140, 10) : RGB(40, 40, 80));
    draw_text_glow_centered(y + h - 50, 3, "UP DOWN MOVE   X PICK   LEFT RIGHT CHANGE   CIRCLE BACK",
                            RGB(220, 220, 255), RGB(90, 90, 200));
}

static void draw_groups(const Editor *e)
{
    const char *title = e->edit_trigger ? "THIS MOVE TRIGGER" : "GROUPS AND MOVES";
    char items[EG_COUNT][48];
    if (e->brush_group)
        snprintf(items[EG_BRUSH], 48, "NEW OBJECTS GO IN GROUP: < %d >", e->brush_group);
    else
        snprintf(items[EG_BRUSH], 48, "NEW OBJECTS GO IN GROUP: < NONE >");
    snprintf(items[EG_MOVE_GROUP], 48, "MOVE TRIGGER MOVES GROUP: < %d >", e->move_group);
    snprintf(items[EG_DX], 48, "%s: < %d >", e->move_dx < 0 ? "MOVE LEFT" : "MOVE RIGHT", e->move_dx < 0 ? -e->move_dx : e->move_dx);
    snprintf(items[EG_DY], 48, "%s: < %d >", e->move_dy < 0 ? "MOVE DOWN" : "MOVE UP", e->move_dy < 0 ? -e->move_dy : e->move_dy);
    snprintf(items[EG_TIME], 48, "TIME: < %d.%d SECONDS >", e->move_time / 60, e->move_time % 60 / 6);
    snprintf(items[EG_BACK], 48, "BACK");
    int w = 1300, h = 760, x = (SCREEN_W - w) / 2, y = 160;
    ui_panel(x, y, w, h, RGB(170, 70, 255));
    draw_text_glow_centered(y + 30, 7, title, COL_WHITE, RGB(170, 70, 255));
    for (int i = 0; i < EG_COUNT; i++) {
        int yy = y + 150 + i * 80, scale = ui_fit_scale(items[i], w - 120, 5);
        if (i == e->group_sel) {
            int tw = text_width(items[i], scale) + 40;
            draw_frame((SCREEN_W - tw) / 2, yy - 12, tw, 7 * scale + 24, 4, COL_GREEN);
            draw_text_glow_centered(yy, scale, items[i], COL_WHITE, COL_CYAN);
        } else {
            draw_text((SCREEN_W - text_width(items[i], scale)) / 2, yy, scale, items[i], COL_DIM);
        }
    }
    draw_text_glow_centered(y + h - 110, 3, e->edit_trigger ? "CHANGES GO STRAIGHT INTO THE TRIGGER UNDER THE CURSOR" :
                            "PUT OBJECTS IN A GROUP, THEN PLACE A MOVE TRIGGER BEFORE THEM", COL_YELLOW, RGB(200, 120, 0));
    draw_text_glow_centered(y + h - 50, 3, "UP DOWN MOVE   LEFT RIGHT CHANGE   CIRCLE BACK", RGB(220, 220, 255),
                            RGB(90, 90, 200));
}

static void draw_objset(const Editor *e, const Level *lv)
{
    const LevelObj *o = e->obj_edit > 0 ? &lv->objs[e->obj_edit - 1] : &e->tmpl;
    const ObjDef *d = obj_def(o->type);
    char item[64], val[40];
    int w = 1300, h = 200 + (d->nset + 1) * 80 + 130, x = (SCREEN_W - w) / 2, y = 160;
    ui_panel(x, y, w, h, RGB(170, 70, 255));
    draw_text_glow_centered(y + 30, 7, d->label, COL_WHITE, RGB(170, 70, 255));
    for (int i = 0; i <= d->nset; i++) {
        if (i < d->nset) {
            d->set_text(o, i, val, sizeof(val));
            snprintf(item, sizeof(item), "%s: < %s >", d->set_names[i], val);
        } else {
            snprintf(item, sizeof(item), "BACK");
        }
        int yy = y + 150 + i * 80, scale = ui_fit_scale(item, w - 120, 5);
        if (i == e->obj_row) {
            int tw = text_width(item, scale) + 40;
            draw_frame((SCREEN_W - tw) / 2, yy - 12, tw, 7 * scale + 24, 4, COL_GREEN);
            draw_text_glow_centered(yy, scale, item, COL_WHITE, COL_CYAN);
        } else {
            draw_text((SCREEN_W - text_width(item, scale)) / 2, yy, scale, item, COL_DIM);
        }
    }
    draw_text_glow_centered(y + h - 110, 3, e->obj_edit > 0 ? "CHANGES GO STRAIGHT INTO THE ONE UNDER THE CURSOR" :
                            "NEW ONES YOU PLACE GET THESE", COL_YELLOW, RGB(200, 120, 0));
    draw_text_glow_centered(y + h - 50, 3, "UP DOWN MOVE   LEFT RIGHT CHANGE   CIRCLE BACK", RGB(220, 220, 255),
                            RGB(90, 90, 200));
}

static void draw_slots(const Editor *e, int frame)
{
    LevelColors c = LEVEL_PALETTES[7];
    run_draw_backdrop(c, (float)frame * 2.0f, 0);
    draw_text_glow_centered(50, 9, "MY LEVELS", COL_WHITE, COL_PINK);
    for (int i = 0; i < EDIT_SLOTS; i++) {
        int x = 100 + (i % 5) * 350, y = 180 + (i / 5) * 175;
        int on = i == e->slot_sel;
        ui_panel(x, y, 310, 140, on ? COL_GREEN : RGB(80, 60, 140));
        char num[8];
        snprintf(num, sizeof(num), "%d", i + 1);
        draw_text_glow(x + 16, y + 14, 5, num, COL_YELLOW, RGB(200, 120, 0));
        if (e->slot_used[i]) {
            int s = ui_fit_scale(e->slot_names[i], 280, 4);
            draw_text(x + 16, y + 80, s, e->slot_names[i], COL_WHITE);
        } else {
            draw_text(x + 16, y + 84, 3, on ? "X TO MAKE A NEW LEVEL" : "EMPTY", COL_DIM);
        }
    }
    if (e->msg_frames > 0)
        draw_text_glow_centered(SCREEN_H - 170, 5, e->msg, COL_YELLOW, COL_PINK);
    draw_text_glow_centered(SCREEN_H - 90, 4, "X OPEN   SQUARE DELETE   CIRCLE BACK", RGB(220, 220, 255), RGB(90, 90, 200));
}

int editor_brush_type(const Editor *e)
{
    palette_init();
    return brushes[e->brush].type;
}

void editor_draw(const Editor *e, const Level *lv, int frame)
{
    palette_init();
    draw_set_offset(0, 0);
    if (e->screen == ED_SLOTS) {
        draw_slots(e, frame);
        return;
    }
    if (e->screen == ED_MAP) {
        draw_map(e, lv, frame);
        return;
    }
    level_draw(lv, lv->colors, e->cam_x, e->cam_y, frame);
    draw_grid(e, lv);
    draw_marked_colors(e, lv);

    if (e->has_sel) {
        int x = cell_sx(e, e->sel_x0), y = cell_sy(e, e->sel_y1);
        draw_frame(x - 2, y - 2, (e->sel_x1 - e->sel_x0 + 1) * TILE + 4, (e->sel_y1 - e->sel_y0 + 1) * TILE + 4, 4,
                   e->selecting ? COL_YELLOW : COL_CYAN);
    }
    if ((frame / 12) % 3 != 2) {
        LevelObj ghost = { (int16_t)e->cx, (int16_t)e->cy, brushes[e->brush].type, (uint8_t)e->brush_rot,
                           (uint8_t)(brushes[e->brush].type == OBJ_SIGN ? e->brush_sign : e->brush_color), 0, 0, 0, 0, { 0 } };
        if (level_find(lv, e->cx, e->cy) < 0 && ghost.type != OBJ_COLOR)
            level_draw_object(&ghost, frame);
    }
    int pulse = (frame / 6) % 2;
    draw_frame(cell_sx(e, e->cx) - 6, cell_sy(e, e->cy) - 6, TILE + 12, TILE + 12, 6, pulse ? COL_YELLOW : COL_WHITE);

    draw_bar(e, lv);
    if (e->msg_frames > 0 && e->screen == ED_EDIT)
        draw_text_glow_centered(380, 7, e->msg, COL_YELLOW, COL_PINK);
    if (e->screen == ED_MENU)
        draw_menu_panel(e, lv);
    if (e->screen == ED_NAME)
        kb_draw(&e->kb, "NAME YOUR LEVEL", COL_PINK, frame);
    if (e->screen == ED_GROUPS)
        draw_groups(e);
    if (e->screen == ED_TEMPLATES)
        draw_templates(e, lv);
    if (e->screen == ED_OBJSET)
        draw_objset(e, lv);
    if (e->music_on && e->screen == ED_EDIT && (frame / 20) % 2)
        draw_text_glow(SCREEN_W - 24 - text_width("MUSIC", 4), 96, 4, "MUSIC", COL_GREEN, RGB(30, 140, 10));
    if (e->msg_frames > 0 && e->screen == ED_MENU)
        draw_text_glow_centered(770, 6, e->msg, COL_YELLOW, COL_PINK);
}
