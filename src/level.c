// Fritz Dash - level loading: a small JSON reader and the level schema.
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "level.h"
#include "mode.h"

static const char *const OBJ_NAMES[OBJ_COUNT] = {
    [OBJ_BLOCK] = "block",
    [OBJ_SPIKE] = "spike",
    [OBJ_HALF_SPIKE] = "half_spike",
    [OBJ_PLATFORM] = "platform",
    [OBJ_PAD_YELLOW] = "pad_yellow",
    [OBJ_PAD_PINK] = "pad_pink",
    [OBJ_ORB_YELLOW] = "orb_yellow",
    [OBJ_ORB_PINK] = "orb_pink",
    [OBJ_GRAVITY_UP] = "portal_gravity_up",
    [OBJ_GRAVITY_DOWN] = "portal_gravity_down",
    [OBJ_SPEED_05] = "portal_speed_05",
    [OBJ_SPEED_1] = "portal_speed_1",
    [OBJ_SPEED_2] = "portal_speed_2",
    [OBJ_SPEED_3] = "portal_speed_3",
    [OBJ_CUBE_PORTAL] = "portal_cube",
    [OBJ_SHIP_PORTAL] = "portal_ship",
    [OBJ_END] = "end",
    [OBJ_COLOR] = "color_trigger",
    [OBJ_BALL_PORTAL] = "portal_ball",
    [OBJ_UFO_PORTAL] = "portal_ufo",
    [OBJ_WAVE_PORTAL] = "portal_wave",
    [OBJ_MINI_PORTAL] = "portal_mini",
    [OBJ_BIG_PORTAL] = "portal_big",
    [OBJ_MOVE] = "move_trigger",
    [OBJ_SAW] = "saw",
    [OBJ_SAW_BIG] = "saw_big",
    [OBJ_PULSE] = "pulse",
    [OBJ_PULSE_BARS] = "pulse_bars",
    [OBJ_DECO_GLOW] = "deco_glow",
    [OBJ_DECO_STAR] = "deco_star",
    [OBJ_DECO_ARROW] = "deco_arrow",
    [OBJ_DECO_BLOCK] = "deco_block",
    [OBJ_DECO_SPIKES] = "deco_spikes",
    [OBJ_COIN] = "coin",
    [OBJ_SIGN] = "sign",
};

const char *const SIGN_TEXTS[NUM_SIGNS] = {
    "PRESS X TO JUMP", "HOLD X TO KEEP JUMPING", "JUMP ONTO THE BLOCKS", "PADS JUMP FOR YOU",
    "PRESS X IN THE ORB", "PORTALS CHANGE THINGS", "UPSIDE DOWN!", "HOLD X TO FLY UP",
    "LET GO TO FLY DOWN", "GRAB THE SECRET COIN!", "STUCK? PAUSE AND PRACTISE", "SQUARE = CHECKPOINT",
    "TOUCHPAD = SHOW ME HOW", "WATCH OUT!", "YOU CAN DO IT!", "ALMOST THERE!",
    "TAP X TO FLAP", "X FLIPS THE BALL", "HOLD X: THE WAVE GOES UP", "MINI PORTAL: TINY YOU!",
    "GO GO GO!", "NICE!", "WELL DONE!", "FRITZ WAS HERE",
};

