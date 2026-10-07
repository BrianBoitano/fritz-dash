// Fritz Dash - the level solver shared by the simulator (tools/sim.c) and the
// home-server renderer (tools/render.c): a depth-first search over X held or
// released (and any other button the mode reads, run_choices), decided every
// `period` frames, skipping states run_key() says
// were already shown to fail. Desktop only (it is far too big for a frame).
#ifndef FRITZ_SOLVER_H
#define FRITZ_SOLVER_H

#include <stdint.h>
#include "../src/level.h"

#define SOLVE_DEPTH 16000
#define MAX_TRACE   60000

// The inputs of the last solution, one per frame from the level start.
extern uint16_t trace[MAX_TRACE];
extern int ntrace;
// Furthest x any searched run reached (to find where a level is stuck).
extern float solve_far;

// Coins a finish must pick up (one bit per coin number); 0 for none.
void want_coins(const Level *lv, int mask);
// Easter eggs a finish must take (bit n - 1 for egg n, egg objects or zones); 0 for none.
void want_eggs(const Level *lv, int mask);
// Returns 1 (trace filled), 0 (no way through) or -1 (search budget used up).
// phase is the length of the first decision (0: a whole period).
int solve(const Level *lv, int period, int phase, long budget);
// Plays a trace from the start; returns 1 if it finishes the level with the
// coins and eggs asked for.
int replay(const Level *lv, const uint16_t *tr, int n);

#endif
