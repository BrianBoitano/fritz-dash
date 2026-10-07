// Fritz Dash - level physics: movement (each mode's own step is in its
// mode_<name>.c), collisions, pads, orbs, portals, triggers, death,
// completion and practice checkpoints.
#include <string.h>

#include "eggs.h"
#include "input.h"
#include "mode.h"
#include "synth.h"
#include "transform.h"

// Distances are world pixels (TILE = 80 per grid cell), times are 60 fps
// frames. The proportions follow Geometry Dash: a cube jump is about 2.1
// cells high and 23 frames long, and the four speeds are its 0.5x..3x.
#define PAD_YELLOW_V 40.0f
#define PAD_PINK_V   24.0f
#define ORB_YELLOW_V 30.0f
#define ORB_PINK_V   21.0f
#define LAND_TOL     12.0f      // catching a block's top corner still lands
#define INNER        14.0f      // half size of the box that dies on walls
#define HAZARD_INSET 6.0f
#define WORLD_TOP    (40.0f * TILE)
#define ROT_STEP     0.1348f    // half a turn per cube jump
#define CP_AUTO      120        // frames between automatic checkpoints
#define CP_RACE      20         // and in a race
#define CP_DOOMED    20         // dying this soon after a respawn drops that checkpoint

static const float SPEEDS[4] = { 11.16f, 13.85f, 17.21f, 20.80f };

float run_speed_px(int speed)
{
    return SPEEDS[speed & 3];
}

float run_half_height(int mode)
{
    return mode_def(mode)->half_h;
}

float run_half_h(const Body *b)
{
    return run_half_height(b->mode) * (b->mini ? 0.5f : 1.0f);
}

float run_half_w(const Body *b)
{
    return mode_def(b->mode)->half_w * (b->mini ? 0.5f : 1.0f);
}

// Modes with a corridor fly in it, and its edges never kill.
static int bounded(int mode)
{
    return mode_def(mode)->corridor > 0;
}

// On a floor or ceiling counts as a safe spot for this mode (cube, ball).
static int needs_ground(int mode)
{
    const ModeDef *d = mode_def(mode);
    return !d->corridor || (d->flags & MF_GROUNDED);
}

// Collision box of each object type, in cell units before rotation. A saw
// is a circle: x0 is its radius, about the cell centre.
typedef struct {
    uint8_t kind;
    float x0, y0, x1, y1;
} Shape;

// 3.2 objects get theirs from their ObjDef (shapes_init) and mode portals
// the portal shape.
static Shape SHAPES[OBJ_COUNT] = {
    [OBJ_BLOCK] = { K_SOLID, 0, 0, 1, 1 },
    [OBJ_SPIKE] = { K_HAZARD, 0.38f, 0, 0.62f, 0.5f },
    [OBJ_HALF_SPIKE] = { K_HAZARD, 0.38f, 0, 0.62f, 0.25f },
    [OBJ_PLATFORM] = { K_SOLID, 0, 0.5f, 1, 1 },
    [OBJ_PAD_YELLOW] = { K_TRIGGER, 0.05f, 0, 0.95f, 0.25f },
    [OBJ_PAD_PINK] = { K_TRIGGER, 0.05f, 0, 0.95f, 0.25f },
    [OBJ_ORB_YELLOW] = { K_TRIGGER, -0.15f, -0.15f, 1.15f, 1.15f },
    [OBJ_ORB_PINK] = { K_TRIGGER, -0.15f, -0.15f, 1.15f, 1.15f },
    [OBJ_GRAVITY_UP] = { K_PORTAL, 0.1f, -1, 0.9f, 2 },
    [OBJ_GRAVITY_DOWN] = { K_PORTAL, 0.1f, -1, 0.9f, 2 },
    [OBJ_SPEED_05] = { K_PORTAL, 0.1f, -1, 0.9f, 2 },
    [OBJ_SPEED_1] = { K_PORTAL, 0.1f, -1, 0.9f, 2 },
    [OBJ_SPEED_2] = { K_PORTAL, 0.1f, -1, 0.9f, 2 },
    [OBJ_SPEED_3] = { K_PORTAL, 0.1f, -1, 0.9f, 2 },
    [OBJ_CUBE_PORTAL] = { K_PORTAL, 0.1f, -1, 0.9f, 2 },
    [OBJ_SHIP_PORTAL] = { K_PORTAL, 0.1f, -1, 0.9f, 2 },
    [OBJ_BALL_PORTAL] = { K_PORTAL, 0.1f, -1, 0.9f, 2 },
    [OBJ_UFO_PORTAL] = { K_PORTAL, 0.1f, -1, 0.9f, 2 },
    [OBJ_WAVE_PORTAL] = { K_PORTAL, 0.1f, -1, 0.9f, 2 },
    [OBJ_MINI_PORTAL] = { K_PORTAL, 0.15f, -1, 0.85f, 2 },
    [OBJ_BIG_PORTAL] = { K_PORTAL, 0.15f, -1, 0.85f, 2 },
    [OBJ_COIN] = { K_TRIGGER, -0.1f, -0.1f, 1.1f, 1.1f },
    [OBJ_SAW] = { K_SAW, 0.36f, 0, 0, 0 },
    [OBJ_SAW_BIG] = { K_SAW, 1.1f, 0, 0, 0 },
};

typedef struct {
    float x0, y0, x1, y1;
} Box;

static void shapes_init(void)
{
    static int done;
    if (done)
        return;
    done = 1;
    for (int t = OBJ_PORTAL_ROBOT; t <= OBJ_PORTAL_SHAPESHIFTER; t++)
        SHAPES[t] = SHAPES[OBJ_CUBE_PORTAL];
    for (int t = 0; t < OBJ_COUNT; t++) {
        const ObjDef *d = obj_def(t);
        if (d) {
            Shape s = { (uint8_t)d->kind, d->x0, d->y0, d->x1, d->y1 };
            SHAPES[t] = s;
        }
    }
}

int obj_kind(int type)
{
    shapes_init();
    return type > OBJ_NONE && type < OBJ_COUNT ? SHAPES[type].kind : K_NONE;
}