const LevelColors LEVEL_PALETTES[NUM_PALETTES] = {
    { RGB(10, 4, 40), RGB(62, 10, 92), RGB(20, 6, 52), RGB(0, 240, 255) },
    { RGB(2, 14, 48), RGB(10, 62, 124), RGB(4, 20, 62), RGB(140, 255, 40) },
    { RGB(40, 2, 40), RGB(112, 10, 92), RGB(42, 4, 42), RGB(255, 230, 0) },
    { RGB(2, 30, 22), RGB(10, 92, 62), RGB(4, 34, 26), RGB(255, 70, 210) },
    { RGB(40, 6, 2), RGB(122, 32, 10), RGB(42, 10, 4), RGB(0, 240, 255) },
    { RGB(0, 0, 0), RGB(30, 30, 40), RGB(10, 10, 14), RGB(255, 255, 255) },
    { RGB(4, 20, 40), RGB(20, 110, 150), RGB(6, 40, 60), RGB(255, 200, 60) },
    { RGB(30, 0, 50), RGB(90, 0, 140), RGB(20, 0, 40), RGB(80, 255, 200) },
    { RGB(50, 20, 0), RGB(200, 90, 0), RGB(60, 24, 0), RGB(255, 255, 120) },
    { RGB(0, 30, 10), RGB(30, 140, 40), RGB(4, 40, 14), RGB(255, 255, 255) },
    { RGB(40, 0, 10), RGB(180, 0, 40), RGB(50, 0, 14), RGB(255, 210, 0) },
    { RGB(8, 8, 30), RGB(60, 60, 160), RGB(14, 14, 50), RGB(255, 120, 255) },
};

const char *const PALETTE_NAMES[NUM_PALETTES] = {
    "NEON NIGHT", "OCEAN", "CANDY", "JUNGLE", "LAVA", "SHADOW",
    "SUNSET SEA", "GALAXY", "DESERT", "FOREST", "VOLCANO", "ARCADE",
};

// Names of 3.0's objects come from OBJ_NAMES, a mode's portal from its
// ModeDef and a newer object from its ObjDef.
const char *obj_name(int type)
{
    if (type <= OBJ_NONE || type >= OBJ_COUNT)
        return "";
    if (OBJ_NAMES[type])
        return OBJ_NAMES[type];
    const ObjDef *d = obj_def(type);
    if (d)
        return d->name;
    int m = run_portal_mode(type);
    return m >= 0 ? MODES[m]->portal_name : "";
}

const char *mode_name(int mode)
{
    return mode >= 0 && mode < MODE_COUNT && MODES[mode] ? MODES[mode]->name : NULL;
}

int mode_from_name(const char *s)
{
    for (int m = 0; m < MODE_COUNT; m++)
        if (MODES[m] && !strcmp(s, MODES[m]->name))
            return m;
    return -1;
}

// Triggers name a group with "g" (the one they move or change); they are not in it.
int obj_grouped(const LevelObj *o)
{
    return o->group && o->type != OBJ_MOVE && o->type != OBJ_COLOR && o->type != OBJ_END &&
           !(obj_def(o->type) && obj_def(o->type)->kind == K_PASS);
}

static int clampi(double v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : (int)v;
}

int obj_type_from_name(const char *s)
{
    for (int i = OBJ_NONE + 1; i < OBJ_COUNT; i++)
        if (OBJ_NAMES[i] && !strcmp(s, OBJ_NAMES[i]))
            return i;
    if (!strncmp(s, "portal_", 7)) {
        int m = mode_from_name(s + 7);
        return m >= MODE_ROBOT ? OBJ_PORTAL_ROBOT + m - MODE_ROBOT : OBJ_NONE;
    }
    for (int i = OBJ_NONE + 1; i < OBJ_COUNT; i++)
        if (OBJECTS[i] && !strcmp(s, OBJECTS[i]->name))
            return i;
    return OBJ_NONE;
}

// ---------------------------------------------------------------------------
// JSON reader. Reads straight from the text with no allocation. Any error sets
// `bad` and moves to the end, so every loop below stops on its own.

typedef struct {
    const char *p, *end;
    int bad;
} Json;

static void js_fail(Json *j)
{
    j->bad = 1;
    j->p = j->end;
}

static int js_peek(Json *j)
{
    while (j->p < j->end && (*j->p == ' ' || *j->p == '\n' || *j->p == '\r' || *j->p == '\t'))
        j->p++;
    return j->p < j->end ? (unsigned char)*j->p : -1;
}

static int js_eat(Json *j, int c)
{
    if (js_peek(j) != c)
        return 0;
    j->p++;
    return 1;
}

