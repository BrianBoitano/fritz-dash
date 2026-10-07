// Fritz Dash - helpers mode pack B's modes share (magnet .. shapeshifter):
// finding level objects near the player, a pack B object's box, and
// cosmetic sparks from the run's spare particles (drawing only, never physics).
#ifndef FRITZ_PACKB_H
#define FRITZ_PACKB_H

#include "mode.h"

// The level index of o (a copy morph blocks made has none of its own).
static inline int pb_index(const Run *r, const LevelObj *o)
{
    const Level *lv = r->lv;
    return o >= lv->objs && o < lv->objs + lv->nobj ? (int)(o - lv->objs) : level_find(lv, o->x, o->y);
}

// Indexes of the objects that can be in world columns x0..x1 now (moving
// ones included); returns how many (at most max).
static inline int pb_near(const Run *r, float x0, float x1, int *out, int max)
{
    const Level *lv = r->lv;
    int n = 0, last = (int)(x1 / TILE) + 2;
    for (int i = level_first_at(lv, (int)(x0 / TILE) - 3); i < lv->nobj && lv->objs[i].x <= last && n < max; i++)
        if (!obj_grouped(&lv->objs[i]))
            out[n++] = i;
    for (int k = 0; k < lv->nmoving && n < max; k++) {
        int i = lv->moving[k];
        float ox, oy;
        run_group_offset(&r->b, lv->objs[i].group, &ox, &oy);
        float cx = ((float)lv->objs[i].x + 0.5f) * TILE + ox;
        if (cx > x0 - 3 * TILE && cx < x1 + 3 * TILE)
            out[n++] = i;
    }
    return n;
}

// A pack B object's box now (world pixels): its ObjDef box turned with it
// and moved with its group. Its kind_now may call this (run_obj_now may not).
static inline void pb_box(const Body *b, const LevelObj *o, float *x0, float *y0, float *x1, float *y1)
{
    const ObjDef *d = obj_def(o->type);
    float ax, ay, bx, by, ox = 0, oy = 0;
    cell_rotate(o->rot, d->x0, d->y0, &ax, &ay);
    cell_rotate(o->rot, d->x1, d->y1, &bx, &by);
    if (obj_grouped(o))
        run_group_offset(b, o->group, &ox, &oy);
    *x0 = ((float)o->x + (ax < bx ? ax : bx)) * TILE + ox;
    *x1 = ((float)o->x + (ax < bx ? bx : ax)) * TILE + ox;
    *y0 = ((float)o->y + (ay < by ? ay : by)) * TILE + oy;
    *y1 = ((float)o->y + (ay < by ? by : ay)) * TILE + oy;
}

static inline uint32_t pb_rand(Run *r)
{
    uint32_t x = r->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->rng = x;
    return x;
}

static inline float pb_randf(Run *r, float lo, float hi)
{
    return lo + (hi - lo) * (float)(pb_rand(r) % 1000) / 1000.0f;
}

// Up to n sparks at world (x, y) from particles not in use: speeds vx +-
// spread and vy0..vy1 (px a frame; they fall), living about life frames.
static inline void pb_sparks(Run *r, float x, float y, int n, color_t c, float vx, float spread, float vy0, float vy1,
                             int life, int size)
{
    for (int i = 0; i < MAX_PARTS && n > 0; i++) {
        Particle *p = &r->parts[i];
        if (p->life > 0)
            continue;
        p->x = x;
        p->y = y;
        p->vx = vx + pb_randf(r, -spread, spread);
        p->vy = pb_randf(r, vy0, vy1);
        p->max_life = p->life = life / 2 + (int)(pb_rand(r) % (uint32_t)(life / 2 + 1));
        p->size = size / 2 + (int)(pb_rand(r) % (uint32_t)(size / 2 + 1));
        p->color = c;
        n--;
    }
}

#endif
