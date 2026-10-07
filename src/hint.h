// Fritz Dash - "show me how": a search for button presses that get safely
// through the next few seconds from a practice checkpoint. It runs a few
// thousand physics steps a frame so the game never stalls.
#ifndef FRITZ_HINT_H
#define FRITZ_HINT_H

#include "play.h"

#define HINT_FRAMES 180         // the hint ghost plays three seconds
#define HINT_AHEAD  120         // and the path must stay safe two seconds past that
#define HINT_STEPS  3000        // physics steps searched per frame
#define HINT_MAX    (HINT_FRAMES + HINT_AHEAD)

enum { HINT_IDLE, HINT_SEARCH, HINT_READY, HINT_FAIL };

typedef struct {
    Body b;
    int buffered, held_lock, prev_input, ntouch;
    int16_t touch[MAX_TOUCH];
    uint8_t touch_used[MAX_TOUCH];
} HintState;

typedef struct {
    int state;
    int period, len;            // decisions every period frames; frames found
    long budget;                // steps left before this search gives up
    long steps;                 // steps used so far
    int sp;
    Body from;                  // where the hint starts
    uint16_t trace[HINT_MAX];
} Hint;

// Starts a search from run r's current body.
void hint_start(Hint *h, const Run *r);
// Searches up to max steps; returns the state (HINT_SEARCH while not done).
int hint_step(Hint *h, int max);
// A run set up at the hint's start, to replay the trace with run_step.
void hint_run(const Hint *h, const Run *like, Run *out);

#endif