// Reads a string into out (may be NULL to skip it). Escapes other than the
// simple ones become '?'.
static void js_string(Json *j, char *out, int max)
{
    int n = 0;
    if (!js_eat(j, '"')) {
        js_fail(j);
        return;
    }
    while (j->p < j->end && *j->p != '"') {
        char c = *j->p++;
        if (c == '\\') {
            if (j->p >= j->end)
                break;
            c = *j->p++;
            if (c == 'u') {
                if (j->end - j->p < 4)
                    break;
                j->p += 4;
                c = '?';
            } else if (c == 'n' || c == 't' || c == 'r') {
                c = ' ';
            } else if (c != '"' && c != '\\' && c != '/') {
                c = '?';
            }
        }
        if (out && n < max - 1)
            out[n++] = c;
    }
    if (j->p >= j->end) {
        js_fail(j);
        return;
    }
    j->p++;
    if (out)
        out[n] = 0;
}

static double js_number(Json *j)
{
    int c = js_peek(j), neg = 0, digits = 0;
    double v = 0, scale = 1;
    if (c == '-') {
        neg = 1;
        j->p++;
    }
    while (j->p < j->end && *j->p >= '0' && *j->p <= '9') {
        v = v * 10 + (*j->p++ - '0');
        digits++;
    }
    if (j->p < j->end && *j->p == '.') {
        j->p++;
        while (j->p < j->end && *j->p >= '0' && *j->p <= '9') {
            scale /= 10;
            v += (*j->p++ - '0') * scale;
            digits++;
        }
    }
    if (j->p < j->end && (*j->p == 'e' || *j->p == 'E')) {
        j->p++;
        if (j->p < j->end && (*j->p == '+' || *j->p == '-'))
            j->p++;
        while (j->p < j->end && *j->p >= '0' && *j->p <= '9')
            j->p++;
    }
    if (!digits)
        js_fail(j);
    return neg ? -v : v;
}

// Steps through an object: returns 1 with the next key read and the reader
// on its value, or 0 at the closing brace. Start with *first = 1.
static int js_members(Json *j, int *first, char *key, int max)
{
    if (*first) {
        *first = 0;
        if (!js_eat(j, '{')) {
            js_fail(j);
            return 0;
        }
        if (js_eat(j, '}'))
            return 0;
    } else {
        if (js_eat(j, '}'))
            return 0;
        if (!js_eat(j, ',')) {
            js_fail(j);
            return 0;
        }
    }
    js_string(j, key, max);
    if (!js_eat(j, ':'))
        js_fail(j);
    return !j->bad;
}

// Same for arrays: returns 1 with the reader on the next element.
static int js_items(Json *j, int *first)
{
    if (*first) {
        *first = 0;
        if (!js_eat(j, '[')) {
            js_fail(j);
            return 0;
        }
        return !js_eat(j, ']');
    }
    if (js_eat(j, ']'))
        return 0;
    if (!js_eat(j, ','))
        js_fail(j);
    return !j->bad;
}

static void js_skip(Json *j, int depth)
{
    int c = js_peek(j), first = 1;
    if (depth > 32) {
        js_fail(j);
        return;
    }
    if (c == '"') {
        js_string(j, NULL, 0);
    } else if (c == '{') {
        while (js_members(j, &first, NULL, 0))
            js_skip(j, depth + 1);
    } else if (c == '[') {
        while (js_items(j, &first))
            js_skip(j, depth + 1);
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        js_number(j);
    } else if (c >= 'a' && c <= 'z') {
        while (j->p < j->end && *j->p >= 'a' && *j->p <= 'z')
            j->p++;
    } else {
        js_fail(j);
    }
}

// ---------------------------------------------------------------------------
// Level schema

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// "#rrggbb" -> colour; anything else leaves *c unchanged.
static void read_color(Json *j, color_t *c)
{
    char s[16];
    js_string(j, s, sizeof(s));
    if (j->bad || s[0] != '#' || strlen(s) != 7)
        return;
    color_t v = 0;
    for (int i = 1; i < 7; i++) {
        int d = hex_digit(s[i]);
        if (d < 0)
            return;
        v = v * 16 + (color_t)d;
    }
    *c = v;
}

