// Fritz Dash - level solver for the simulator and the renderer (see solver.h).
#include <string.h>

#include "../src/hint.h"
#include "../src/input.h"
#include "solver.h"

#define MEMO_BITS 22

// Search nodes keep only what the physics reads (see HintState); one scratch
// run is loaded from a node before each step, which keeps copies small.
typedef struct {
    HintState s;
    int choice, len, nin;
    int in[8];              // inputs to try, the same as before first (run_choices)
} Node;

uint16_t trace[MAX_TRACE];
int ntrace;
float solve_far;

static Node stack[SOLVE_DEPTH];
static Run scratch;
static uint64_t memo[1u << MEMO_BITS];

static void node_save(const Run *r, HintState *s)
{
    s->b = r->b;
    s->buffered = r->buffered;
    s->held_lock = r->held_lock;
    s->prev_input = r->prev_input;
    s->ntouch = r->ntouch;
    memcpy(s->touch, r->touch, sizeof(s->touch));
    memcpy(s->touch_used, r->touch_used, sizeof(s->touch_used));
}

static void node_load(Run *r, const HintState *s)
{
    r->b = s->b;
    r->buffered = s->buffered;
    r->held_lock = s->held_lock;
    r->prev_input = s->prev_input;
    r->ntouch = s->ntouch;
    memcpy(r->touch, s->touch, sizeof(r->touch));
    memcpy(r->touch_used, s->touch_used, sizeof(r->touch_used));
    r->state = RUN_PLAY;
}

// Coins a finish must have picked up, and how far past each one a path may
// go without it before the search gives up on that path.
static int need_coins;
static float coin_limit[MAX_COINS];

void want_coins(const Level *lv, int mask)
{
    need_coins = mask;
    for (int i = 0; i < lv->nobj; i++) {
        const LevelObj *o = &lv->objs[i];
        if (o->type == OBJ_COIN && o->param < MAX_COINS)
            coin_limit[o->param] = obj_grouped(o) ? 1e9f : ((float)o->x + 6) * TILE;
    }
}

static int missed_coin(const Run *r)
{
    for (int k = 0; k < MAX_COINS; k++)
        if ((need_coins >> k & 1) && !(r->b.coins >> k & 1) && r->b.x > coin_limit[k])
            return 1;
    return 0;
}

// Eggs (and egg zones) a finish must have taken, the same way.
static int need_eggs;
static float egg_limit[MAX_EGGS];

void want_eggs(const Level *lv, int mask)
{
    need_eggs = mask;
    for (int i = 0; i < lv->nobj; i++) {
        const LevelObj *o = &lv->objs[i];
        int zone = o->type >= OBJ_EGG_NOJUMP && o->type <= OBJ_EGG_ORBS;
        if ((o->type == OBJ_EGG || o->type == OBJ_FAKE_WALL || o->type == OBJ_FAKE_FLOOR || zone) && o->param >= 1 &&
            o->param <= MAX_EGGS)
            egg_limit[o->param - 1] = obj_grouped(o) ? 1e9f : ((float)o->x + (zone ? o->dx : 0) + 6) * TILE;
    }
}

static int missed_egg(const Run *r)
{
    for (int k = 0; k < MAX_EGGS; k++)
        if ((need_eggs >> k & 1) && !(r->b.eggs >> k & 1) && r->b.x > egg_limit[k])
            return 1;
    return 0;
}

// run_key, plus the egg state while eggs are wanted (run_key leaves it out,
// so every other search is exactly as before eggs).
static uint64_t key_of(const Run *r)
{
    uint64_t k = run_key(r);
    if (need_eggs)
        k = (k ^ (uint64_t)(r->b.eggs | r->b.zone_ok << 8 | r->b.zone_n << 9 | (uint32_t)r->b.zone << 16)) *
            1099511628211ull | 1;
    return k;
}

// Probing stops after 64 slots, so a nearly full table forgets states
// instead of slowing every lookup down.
static int memo_has(uint64_t k)
{
    uint32_t i = (uint32_t)k & ((1u << MEMO_BITS) - 1);
    for (int n = 0; n < 64; n++, i = (i + 1) & ((1u << MEMO_BITS) - 1)) {
        if (memo[i] == k)
            return 1;
        if (!memo[i])
            return 0;
    }
    return 0;
}

static void memo_add(uint64_t k)
{
    uint32_t i = (uint32_t)k & ((1u << MEMO_BITS) - 1);
    for (int n = 0; n < 63 && memo[i] && memo[i] != k; n++)
        i = (i + 1) & ((1u << MEMO_BITS) - 1);
    memo[i] = k;
}

int solve(const Level *lv, int period, int phase, long budget)
{
    memset(memo, 0, sizeof(memo));
    int sp = 0;
    run_start(&scratch, lv, 0, 0);
    node_save(&scratch, &stack[0].s);
    stack[0].choice = -1;
    stack[0].len = phase ? phase : period;
    stack[0].nin = run_choices(&scratch, 0, stack[0].in);
    while (sp >= 0) {
        Node *n = &stack[sp];
        node_load(&scratch, &n->s);
        if (++n->choice >= n->nin) {
            memo_add(key_of(&scratch));
            sp--;
            continue;
        }
        int in = n->in[n->choice], died = 0, done = 0, f;
        for (f = 0; f < n->len; f++) {
            int ev = run_step(&scratch, in);
            if (ev == EV_DIED) {
                died = 1;
                break;
            }
            if (ev == EV_COMPLETE) {
                done = 1;
                break;
            }
        }
        budget -= f;
        if (scratch.b.x > solve_far)
            solve_far = scratch.b.x;
        if ((done && ((scratch.b.coins & need_coins) != need_coins || (scratch.b.eggs & need_eggs) != need_eggs)) ||
            (!done && !died && (missed_coin(&scratch) || missed_egg(&scratch)))) {
            done = 0;
            died = 1;
        }
        if (done) {
            ntrace = 0;
            for (int i = 0; i <= sp; i++) {
                int len = i == sp ? f + 1 : stack[i].len;
                for (int t = 0; t < len && ntrace < MAX_TRACE; t++)
                    trace[ntrace++] = (uint16_t)stack[i].in[stack[i].choice];
            }
            return 1;
        }
        if (budget < 0)
            return -1;
        if (died || sp + 1 >= SOLVE_DEPTH || memo_has(key_of(&scratch)))
            continue;
        Node *c = &stack[++sp];
        node_save(&scratch, &c->s);
        c->choice = -1;
        c->len = period;
        c->nin = run_choices(&scratch, in, c->in);   // keep doing the same thing first
    }
    return 0;
}

int replay(const Level *lv, const uint16_t *tr, int n)
{
    Run r;
    run_start(&r, lv, 0, 0);
    for (int i = 0; i < n; i++) {
        int ev = run_step(&r, tr[i]);
        if (ev == EV_DIED)
            return 0;
        if (ev == EV_COMPLETE)
            return (r.b.coins & need_coins) == need_coins && (r.b.eggs & need_eggs) == need_eggs;
    }
    return 0;
}