void cell_rotate(int r, float x, float y, float *ox, float *oy)
{
    switch (r & 3) {
    case 0: *ox = x; *oy = y; break;
    case 1: *ox = y; *oy = 1 - x; break;
    case 2: *ox = 1 - x; *oy = 1 - y; break;
    default: *ox = 1 - y; *oy = x; break;
    }
}

static Box obj_box(const LevelObj *o)
{
    const Shape *s = &SHAPES[o->type];
    if (s->kind == K_SAW) {
        float cx = ((float)o->x + 0.5f) * TILE, cy = ((float)o->y + 0.5f) * TILE, rr = s->x0 * TILE;
        Box b = { cx - rr, cy - rr, cx + rr, cy + rr };
        return b;
    }
    float ax, ay, bx, by;
    int r = s->kind == K_PORTAL ? 0 : o->rot;
    cell_rotate(r, s->x0, s->y0, &ax, &ay);
    cell_rotate(r, s->x1, s->y1, &bx, &by);
    Box b;
    b.x0 = (o->x + (ax < bx ? ax : bx)) * TILE;
    b.x1 = (o->x + (ax < bx ? bx : ax)) * TILE;
    b.y0 = (o->y + (ay < by ? ay : by)) * TILE;
    b.y1 = (o->y + (ay < by ? by : ay)) * TILE;
    return b;
}

static int overlap(const Box *a, const Box *b)
{
    return a->x0 < b->x1 && a->x1 > b->x0 && a->y0 < b->y1 && a->y1 > b->y0;
}

// Does box a touch the circle that fills box s?
static int touch_circle(const Box *a, const Box *s)
{
    float cx = (s->x0 + s->x1) / 2, cy = (s->y0 + s->y1) / 2, rr = (s->x1 - s->x0) / 2;
    float nx = cx < a->x0 ? a->x0 : cx > a->x1 ? a->x1 : cx;
    float ny = cy < a->y0 ? a->y0 : cy > a->y1 ? a->y1 : cy;
    return (nx - cx) * (nx - cx) + (ny - cy) * (ny - cy) < rr * rr;
}

void run_group_offset(const Body *b, int g, float *ox, float *oy)
{
    *ox = *oy = 0;
    if (g < 1 || g > MAX_GROUPS)
        return;
    const GroupMove *m = &b->grp[g - 1];
    if (m->t >= m->n) {
        *ox = m->x1;
        *oy = m->y1;
        return;
    }
    float s = (float)m->t / (float)m->n;
    s = s * s * (3 - 2 * s);
    *ox = m->x0 + (m->x1 - m->x0) * s;
    *oy = m->y0 + (m->y1 - m->y0) * s;
}

float run_beat(const Run *r)
{
    int bpm = r->bpm > 0 ? r->bpm : 120;
    float ph = (float)((r->song_t * bpm) % 3600) / 3600.0f;
    return (1 - ph) * (1 - ph);
}

int run_beat_at(const Run *r, int t)
{
    return t * (r->lv_bpm > 0 ? r->lv_bpm : 120) / 3600;
}

float run_beat_frac(const Run *r, int t)
{
    return (float)((t * (r->lv_bpm > 0 ? r->lv_bpm : 120)) % 3600) / 3600.0f;
}

uint64_t run_key(const Run *r)
{
    const Body *b = &r->b;
    uint64_t h = 1469598103934665603ull;
    int v[14] = { (int)(b->x * 4), (int)(b->y * 4), (int)(b->vy * 8), b->mode, b->grav, b->speed, b->on_ground,
                  r->buffered, r->prev_input & IN_CROSS, r->held_lock, r->ntouch, (int)b->lo, b->mini, b->coins };
    for (int i = 0; i < 14; i++)
        h = (h ^ (uint64_t)(uint32_t)v[i]) * 1099511628211ull;
    for (int i = 0; i < r->ntouch; i++)
        h = (h ^ (uint64_t)(r->touch[i] * 2 + r->touch_used[i])) * 1099511628211ull;
    for (int g = 1; g <= MAX_GROUPS; g++) {
        float ox, oy;
        run_group_offset(b, g, &ox, &oy);
        h = (h ^ (uint64_t)(uint32_t)((int)(ox * 4) * 65599 + (int)(oy * 4))) * 1099511628211ull;
    }
    // 3.2 state, only when there is some, so older levels search exactly as before.
    for (int i = 0; i < 4; i++)
        if (b->ms[i])
            h = (h ^ (uint64_t)(uint32_t)(b->ms[i] * 16 + i)) * 1099511628211ull;
    for (int i = 0; i < 2; i++)
        if (b->mf[i] != 0)
            h = (h ^ (uint64_t)(uint32_t)((int)(b->mf[i] * 4) * 4 + i)) * 1099511628211ull;
    for (int g = 0; g < MAX_GROUPS; g++)
        if (b->mb_to[g])
            h = (h ^ (uint64_t)(uint32_t)(b->mb_to[g] << 24 | b->mb_every[g] << 16 | (uint16_t)b->mb_beat[g])) *
                1099511628211ull;
    for (int i = 0; i < 6; i++)
        if (b->pb_broke[i])
            h = (h ^ (uint64_t)(uint32_t)(0x5000000 + b->pb_broke[i] * 8 + i)) * 1099511628211ull;
    if (b->pb_groups)
        h = (h ^ (uint64_t)(0x7000000u | b->pb_groups)) * 1099511628211ull;
    if (r->lv->timed)
        h = (h ^ (uint64_t)(uint32_t)b->t) * 1099511628211ull;
    return h | 1;
}