static void read_colors(Json *j, Level *lv)
{
    int first = 1;
    char key[16];
    while (js_members(j, &first, key, sizeof(key))) {
        if (!strcmp(key, "top")) read_color(j, &lv->colors.top);
        else if (!strcmp(key, "bottom")) read_color(j, &lv->colors.bottom);
        else if (!strcmp(key, "ground")) read_color(j, &lv->colors.ground);
        else if (!strcmp(key, "line")) read_color(j, &lv->colors.line);
        else js_skip(j, 1);
    }
}

// "m": a list of mode names; unknown ones are left out.
static void read_list(Json *j, uint8_t *list)
{
    int first = 1, n = 0;
    char s[24];
    while (js_items(j, &first)) {
        js_string(j, s, sizeof(s));
        int m = mode_from_name(s);
        if (!j->bad && m >= 0 && n < MAX_LIST)
            list[n++] = (uint8_t)(m + 1);
    }
}

static void read_object(Json *j, Level *lv)
{
    int first = 1, has_x = 0;
    char key[8], t[32] = "";
    double x = 0, y = 0, r = 0, c = 0, g = 0, dx = 0, dy = 0, f = 0;
    uint8_t list[MAX_LIST] = { 0 };
    while (js_members(j, &first, key, sizeof(key))) {
        if (!strcmp(key, "t")) js_string(j, t, sizeof(t));
        else if (!strcmp(key, "x")) { x = js_number(j); has_x = 1; }
        else if (!strcmp(key, "y")) y = js_number(j);
        else if (!strcmp(key, "r")) r = js_number(j);
        else if (!strcmp(key, "c")) c = js_number(j);
        else if (!strcmp(key, "g")) g = js_number(j);
        else if (!strcmp(key, "dx")) dx = js_number(j);
        else if (!strcmp(key, "dy")) dy = js_number(j);
        else if (!strcmp(key, "f")) f = js_number(j);
        else if (!strcmp(key, "m") && js_peek(j) == '[') read_list(j, list);
        else js_skip(j, 2);
    }
    if (j->bad)
        return;
    int type = obj_type_from_name(t);
    if (!type || !has_x || x < 0 || x > LEVEL_MAX_X || y < 0 || y > 39 || lv->nobj >= MAX_LEVEL_OBJS) {
        lv->skipped++;
        return;
    }
    LevelObj *o = &lv->objs[lv->nobj++];
    o->x = (int16_t)x;
    o->y = (int16_t)y;
    o->type = (uint8_t)type;
    o->rot = (uint8_t)((int)r & 3);
    o->param = (uint8_t)clampi(c, 0, 255);
    o->group = (uint8_t)(g >= 1 && g <= MAX_GROUPS ? (int)g : 0);
    o->dx = (int8_t)clampi(dx, -60, 60);
    o->dy = (int8_t)clampi(dy, -40, 40);
    o->time = (uint16_t)clampi(f, 0, 600);
    memcpy(o->list, list, sizeof(o->list));
}

// "eggs": [ { "n": 1, "name": "..", "hint": ".." }, ... ]
static void read_eggs(Json *j, Level *lv)
{
    int first = 1;
    while (js_items(j, &first)) {
        LevelEgg e = { 0, "", "" };
        int f = 1;
        char key[8];
        while (js_members(j, &f, key, sizeof(key))) {
            if (!strcmp(key, "n")) e.n = clampi(js_number(j), 0, 255);
            else if (!strcmp(key, "name")) js_string(j, e.name, sizeof(e.name));
            else if (!strcmp(key, "hint")) js_string(j, e.hint, sizeof(e.hint));
            else js_skip(j, 2);
        }
        if (!j->bad && e.n >= 1 && e.n <= MAX_EGGS && !level_egg(lv, e.n) && lv->neggs < MAX_EGGS)
            lv->eggs[lv->neggs++] = e;
    }
}

