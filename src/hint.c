// Fritz Dash - the "show me how" search: depth-first over X held or not,
// decided every few frames, keeping the same input first, skipping states
// already shown to fail. A comfortable 100 ms press grid is tried first.
#include <string.h>

#include "hint.h"
#include "input.h"

#define DEPTH     (HINT_MAX + 1)
#define MEMO_BITS 16

typedef struct {
    HintState s;
    int choice, len, nin;
    int in[8];              // inputs to try, the same as before first (run_choices)
} Node;

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
    for (uint32_t i = (uint32_t)k & ((1u << MEMO_BITS) - 1);; i = (i + 1) & ((1u << MEMO_BITS) - 1)) {
        if (memo[i] == k)
            return 1;
        if (!memo[i])
            return 0;
    }
}

static void memo_add(uint64_t k)
{
    uint32_t i = (uint32_t)k & ((1u << MEMO_BITS) - 1), tries = 0;
    while (memo[i] && memo[i] != k && ++tries < 64)
        i = (i + 1) & ((1u << MEMO_BITS) - 1);
    memo[i] = k;
}

static void begin(Hint *h, int period, long budget)
{
    memset(memo, 0, sizeof(memo));
    h->period = period;
    h->budget = budget;
    h->sp = 0;
    Node *n = &stack[0];
    load_state(&scratch, &n->s);
    n->choice = -1;
    n->len = period;
    n->nin = run_choices(&scratch, 0, n->in);
}

void hint_start(Hint *h, const Run *r)
{
    scratch = *r;
    scratch.practice = 0;
    scratch.auto_cp = 0;
    scratch.ncp = 0;
    scratch.held_lock = 1;      // as after a respawn
    scratch.buffered = 0;
    scratch.ntouch = 0;
    h->from = r->b;
    save_state(&scratch, &stack[0].s);
    h->state = HINT_SEARCH;
    h->steps = 0;
    h->len = 0;
    begin(h, 6, 60000L);
}

int hint_step(Hint *h, int max)
{
    long stop = h->steps + max;
    while (h->state == HINT_SEARCH && h->steps < stop) {
        if (h->sp < 0 || h->budget < 0) {
            if (h->period == 6) {
                begin(h, 2, 240000L);   // nothing on the comfortable grid: try finer
                continue;
            }
            h->state = HINT_FAIL;
            break;
        }
        Node *n = &stack[h->sp];
        if (++n->choice >= n->nin) {
            load_state(&scratch, &n->s);
            memo_add(run_key(&scratch));
            h->sp--;
            continue;
        }
        load_state(&scratch, &n->s);
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
        h->steps += f + 1;
        h->budget -= f + 1;
        int frames = h->sp * h->period + f;
        if (done || (!died && frames >= HINT_MAX - h->period)) {
            int len = 0;
            for (int i = 0; i <= h->sp; i++) {
                int k = i == h->sp ? f : stack[i].len;
                for (int t = 0; t < k && len < HINT_MAX; t++)
                    h->trace[len++] = (uint16_t)stack[i].in[stack[i].choice];
            }
            h->len = len;
            h->state = HINT_READY;
            break;
        }
        if (died || h->sp + 1 >= DEPTH || memo_has(run_key(&scratch)))
            continue;
        Node *c = &stack[++h->sp];
        save_state(&scratch, &c->s);
        c->choice = -1;
        c->len = h->period;
        c->nin = run_choices(&scratch, in, c->in);   // keep doing the same thing first
    }
    return h->state;
}

void hint_run(const Hint *h, const Run *like, Run *out)
{
    *out = *like;
    out->practice = 0;
    out->auto_cp = 0;
    out->ncp = 0;
    out->b = h->from;
    out->state = RUN_PLAY;
    out->state_frames = 0;
    out->held_lock = 1;
    out->buffered = 0;
    out->ntouch = 0;
    for (int i = 0; i < TRAIL_N; i++) {
        out->trail_x[i] = h->from.x;
        out->trail_y[i] = h->from.y;
    }
    for (int i = 0; i < MAX_PARTS; i++)
        out->parts[i].life = 0;
}
