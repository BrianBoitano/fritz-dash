// Fritz Dash - difficulty suggestion: a time-sliced copy of the solver.
#include <string.h>

#include "hint.h"
#include "input.h"
#include "rate.h"

#define DEPTH     8000
#define MEMO_BITS 20
#define PHASES    17

const char *const RATING_NAMES[RATING_COUNT] = { "EASY", "NORMAL", "HARD", "HARDER", "INSANE", "DEMON", "UNBEATEN" };

typedef struct {
    HintState s;
    int choice, len, nin;
    int in[8];              // inputs to try, the same as before first (run_choices)
} Node;

static Level lv;
static Node stack[DEPTH];
static Run scratch;
static uint64_t memo[1u << MEMO_BITS];

static void save_state(const Run *r, HintState *s)
{
    s->b = r->b;
    s->buffered = r->buffered;
    s->held_lock = r->held_lock;
    s->prev_input = r->prev_input;
    s->ntouch = r->ntouch;
    memcpy(s->touch, r->touch, sizeof(s->touch));
    memcpy(s->touch_used, r->touch_used, sizeof(s->touch_used));
}

static void load_state(Run *r, const HintState *s)
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

// Phase 0 is the fine search; then six offsets of the 100 ms grid and ten
// of the 170 ms one, with smaller budgets (the same as tools/render.c's
// facts, scaled down to keep the wait short).
static void begin(Rate *r)
{
    int p = r->phase;
    r->period = p == 0 ? 2 : p <= 6 ? 6 : 10;
    int phase = p == 0 ? 0 : p <= 6 ? p - 1 : p - 7;
    r->budget = p == 0 ? 3000000L : p <= 6 ? 600000L : 300000L;
    memset(memo, 0, sizeof(memo));
    run_start(&scratch, &lv, 0, 0);
    r->sp = 0;
    save_state(&scratch, &stack[0].s);
    stack[0].choice = -1;
    stack[0].len = phase ? phase : r->period;
    stack[0].nin = run_choices(&scratch, 0, stack[0].in);
}

static void count_hazards(int *hazards, int *orbs, int *pads)
{
    *hazards = *orbs = *pads = 0;
    for (int i = 0; i < lv.nobj; i++) {
        const char *t = obj_name(lv.objs[i].type);
        *hazards += strstr(t, "spike") || strstr(t, "saw");
        *orbs += !strncmp(t, "orb", 3);
        *pads += !strncmp(t, "pad", 3);
    }
}

int rate_score10(int comfort, int relaxed, int max_speed, int modes, int presses, int frames, int hazards, int orbs,
                 int pads)
{
    float secs = (float)frames / 60.0f;
    if (secs < 1)
        secs = 1;
    float s = 1.0f + 0.25f * (float)(10 - relaxed) + 0.8f * (float)(6 - comfort) +
              (float)(max_speed > 1 ? max_speed - 1 : 0) + (modes ? 1.5f : 0) + 2.0f * (float)presses / secs +
              0.3f * (float)hazards / secs + (orbs ? 0.5f : 0) + (pads ? 0.5f : 0);
    return (int)(s * 10.0f + 0.5f);
}

int rate_from_score10(int score10)
{
    static const int TOP[5] = { 26, 45, 71, 85, 105 };
    for (int i = 0; i < 5; i++)
        if (score10 < TOP[i])
            return i;
    return RATING_DEMON;
}

static void finish(Rate *r)
{
    int hazards, orbs, pads;
    count_hazards(&hazards, &orbs, &pads);
    r->score10 = r->solved ? rate_score10(r->comfort, r->relaxed, r->max_speed, r->mode_frames, r->presses, r->frames,
                                          hazards, orbs, pads) : 0;
    r->rating = r->solved ? rate_from_score10(r->score10) : RATING_UNBEATEN;
    r->state = RATE_DONE;
}

// The fine path's facts, from the nodes on the stack: presses, frames held,
// frames out of the cube and the top speed.
static void path_facts(Rate *r, int last_len)
{
    int prev = 0;
    r->presses = r->held = r->mode_frames = r->frames = 0;
    r->max_speed = lv.speed;
    for (int i = 0; i <= r->sp; i++) {
        const Node *n = &stack[i];
        int in = n->in[n->choice], len = i == r->sp ? last_len : n->len;
        r->presses += (in & IN_CROSS) && !(prev & IN_CROSS);
        r->held += (in & IN_CROSS) ? len : 0;
        r->mode_frames += n->s.b.mode != MODE_CUBE ? len : 0;
        if (n->s.b.speed > r->max_speed)
            r->max_speed = n->s.b.speed;
        r->frames += len;
        prev = in;
    }
}

void rate_start(Rate *r, const Level *l)
{
    memset(r, 0, sizeof(*r));
    lv = *l;
    r->hash = level_hash(&lv);
    r->state = RATE_BUSY;
    begin(r);
}

// The end of one phase: 1 if a path was found.
static void phase_done(Rate *r, int found)
{
    if (r->phase == 0) {
        r->solved = found;
        if (!found) {
            finish(r);
            return;
        }
    } else if (r->phase <= 6) {
        r->comfort += found;
    } else {
        r->relaxed += found;
    }
    if (++r->phase >= PHASES)
        finish(r);
    else
        begin(r);
}

int rate_step(Rate *r, long max)
{
    long stop = r->steps + max;
    while (r->state == RATE_BUSY && r->steps < stop) {
        if (r->sp < 0 || r->budget < 0) {
            phase_done(r, 0);
            continue;
        }
        Node *n = &stack[r->sp];
        load_state(&scratch, &n->s);
        if (++n->choice >= n->nin) {
            memo_add(run_key(&scratch));
            r->sp--;
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
        r->steps += f + 1;
        r->budget -= f + 1;
        if (done) {
            if (r->phase == 0)
                path_facts(r, f + 1);
            phase_done(r, 1);
            continue;
        }
        if (died || r->sp + 1 >= DEPTH || memo_has(run_key(&scratch)))
            continue;
        Node *c = &stack[++r->sp];
        save_state(&scratch, &c->s);
        c->choice = -1;
        c->len = r->period;
        c->nin = run_choices(&scratch, in, c->in);   // keep doing the same thing first
    }
    return r->state;
}

int rate_percent(const Rate *r)
{
    if (r->state == RATE_DONE)
        return 100;
    return r->phase * 100 / PHASES;
}