const LevelEgg *level_egg(const Level *lv, int n)
{
    for (int i = 0; i < lv->neggs; i++)
        if (lv->eggs[i].n == n)
            return &lv->eggs[i];
    return NULL;
}

static int obj_before(const LevelObj *a, const LevelObj *b)
{
    return a->x != b->x ? a->x < b->x : a->y < b->y;
}

static void sort_objects(Level *lv)
{
    // Shell sort: in place, no allocation, fast enough for a few thousand.
    for (int gap = lv->nobj / 2; gap > 0; gap /= 2)
        for (int i = gap; i < lv->nobj; i++) {
            LevelObj v = lv->objs[i];
            int k = i;
            while (k >= gap && obj_before(&v, &lv->objs[k - gap])) {
                lv->objs[k] = lv->objs[k - gap];
                k -= gap;
            }
            lv->objs[k] = v;
        }
}

int level_parse(Level *lv, const char *text, int len)
{
    Json j = { text, text + len, 0 };
    memset(lv, 0, offsetof(Level, objs));
    strcpy(lv->name, "UNNAMED");
    lv->speed = 1;
    lv->mode = MODE_CUBE;
    lv->colors = LEVEL_PALETTES[0];

    int first = 1;
    char key[24], s[32];
    while (js_members(&j, &first, key, sizeof(key))) {
        if (!strcmp(key, "format")) {
            js_string(&j, s, sizeof(s));
            if (!j.bad && strcmp(s, "fritzdash-level"))
                return LEVEL_BAD_FORMAT;
        } else if (!strcmp(key, "name")) {
            js_string(&j, lv->name, sizeof(lv->name));
        } else if (!strcmp(key, "author")) {
            js_string(&j, lv->author, sizeof(lv->author));
        } else if (!strcmp(key, "speed")) {
            int v = (int)js_number(&j);
            lv->speed = v < 0 ? 0 : v > 3 ? 3 : v;
        } else if (!strcmp(key, "stars")) {
            int v = (int)js_number(&j);
            lv->stars = v < 0 ? 0 : v > 10 ? 10 : v;
        } else if (!strcmp(key, "song")) {
            if (js_peek(&j) == '"') {
                js_string(&j, s, sizeof(s));
                int n = 0;
                for (const char *c = s + 5; !strncmp(s, "user:", 5) && *c >= '0' && *c <= '9' && n < 100; c++)
                    n = n * 10 + (*c - '0');
                lv->user_song = n >= 1 && n <= 10 ? n : 0;
            } else {
                int v = (int)js_number(&j);
                lv->song = v < 0 ? 0 : v > 255 ? 255 : v;
            }
        } else if (!strcmp(key, "verified")) {
            lv->verified = (uint32_t)js_number(&j);
        } else if (!strcmp(key, "rated")) {
            lv->rated = (uint32_t)js_number(&j);
        } else if (!strcmp(key, "rating")) {
            int v = (int)js_number(&j);
            lv->rating = v < 0 ? 0 : v > 15 ? 15 : v;
        } else if (!strcmp(key, "mode")) {
            js_string(&j, s, sizeof(s));
            int m = mode_from_name(s);
            lv->mode = m < 0 ? MODE_CUBE : m;
        } else if (!strcmp(key, "colors")) {
            read_colors(&j, lv);
        } else if (!strcmp(key, "eggs") && js_peek(&j) == '[') {
            read_eggs(&j, lv);
        } else if (!strcmp(key, "objects")) {
            int f = 1;
            while (js_items(&j, &f))
                read_object(&j, lv);
        } else {
            js_skip(&j, 1);
        }
    }
    if (j.bad || js_peek(&j) != -1)
        return LEVEL_BAD_JSON;

    sort_objects(lv);
    level_update_end(lv);
    return LEVEL_OK;
}