static uint32_t rnd_u(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static float rnd_f(uint32_t *state, int lo, int hi)
{
    return (float)(lo + (int)(rnd_u(state) % (uint32_t)(hi - lo + 1)));
}

enum { FX_BURST, FX_RING, FX_SHATTER, FX_FIREWORKS, FX_COINS, FX_CONFETTI, FX_SUPERNOVA, FX_LAVA };

void fx_burst(Particle *parts, const Look *look, float x, float y, uint32_t *rng)
{
    static const color_t RAINBOW[6] = { RGB(255, 60, 60), RGB(255, 160, 0), RGB(255, 240, 0),
                                        RGB(60, 255, 90), RGB(40, 160, 255), RGB(200, 80, 255) };
    if (rw_burst(parts, look, x, y, rng))
        return;
    color_t c1 = LOOK_COLORS[look->v[LOOK_COL1]], c2 = LOOK_COLORS[look->v[LOOK_COL2]];
    color_t cols[4] = { c1, c2, RGB(255, 255, 255), RGB(255, 235, 40) };
    int fx = look->v[LOOK_DEATH];
    for (int i = 0; i < MAX_PARTS; i++) {
        Particle *p = &parts[i];
        p->x = x;
        p->y = y;
        p->color = cols[i % 4];
        if (fx == FX_RING) {
            float a = (float)i * (6.2831853f / MAX_PARTS);
            p->vx = fcos(a) * 13.0f;
            p->vy = fsin(a) * 13.0f + 4.0f;
            p->max_life = p->life = 45;
            p->size = 14;
        } else if (fx == FX_SHATTER) {
            if (i >= 14) {
                p->life = 0;
                continue;
            }
            p->vx = rnd_f(rng, -70, 70) / 10.0f;
            p->vy = rnd_f(rng, 20, 140) / 10.0f;
            p->max_life = p->life = (int)rnd_f(rng, 50, 75);
            p->size = (int)rnd_f(rng, 24, 36);
        } else if (fx == FX_CONFETTI) {
            p->vx = rnd_f(rng, -120, 120) / 10.0f;
            p->vy = rnd_f(rng, 60, 220) / 10.0f;
            p->max_life = p->life = (int)rnd_f(rng, 50, 80);
            p->size = (int)rnd_f(rng, 8, 14);
            p->color = RAINBOW[(i * 7) % 6];
        } else if (fx == FX_COINS) {
            p->vx = rnd_f(rng, -90, 90) / 10.0f;
            p->vy = rnd_f(rng, 120, 260) / 10.0f;
            p->max_life = p->life = (int)rnd_f(rng, 45, 75);
            p->size = (int)rnd_f(rng, 14, 22);
            p->color = i % 3 ? RGB(255, 205, 30) : RGB(255, 245, 170);
        } else if (fx == FX_SUPERNOVA) {
            // A fast white ring around a slower one in his colours.
            int outer = i < MAX_PARTS / 2;
            float a = ((float)i + (outer ? 0.0f : 0.5f)) * (6.2831853f * 2 / MAX_PARTS), v = outer ? 20.0f : 10.0f;
            p->vx = fcos(a) * v;
            p->vy = fsin(a) * v + 5.0f;
            p->max_life = p->life = outer ? 42 : 58;
            p->size = outer ? 16 : 24;
            p->color = outer ? (i % 2 ? RGB(255, 255, 255) : RGB(255, 240, 140)) : cols[i % 2];
        } else if (fx == FX_LAVA) {
            static const color_t LAVA[4] = { RGB(255, 240, 120), RGB(255, 170, 30), RGB(255, 90, 20), RGB(200, 30, 20) };
            p->vx = rnd_f(rng, -60, 60) / 10.0f;
            p->vy = rnd_f(rng, 140, 300) / 10.0f;
            p->max_life = p->life = (int)rnd_f(rng, 45, 80);
            p->size = (int)rnd_f(rng, 10, 24);
            p->color = LAVA[i % 4];
        } else if (fx == FX_FIREWORKS) {
            p->vx = rnd_f(rng, -110, 110) / 10.0f;
            p->vy = rnd_f(rng, 80, 280) / 10.0f;
            p->max_life = p->life = (int)rnd_f(rng, 35, 70);
            p->size = (int)rnd_f(rng, 6, 14);
            p->color = RAINBOW[i % 6];
        } else {
            p->vx = rnd_f(rng, -160, 160) / 10.0f;
            p->vy = rnd_f(rng, -40, 220) / 10.0f;
            p->max_life = p->life = (int)rnd_f(rng, 30, 60);
            p->size = (int)rnd_f(rng, 10, 26);
        }
    }
}

// A few gold sparks from particles that are not in use.
void fx_sparkle(Particle *parts, float x, float y, uint32_t *rng)
{
    for (int i = 0, n = 0; i < MAX_PARTS && n < 12; i++) {
        Particle *p = &parts[i];
        if (p->life > 0)
            continue;
        p->x = x;
        p->y = y;
        p->vx = rnd_f(rng, -90, 90) / 10.0f;
        p->vy = rnd_f(rng, 20, 160) / 10.0f;
        p->max_life = p->life = (int)rnd_f(rng, 20, 40);
        p->size = (int)rnd_f(rng, 8, 16);
        p->color = n % 3 ? RGB(255, 210, 40) : RGB(255, 255, 255);
        n++;
    }
}

void fx_update(Particle *parts)
{
    for (int i = 0; i < MAX_PARTS; i++) {
        Particle *p = &parts[i];
        if (p->life <= 0)
            continue;
        p->life--;
        p->x += p->vx;
        p->y += p->vy;
        p->vy -= 0.8f;
    }
}

static void start_body(Body *b, const Level *lv)
{
    const ModeDef *d = mode_def(lv->mode);
    memset(b, 0, sizeof(*b));
    b->mode = lv->mode;
    b->x = PLAYER_S / 2.0f;
    b->y = run_half_height(b->mode);
    b->grav = 1;
    b->speed = lv->speed;
    b->on_ground = 1;
    b->lo = 0;
    b->hi = (float)(d->corridor ? d->corridor : 10) * TILE;
    b->col_from = b->col_to = lv->colors;
    b->col_t = 256;
    b->morph_from = b->mode;
    b->morph_t = MORPH_FRAMES;
    if (d->enter)
        d->enter(b);
}

LevelColors run_colors(const Run *r)
{
    const Body *b = &r->b;
    LevelColors c;
    c.top = col_lerp(b->col_from.top, b->col_to.top, b->col_t);
    c.bottom = col_lerp(b->col_from.bottom, b->col_to.bottom, b->col_t);
    c.ground = col_lerp(b->col_from.ground, b->col_to.ground, b->col_t);
    c.line = col_lerp(b->col_from.line, b->col_to.line, b->col_t);
    return c;
}

// Colour triggers act when the player's centre passes their cell.
static void color_triggers(Run *r, float x0, float x1)
{
    const Level *lv = r->lv;
    for (int i = level_first_at(lv, (int)(x0 / TILE)); i < lv->nobj && lv->objs[i].x * TILE <= x1; i++) {
        const LevelObj *o = &lv->objs[i];
        float tx = ((float)o->x + 0.5f) * TILE;
        if (o->type != OBJ_COLOR || tx <= x0 || tx > x1)
            continue;
        r->b.col_from = run_colors(r);
        r->b.col_to = LEVEL_PALETTES[o->param % NUM_PALETTES];
        r->b.col_t = 0;
    }
}

// Move triggers start their group's move when the player's centre passes.
static void start_move(Body *b, const LevelObj *o)
{
    if (o->group < 1 || o->group > MAX_GROUPS)
        return;
    GroupMove *m = &b->grp[o->group - 1];
    float ox, oy;
    run_group_offset(b, o->group, &ox, &oy);
    m->x0 = ox;
    m->y0 = oy;
    m->x1 = ox + (float)o->dx * TILE;
    m->y1 = oy + (float)o->dy * TILE;
    m->t = 0;
    m->n = (int16_t)(o->time ? o->time : 1);
}

static void move_triggers(Run *r, float x0, float x1)
{
    const Level *lv = r->lv;
    for (int i = level_first_at(lv, (int)(x0 / TILE)); i < lv->nobj && lv->objs[i].x * TILE <= x1; i++) {
        const LevelObj *o = &lv->objs[i];
        float tx = ((float)o->x + 0.5f) * TILE;
        if (o->type == OBJ_MOVE && tx > x0 && tx <= x1)
            start_move(&r->b, o);
    }
}

static void reset_motion(Run *r)
{
    r->state = RUN_PLAY;
    r->state_frames = 0;
    r->since_respawn = 0;
    r->cp_timer = 0;
    r->ntouch = 0;
    r->buffered = 0;
    r->held_lock = 1;    // a held X must be pressed again after a respawn
    for (int i = 0; i < TRAIL_N; i++) {
        r->trail_x[i] = r->b.x;
        r->trail_y[i] = r->b.y;
    }
}

// The tempo of the level's own song: the beat clock things act on, the
// same in practice (where another song may play) and in every solver.
static int level_bpm(const Level *lv)
{
    return synth_bpm(lv->user_song ? SONG_USER + (lv->user_song - 1) % USER_SONGS : lv->song % NUM_SONGS);
}

void run_start(Run *r, const Level *lv, int practice, int auto_cp)
{
    shapes_init();
    memset(r, 0, sizeof(*r));
    r->lv = lv;
    r->lv_bpm = level_bpm(lv);
    r->practice = practice;
    r->auto_cp = auto_cp;
    r->attempt = 1;
    r->rng = 0x2545F491u;
    start_body(&r->start, lv);
    r->b = r->start;
    reset_motion(r);
}

void run_restart(Run *r)
{
    r->attempt++;
    r->ncp = 0;
    r->pct = 0;
    r->b = r->start;
    reset_motion(r);
}

static void add_checkpoint_at(Run *r, const Body *b)
{
    if (r->ncp == MAX_CHECKPOINTS) {
        memmove(&r->cps[0], &r->cps[1], sizeof(r->cps[0]) * (MAX_CHECKPOINTS - 1));
        r->ncp--;
    }
    r->cps[r->ncp++] = *b;
    r->cp_timer = 0;
    if (!r->race)
        r->sfx |= SFX_CHECKPOINT;
}

static void add_checkpoint(Run *r)
{
    add_checkpoint_at(r, &r->b);
}

// A race drops the trailing player on the leader's checkpoints, so they are
// only taken standing on something (cube, ball) or well inside the corridor.
int run_safe_spot(const Body *b)
{
    if (needs_ground(b->mode))
        return b->on_ground;
    float hh = run_half_h(b);
    return b->y - hh >= b->lo + TILE / 2 && b->y + hh <= b->hi - TILE / 2;
}

void run_set_practice(Run *r, int on)
{
    if (on == r->practice)
        return;
    r->practice = on;
    r->ncp = 0;
    if (on) {
        if (r->state == RUN_PLAY)
            add_checkpoint(r);
    } else {
        run_restart(r);
    }
}

static void respawn(Run *r)
{
    r->attempt++;
    if (r->practice && r->ncp > 0) {
        r->b = r->cps[r->ncp - 1];
    } else {
        r->ncp = 0;
        r->b = r->start;
    }
    reset_motion(r);
}

void run_teleport(Run *r, const Body *b)
{
    r->b = *b;
    r->state = RUN_PLAY;
    reset_motion(r);
    if (r->practice)
        add_checkpoint(r);
}

void run_rewind(Run *r)
{
    r->b = r->practice && r->ncp > 0 ? r->cps[r->ncp - 1] : r->start;
    reset_motion(r);
}

static int die(Run *r)
{
    if (r->practice && r->ncp > 0 && r->since_respawn < CP_DOOMED)
        r->ncp--;
    r->state = RUN_DEAD;
    r->state_frames = 0;
    r->shake = 14;
    r->sfx |= SFX_DEATH;
    fx_burst(r->parts, &r->look, r->b.x, r->b.y, &r->rng);
    return EV_DIED;
}

static const LevelObj *acting(const Run *r, const LevelObj *o, LevelObj *tmp);

int run_orb_waiting(const Run *r)
{
    for (int k = 0; k < r->ntouch; k++) {
        LevelObj tmp;
        int t = acting(r, &r->lv->objs[r->touch[k]], &tmp)->type;
        if ((t == OBJ_ORB_YELLOW || t == OBJ_ORB_PINK) && !r->touch_used[k])
            return 1;
    }
    return 0;
}

void run_jumped(Run *r)
{
    r->buffered = 0;
    r->jumps++;
    r->sfx |= SFX_JUMP;
}

static int complete(Run *r)
{
    Look party = r->look;
    party.v[LOOK_DEATH] = FX_FIREWORKS;
    r->state = RUN_DONE;
    r->state_frames = 0;
    r->pct = 100;
    r->sfx |= SFX_DONE;
    fx_burst(r->parts, &party, r->b.x, r->b.y, &r->rng);
    return EV_COMPLETE;
}

static void launch(Run *r, float v)
{
    Body *b = &r->b;
    b->vy = v * mode_def(b->mode)->boost * (b->mini ? MINI_JUMP : 1.0f) * (float)b->grav;
    b->on_ground = 0;
}

// A mode's corridor (ten cells for the ship, UFO and wave, eight for the
// ball) is centred on its portal and never below the ground.
void run_corridor(int mode, const LevelObj *o, float *lo, float *hi)
{
    const ModeDef *d = mode_def(mode);
    float cells = (float)(d->corridor ? d->corridor : 10);
    int c = (int)((float)o->y + 0.5f - cells / 2 + 100.0f) - 100;
    if (c < 0)
        c = 0;
    *lo = (float)(c * TILE);
    *hi = *lo + cells * TILE;
}

int run_portal_mode(int type)
{
    switch (type) {
    case OBJ_CUBE_PORTAL: return MODE_CUBE;
    case OBJ_SHIP_PORTAL: return MODE_SHIP;
    case OBJ_BALL_PORTAL: return MODE_BALL;
    case OBJ_UFO_PORTAL: return MODE_UFO;
    case OBJ_WAVE_PORTAL: return MODE_WAVE;
    }
    int m = MODE_ROBOT + type - OBJ_PORTAL_ROBOT;
    return type >= OBJ_PORTAL_ROBOT && type <= OBJ_PORTAL_SHAPESHIFTER && MODES[m] ? m : -1;
}

int run_mode_portal(int mode)
{
    static const int OLD[5] = { OBJ_CUBE_PORTAL, OBJ_SHIP_PORTAL, OBJ_BALL_PORTAL, OBJ_UFO_PORTAL, OBJ_WAVE_PORTAL };
    return mode < MODE_ROBOT ? OLD[mode] : OBJ_PORTAL_ROBOT + mode - MODE_ROBOT;
}

int run_choices(const Run *r, int in, int *out)
{
    int extra = mode_def(r->b.mode)->inputs & ~IN_CROSS & 0xFFFF, n = 0;
    out[n++] = in;
    out[n++] = in ^ IN_CROSS;
    for (int bit = 1; extra && bit <= extra && n < 8; bit <<= 1)
        if (extra & bit) {
            out[n++] = in ^ bit;
            out[n++] = in ^ bit ^ IN_CROSS;
        }
    return n;
}

// A mode with a corridor takes its portal's; the cube (and other free
// modes) keep the one they had, which nothing then uses.
void run_set_mode(Run *r, int mode, const LevelObj *o)
{
    Body *b = &r->b;
    const ModeDef *d = mode_def(mode);
    if (d->corridor && o)
        run_corridor(mode, o, &b->lo, &b->hi);
    if (b->mode == mode)
        return;
    b->morph_from = b->mode;
    b->morph_t = 0;
    b->mode = mode;
    b->vy *= 0.5f;
    b->rot = 0;
    if (d->corridor)
        b->on_ground = 0;
    memset(b->ms, 0, sizeof(b->ms));
    memset(b->mf, 0, sizeof(b->mf));
    if (d->enter)
        d->enter(b);
    r->sfx |= SFX_MORPH;
}

static void enter_portal(Run *r, const LevelObj *o)
{
    Body *b = &r->b;
    switch (o->type) {
    case OBJ_GRAVITY_UP:
    case OBJ_GRAVITY_DOWN: {
        int g = o->type == OBJ_GRAVITY_UP ? -1 : 1;
        if (b->grav != g) {
            b->grav = g;
            b->vy *= 0.5f;
            b->on_ground = 0;
        }
        break;
    }
    case OBJ_SPEED_05: b->speed = 0; break;
    case OBJ_SPEED_1: b->speed = 1; break;
    case OBJ_SPEED_2: b->speed = 2; break;
    case OBJ_SPEED_3: b->speed = 3; break;
    case OBJ_MINI_PORTAL: b->mini = 1; break;
    case OBJ_BIG_PORTAL: b->mini = 0; break;
    default: {
        int m = run_portal_mode(o->type);
        const ObjDef *d = obj_def(o->type);
        if (m >= 0)
            run_set_mode(r, m, o);
        else if (d && d->act)
            d->act(r, o);
        break;
    }
    }
}

// Ground, the top of the world, and the corridor of the flying modes.
// Touching them never kills.
static void clamp_bounds(Body *b, float hh)
{
    float floor_y = bounded(b->mode) ? b->lo : 0.0f;
    float ceil_y = bounded(b->mode) ? b->hi : WORLD_TOP;
    if (b->y - hh < floor_y) {
        b->y = floor_y + hh;
        if (b->vy < 0)
            b->vy = 0;
        if (b->grav > 0)
            b->on_ground = 1;
    }
    if (b->y + hh > ceil_y) {
        b->y = ceil_y - hh;
        if (b->vy > 0)
            b->vy = 0;
        if (b->grav < 0)
            b->on_ground = 1;
    }
}

// Returns 1 if the player dies on this solid. The wave dies on any touch;
// the ship and the UFO also slide along the underside of blocks.
static int hit_solid(Body *b, const Box *s, float prev_y, float hh)
{
    int flags = mode_def(b->mode)->flags;
    if (flags & MF_FRAGILE)
        return 1;
    int ship = (flags & MF_SLIDE) != 0;
    if (b->grav > 0) {
        if (b->vy <= 0 && prev_y - hh >= s->y1 - LAND_TOL) {
            b->y = s->y1 + hh;
            b->vy = 0;
            b->on_ground = 1;
            return 0;
        }
        if (ship && b->vy >= 0 && prev_y + hh <= s->y0 + LAND_TOL) {
            b->y = s->y0 - hh;
            b->vy = 0;
            return 0;
        }
    } else {
        if (b->vy >= 0 && prev_y + hh <= s->y0 + LAND_TOL) {
            b->y = s->y0 - hh;
            b->vy = 0;
            b->on_ground = 1;
            return 0;
        }
        if (ship && b->vy <= 0 && prev_y - hh >= s->y1 - LAND_TOL) {
            b->y = s->y1 + hh;
            b->vy = 0;
            return 0;
        }
    }
    float in = b->mini ? INNER / 2 : INNER;
    Box inner = { b->x - in, b->y - in, b->x + in, b->y + in };
    return overlap(&inner, s);
}

// A grouped object as it acts now (morph blocks change its type).
static const LevelObj *acting(const Run *r, const LevelObj *o, LevelObj *tmp)
{
    if (!o->group || !r->b.mb_to[(o->group - 1) % MAX_GROUPS])
        return o;
    *tmp = *o;
    tmp->type = (uint8_t)morph_type(r, &r->b, o);
    return tmp;
}

// Its kind in this run now: an ObjDef's kind_now can change it.
static int kind_of(const Run *r, const LevelObj *o)
{
    const ObjDef *d = obj_def(o->type);
    return d && d->kind_now ? d->kind_now(r, o) : SHAPES[o->type].kind;
}

// Checks object i, moved by (ox, oy), against the player. Returns 1 if the
// player dies; pads, orbs and portals being touched are added to now.
static int collide(Run *r, int i, float ox, float oy, float prev_y, int16_t *now, int *nnow)
{
    Body *b = &r->b;
    LevelObj tmp;
    const LevelObj *o = acting(r, &r->lv->objs[i], &tmp);
    int kind = kind_of(r, o);
    int egg = o->type == OBJ_EGG || ((o->type == OBJ_FAKE_WALL || o->type == OBJ_FAKE_FLOOR) && o->param);
    if ((kind == K_NONE && !egg) || kind == K_PASS)
        return 0;
    float hw = run_half_w(b), hh = run_half_h(b);
    Box s = obj_box(o);
    s.x0 += ox;
    s.x1 += ox;
    s.y0 += oy;
    s.y1 += oy;
    Box pb = { b->x - hw, b->y - hh, b->x + hw, b->y + hh };
    if (!overlap(&pb, &s))
        return 0;
    if (egg) {
        egg_take(r, o->param, (s.x0 + s.x1) / 2, (s.y0 + s.y1) / 2);
        return 0;
    }
    if (kind == K_SOLID)
        return hit_solid(b, &s, prev_y, hh);
    if (kind == K_HAZARD || kind == K_SAW) {
        float in = (mode_def(b->mode)->flags & MF_FRAGILE) ? 0 : b->mini ? HAZARD_INSET / 2 : HAZARD_INSET;
        Box hb = { pb.x0 + in, pb.y0 + in, pb.x1 - in, pb.y1 - in };
        return kind == K_SAW ? touch_circle(&hb, &s) : overlap(&hb, &s);
    }
    if (*nnow < MAX_TOUCH)
        now[(*nnow)++] = (int16_t)i;
    return 0;
}

static int substep(Run *r, int held, int input)
{
    Body *b = &r->b;
    const Level *lv = r->lv;
    const float dt = 1.0f / SUB;
    float hw = run_half_w(b), hh = run_half_h(b);

    mode_def(b->mode)->step(r, b, held, input);
    if (r->state == RUN_DEAD)
        return die(r);

    float prev_y = b->y;
    b->x += SPEEDS[b->speed] * dt;
    b->y += b->vy * dt;
    b->on_ground = 0;

    if (b->x >= (float)lv->end_x)
        return complete(r);

    clamp_bounds(b, hh);

    int16_t now[MAX_TOUCH];
    int nnow = 0;
    int i0 = level_first_at(lv, (int)((b->x - hw) / TILE) - 3);
    int last = (int)((b->x + hw) / TILE) + 2;
    for (int i = i0; i < lv->nobj && lv->objs[i].x <= last; i++)
        if (!obj_grouped(&lv->objs[i]) && collide(r, i, 0, 0, prev_y, now, &nnow))
            return die(r);
    for (int k = 0; k < lv->nmoving; k++) {
        int i = lv->moving[k];
        float ox, oy;
        run_group_offset(b, lv->objs[i].group, &ox, &oy);
        float d = ((float)lv->objs[i].x + 0.5f) * TILE + ox - b->x;
        if (d > -3 * TILE && d < 3 * TILE && collide(r, i, ox, oy, prev_y, now, &nnow))
            return die(r);
    }

    // Pads and portals act once when first touched; an orb acts while
    // touched if X has a press waiting, then not again until left.
    uint8_t used[MAX_TOUCH];
    for (int k = 0; k < nnow; k++) {
        LevelObj tmp;
        const LevelObj *o = acting(r, &lv->objs[now[k]], &tmp);
        int was = -1;
        for (int m = 0; m < r->ntouch; m++)
            if (r->touch[m] == now[k])
                was = m;
        used[k] = was >= 0 ? r->touch_used[was] : 0;
        if (mode_def(b->mode)->boost == 0 && o->type >= OBJ_PAD_YELLOW && o->type <= OBJ_ORB_PINK)
            continue;   // pads and orbs do nothing to the wave
        if (was < 0) {
            if (o->type == OBJ_PAD_YELLOW || o->type == OBJ_PAD_PINK) {
                launch(r, o->type == OBJ_PAD_YELLOW ? PAD_YELLOW_V : PAD_PINK_V);
                r->sfx |= SFX_PAD;
            } else if (SHAPES[o->type].kind == K_PORTAL) {
                enter_portal(r, o);
                r->sfx |= SFX_PORTAL;
            }
        }
        if (SHAPES[o->type].kind == K_TRIGGER && obj_def(o->type) && obj_def(o->type)->act)
            obj_def(o->type)->act(r, o);
        if (o->type == OBJ_COIN && o->param < MAX_COINS && !(b->coins & (1 << o->param))) {
            b->coins |= 1 << o->param;
            r->sfx |= SFX_COIN;
            fx_sparkle(r->parts, ((float)o->x + 0.5f) * TILE, ((float)o->y + 0.5f) * TILE, &r->rng);
        }
        if ((o->type == OBJ_ORB_YELLOW || o->type == OBJ_ORB_PINK) && !used[k] && r->buffered) {
            launch(r, o->type == OBJ_ORB_YELLOW ? ORB_YELLOW_V : ORB_PINK_V);
            r->buffered = 0;
            used[k] = 1;
            r->sfx |= SFX_ORB;
        }
    }
    memcpy(r->touch, now, sizeof(now[0]) * (size_t)nnow);
    memcpy(r->touch_used, used, (size_t)nnow);
    r->ntouch = nnow;
    if (r->race && b->on_ground && needs_ground(b->mode)) {
        r->ground = *b;
        r->ground_frame = r->frame;
    }
    return EV_NONE;
}

static void update_camera(Body *b)
{
    float target;
    if (bounded(b->mode)) {
        target = b->lo;
    } else {
        target = b->cam_y;
        float rel = b->y - b->cam_y;
        if (rel > 6.0f * TILE)
            target = b->y - 6.0f * TILE;
        if (rel < 2.0f * TILE)
            target = b->y - 2.0f * TILE;
        if (target < 0)
            target = 0;
    }
    b->cam_y += (target - b->cam_y) * 0.15f;
}

void run_turn_cube(Body *b)
{
    if (!b->on_ground) {
        b->rot += ROT_STEP / (b->mini ? MINI_JUMP : 1.0f) * (float)b->grav;
    } else {
        float q = b->rot / HALF_PI_F;
        float target = (float)(int)(q + (q < 0 ? -0.5f : 0.5f)) * HALF_PI_F;
        float d = target - b->rot;
        if (d > 0.25f) d = 0.25f;
        if (d < -0.25f) d = -0.25f;
        b->rot += d;
    }
    run_wrap_rot(b);
}

void run_wrap_rot(Body *b)
{
    if (b->rot > 2 * PI_F * 8)
        b->rot -= 2 * PI_F * 8;
    if (b->rot < -2 * PI_F * 8)
        b->rot += 2 * PI_F * 8;
}

// The nearest block face straight above (dir 1) or below (dir -1) the
// body's hitbox in its column, or the corridor's edge (the ground or the top
// of the world for a mode without one).
float run_surface(const Run *r, const Body *b, int dir)
{
    const Level *lv = r->lv;
    float hw = run_half_w(b), hh = run_half_h(b), x0 = b->x - hw + 1, x1 = b->x + hw - 1;
    float best = dir > 0 ? (bounded(b->mode) ? b->hi : WORLD_TOP) : (bounded(b->mode) ? b->lo : 0);
    int last = (int)(x1 / TILE) + 2;
    for (int i = level_first_at(lv, (int)(x0 / TILE) - 3); i < lv->nobj && lv->objs[i].x <= last; i++) {
        const LevelObj *o = &lv->objs[i];
        float ox = 0, oy = 0;
        if (obj_grouped(o))
            continue;
        LevelObj tmp;
        o = acting(r, o, &tmp);
        if (kind_of(r, o) != K_SOLID)
            continue;
        Box s = obj_box(o);
        if (s.x1 + ox <= x0 || s.x0 + ox >= x1)
            continue;
        if (dir > 0 && s.y0 + oy >= b->y + hh - LAND_TOL && s.y0 + oy < best)
            best = s.y0 + oy;
        if (dir < 0 && s.y1 + oy <= b->y - hh + LAND_TOL && s.y1 + oy > best)
            best = s.y1 + oy;
    }
    for (int k = 0; k < lv->nmoving; k++) {
        const LevelObj *o = &lv->objs[lv->moving[k]];
        float ox, oy;
        run_group_offset(b, o->group, &ox, &oy);
        LevelObj tmp;
        o = acting(r, o, &tmp);
        if (kind_of(r, o) != K_SOLID)
            continue;
        Box s = obj_box(o);
        if (s.x1 + ox <= x0 || s.x0 + ox >= x1)
            continue;
        if (dir > 0 && s.y0 + oy >= b->y + hh - LAND_TOL && s.y0 + oy < best)
            best = s.y0 + oy;
        if (dir < 0 && s.y1 + oy <= b->y - hh + LAND_TOL && s.y1 + oy > best)
            best = s.y1 + oy;
    }
    return best;
}

int run_obj_now(const Run *r, int i, float *x0, float *y0, float *x1, float *y1)
{
    LevelObj tmp;
    const LevelObj *o = acting(r, &r->lv->objs[i], &tmp);
    float ox = 0, oy = 0;
    if (obj_grouped(&r->lv->objs[i]))
        run_group_offset(&r->b, r->lv->objs[i].group, &ox, &oy);
    Box s = obj_box(o);
    *x0 = s.x0 + ox;
    *y0 = s.y0 + oy;
    *x1 = s.x1 + ox;
    *y1 = s.y1 + oy;
    return kind_of(r, o);
}

int run_body_hits(Run *r, Body *o, float prev_y)
{
    const Level *lv = r->lv;
    Body keep = r->b;
    int16_t now[MAX_TOUCH];
    int nnow = 0, dead = 0;
    float hw = run_half_w(o);
    r->b = *o;
    int i0 = level_first_at(lv, (int)((o->x - hw) / TILE) - 3), last = (int)((o->x + hw) / TILE) + 2;
    for (int i = i0; !dead && i < lv->nobj && lv->objs[i].x <= last; i++)
        dead = !obj_grouped(&lv->objs[i]) && collide(r, i, 0, 0, prev_y, now, &nnow);
    for (int k = 0; !dead && k < lv->nmoving; k++) {
        int i = lv->moving[k];
        float ox, oy;
        run_group_offset(&r->b, lv->objs[i].group, &ox, &oy);
        float d = ((float)lv->objs[i].x + 0.5f) * TILE + ox - r->b.x;
        dead = d > -3 * TILE && d < 3 * TILE && collide(r, i, ox, oy, prev_y, now, &nnow);
    }
    *o = r->b;
    keep.eggs |= r->b.eggs;         // an egg the other body touched is his too
    r->b = keep;
    return dead;
}

// Objects that act when the player's centre passes their column (K_PASS).
static void pass_triggers(Run *r, float x0, float x1)
{
    const Level *lv = r->lv;
    for (int i = level_first_at(lv, (int)(x0 / TILE)); i < lv->nobj && lv->objs[i].x * TILE <= x1; i++) {
        const LevelObj *o = &lv->objs[i];
        float tx = ((float)o->x + 0.5f) * TILE;
        if (SHAPES[o->type].kind == K_PASS && tx > x0 && tx <= x1 && obj_def(o->type)->act)
            obj_def(o->type)->act(r, o);
    }
}

int run_step(Run *r, int input)
{
    int edges = input & ~r->prev_input;
    r->prev_input = input;
    int held = (input & IN_CROSS) != 0;
    r->sfx = 0;
    r->egg_new = 0;
    r->song_t++;
    if (!held)
        r->held_lock = 0;
    if (r->held_lock) {
        held = 0;
        edges &= ~IN_CROSS;
    }
    r->frame++;
    r->state_frames++;
    if (r->shake > 0)
        r->shake--;
    if (r->state != RUN_DEAD || !rw_slowmo() || (r->state_frames & 1))
        fx_update(r->parts);

    if (r->state == RUN_DEAD) {
        if (r->state_frames >= (r->practice ? 36 : 50)) {
            respawn(r);
            return EV_RESPAWN;
        }
        return EV_NONE;
    }
    if (r->state == RUN_DONE)
        return EV_NONE;

    if (edges & IN_CROSS)
        r->buffered = 1;
    if (!held)
        r->buffered = 0;
    if (r->practice) {
        if ((edges & IN_SQUARE) && !(mode_def(r->b.mode)->inputs & IN_SQUARE))
            add_checkpoint(r);
        if ((edges & IN_TRIANGLE) && r->ncp > 0)
            r->ncp--;
        if (r->auto_cp && !r->race && ++r->cp_timer >= CP_AUTO && (r->b.on_ground || !needs_ground(r->b.mode)))
            add_checkpoint(r);
    }
    r->since_respawn++;
    r->b.t++;
    r->b.fx_t++;
    if (r->b.morph_t < MORPH_FRAMES)
        r->b.morph_t++;
    for (int g = 0; g < MAX_GROUPS; g++)
        if (r->b.grp[g].t < r->b.grp[g].n)
            r->b.grp[g].t++;

    float x0 = r->b.x;
    int jumps = r->jumps;
    for (int s = 0; s < SUB; s++) {
        int ev = substep(r, held, held ? input : input & ~IN_CROSS);
        if (ev != EV_NONE)
            return ev;
    }

    // A race checkpoint: the body standing on something during this frame
    // (holding X jumps again in the same frame), or well inside a corridor.
    if (r->practice && r->race && r->auto_cp && ++r->cp_timer >= CP_RACE) {
        if (r->ground_frame == r->frame && run_safe_spot(&r->ground))
            add_checkpoint_at(r, &r->ground);
        else if (!needs_ground(r->b.mode) && run_safe_spot(&r->b))
            add_checkpoint(r);
    }
    Body *b = &r->b;
    if (b->col_t < 256)
        b->col_t += 256 / 60 + 1;
    if (b->col_t > 256)
        b->col_t = 256;
    color_triggers(r, x0, b->x);
    move_triggers(r, x0, b->x);
    pass_triggers(r, x0, b->x);
    egg_zones(r, x0, b->x, r->jumps != jumps);
    transform_step(b);
    update_camera(b);
    mode_def(b->mode)->turn(b);
    for (int i = TRAIL_N - 1; i > 0; i--) {
        r->trail_x[i] = r->trail_x[i - 1];
        r->trail_y[i] = r->trail_y[i - 1];
    }
    r->trail_x[0] = b->x;
    r->trail_y[0] = b->y;
    int pct = (int)(b->x * 100.0f / (float)r->lv->end_x);
    r->pct = pct < 0 ? 0 : pct > 99 ? 99 : pct;
    return EV_NONE;
}

void run_start_at(Run *r, int x)
{
    const Level *lv = r->lv;
    Body *b = &r->b;
    *b = r->start;
    for (int i = 0; i < lv->nobj && lv->objs[i].x < x; i++) {
        const LevelObj *o = &lv->objs[i];
        if (o->type == OBJ_COLOR) {
            b->col_from = b->col_to = LEVEL_PALETTES[o->param % NUM_PALETTES];
        } else if (o->type == OBJ_MOVE && o->group >= 1 && o->group <= MAX_GROUPS) {
            start_move(b, o);
            b->grp[o->group - 1].t = b->grp[o->group - 1].n;
        } else if (SHAPES[o->type].kind == K_PORTAL) {
            enter_portal(r, o);
        } else if (SHAPES[o->type].kind == K_PASS) {
            obj_def(o->type)->act(r, o);
        }
    }
    transform_settle(b);
    b->morph_from = b->mode;
    b->morph_t = MORPH_FRAMES;
    b->x = (float)x * TILE + PLAYER_S / 2.0f;
    b->vy = 0;
    b->rot = 0;
    // A cube starts on the lowest cell in that column not taken by a block.
    int cell = 0;
    while (cell < 39) {
        int k = level_find(lv, x, cell);
        if (k < 0 || SHAPES[lv->objs[k].type].kind != K_SOLID)
            break;
        cell++;
    }
    b->y = bounded(b->mode) ? (b->lo + b->hi) / 2 : (float)cell * TILE + run_half_h(b);
    b->on_ground = !bounded(b->mode) && b->grav > 0;
    b->cam_y = bounded(b->mode) ? b->lo : 0;
    r->start = *b;
    reset_motion(r);
}