void level_update_end(Level *lv)
{
    lv->nmoving = 0;
    lv->ncoins = 0;
    lv->timed = 0;
    for (int i = 0; i < lv->nobj; i++) {
        LevelObj *o = &lv->objs[i];
        const ObjDef *d = obj_def(o->type);
        if (d && (d->flags & OF_TIMED))
            lv->timed = 1;
        if (obj_grouped(o) && lv->nmoving < MAX_MOVING)
            lv->moving[lv->nmoving++] = (int16_t)i;
        if (o->type == OBJ_COIN) {
            o->param = (uint8_t)(lv->ncoins < MAX_COINS ? lv->ncoins : 255);
            lv->ncoins += lv->ncoins < MAX_COINS;
        }
    }
    lv->end_x = 0;
    for (int i = 0; i < lv->nobj; i++)
        if (lv->objs[i].type == OBJ_END) {
            lv->end_x = lv->objs[i].x * TILE;
            return;
        }
    lv->end_x = ((lv->nobj ? lv->objs[lv->nobj - 1].x : 20) + 10) * TILE;
}

uint32_t level_hash(const Level *lv)
{
    uint32_t h = 2166136261u;
    int played = 0;
    for (int i = 0; i < lv->nobj; i++)
        played += !obj_def(lv->objs[i].type) || !(obj_def(lv->objs[i].type)->flags & OF_COSMETIC);
    int head[4] = { lv->speed, lv->mode, lv->end_x, played };
    for (int i = 0; i < 4; i++)
        h = (h ^ (uint32_t)head[i]) * 16777619u;
    for (int i = 0; i < lv->nobj; i++) {
        const LevelObj *o = &lv->objs[i];
        if (obj_def(o->type) && (obj_def(o->type)->flags & OF_COSMETIC))
            continue;
        uint32_t v[5] = { (uint32_t)o->x << 16 | (uint16_t)o->y, (uint32_t)o->type << 8 | o->rot,
                          (uint32_t)o->param << 8 | o->group, (uint32_t)(uint8_t)o->dx << 8 | (uint8_t)o->dy, o->time };
        for (int k = 0; k < 5; k++)
            h = (h ^ v[k]) * 16777619u;
        if (o->list[0])
            h = (h ^ ((uint32_t)o->list[0] | (uint32_t)o->list[1] << 8 | (uint32_t)o->list[2] << 16 |
                      (uint32_t)o->list[3] << 24)) * 16777619u;
    }
    return h;
}

int level_verified(const Level *lv)
{
    return lv->verified && lv->verified == level_hash(lv);
}

int level_rating(const Level *lv)
{
    return lv->rated && lv->rated == level_hash(lv) ? lv->rating : -1;
}

int level_first_at(const Level *lv, int x)
{
    int lo = 0, hi = lv->nobj;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (lv->objs[mid].x < x)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

int level_find(const Level *lv, int x, int y)
{
    for (int i = level_first_at(lv, x); i < lv->nobj && lv->objs[i].x == x; i++)
        if (lv->objs[i].y == y)
            return i;
    return -1;
}

int level_put(Level *lv, LevelObj o)
{
    int i = level_find(lv, o.x, o.y);
    if (i >= 0) {
        lv->objs[i] = o;
        return 0;
    }
    if (lv->nobj >= MAX_LEVEL_OBJS)
        return -1;
    i = level_first_at(lv, o.x);
    while (i < lv->nobj && lv->objs[i].x == o.x && lv->objs[i].y < o.y)
        i++;
    memmove(&lv->objs[i + 1], &lv->objs[i], sizeof(LevelObj) * (size_t)(lv->nobj - i));
    lv->objs[i] = o;
    lv->nobj++;
    return 0;
}

void level_remove(Level *lv, int index)
{
    memmove(&lv->objs[index], &lv->objs[index + 1], sizeof(LevelObj) * (size_t)(lv->nobj - index - 1));
    lv->nobj--;
}

static int put_string(char *out, int max, const char *s)
{
    int n = 0;
    out[n++] = '"';
    for (; *s && n < max - 3; s++) {
        if (*s == '"' || *s == '\\')
            out[n++] = '\\';
        out[n++] = (*s >= 32 && *s < 127) ? *s : '?';
    }
    out[n++] = '"';
    out[n] = 0;
    return n;
}

int level_write(const Level *lv, char *buf, int max)
{
    char name[80], author[64];
    const LevelColors *c = &lv->colors;
    put_string(name, sizeof(name), lv->name);
    put_string(author, sizeof(author), lv->author);
    char song[16];
    if (lv->user_song)
        snprintf(song, sizeof(song), "\"user:%d\"", lv->user_song);
    else
        snprintf(song, sizeof(song), "%d", lv->song);
    int n = snprintf(buf, (size_t)max,
                     "{\"format\":\"fritzdash-level\",\"version\":1,\"name\":%s,\"author\":%s,"
                     "\"speed\":%d,\"mode\":\"%s\",\"stars\":%d,\"song\":%s,\n"
                     "\"colors\":{\"top\":\"#%06x\",\"bottom\":\"#%06x\",\"ground\":\"#%06x\",\"line\":\"#%06x\"},\n",
                     name, author, lv->speed, mode_name(lv->mode) ? mode_name(lv->mode) : "cube", lv->stars, song,
                     (unsigned)c->top, (unsigned)c->bottom, (unsigned)c->ground, (unsigned)c->line);
    if (n < max && lv->verified)
        n += snprintf(buf + n, (size_t)(max - n), "\"verified\":%u,\n", (unsigned)lv->verified);
    if (n < max && lv->rated)
        n += snprintf(buf + n, (size_t)(max - n), "\"rating\":%d,\"rated\":%u,\n", lv->rating, (unsigned)lv->rated);
    for (int i = 0; i < lv->neggs && n < max; i++) {
        char nm[EGG_NAME * 2 + 4], hint[EGG_HINT * 2 + 4];
        put_string(nm, sizeof(nm), lv->eggs[i].name);
        put_string(hint, sizeof(hint), lv->eggs[i].hint);
        n += snprintf(buf + n, (size_t)(max - n), "%s{\"n\":%d,\"name\":%s,\"hint\":%s}%s", i ? "," : "\"eggs\":[",
                      lv->eggs[i].n, nm, hint, i == lv->neggs - 1 ? "],\n" : "");
    }
    if (n < max)
        n += snprintf(buf + n, (size_t)(max - n), "\"objects\":[");
    for (int i = 0; i < lv->nobj && n < max; i++) {
        const LevelObj *o = &lv->objs[i];
        n += snprintf(buf + n, (size_t)(max - n), "%s\n{\"t\":\"%s\",\"x\":%d,\"y\":%d", i ? "," : "",
                      obj_name(o->type), o->x, o->y);
        if (n < max && o->rot)
            n += snprintf(buf + n, (size_t)(max - n), ",\"r\":%d", o->rot);
        if (n < max && o->param && o->type != OBJ_COIN)
            n += snprintf(buf + n, (size_t)(max - n), ",\"c\":%d", o->param);
        if (n < max && o->group)
            n += snprintf(buf + n, (size_t)(max - n), ",\"g\":%d", o->group);
        if (n < max && (o->dx || o->dy || o->time))
            n += snprintf(buf + n, (size_t)(max - n), ",\"dx\":%d,\"dy\":%d,\"f\":%d", o->dx, o->dy, o->time);
        for (int k = 0; k < MAX_LIST && o->list[k] && n < max; k++)
            n += snprintf(buf + n, (size_t)(max - n), "%s\"%s\"", k ? "," : ",\"m\":[",
                          mode_name(o->list[k] - 1) ? mode_name(o->list[k] - 1) : "cube");
        if (n < max && o->list[0])
            n += snprintf(buf + n, (size_t)(max - n), "]");
        if (n < max)
            n += snprintf(buf + n, (size_t)(max - n), "}");
    }
    if (n < max)
        n += snprintf(buf + n, (size_t)(max - n), "\n]}\n");
    return n < max ? n : -1;
}
