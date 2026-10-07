// Fritz Dash - suggesting a difficulty for his levels. The game runs the
// solver's search a slice at a time (it never stalls a frame): can the level
// be finished, how many offsets of a 100 ms and a 170 ms press grid still
// finish it, and how busy the run is. The score is the home server's
// (server/fdlib.py rate), so the gallery and the game agree.
#ifndef FRITZ_RATE_H
#define FRITZ_RATE_H

#include "level.h"

#define RATE_STEPS 3000         // physics steps a frame
enum { RATE_IDLE, RATE_BUSY, RATE_DONE };
enum { RATING_EASY, RATING_NORMAL, RATING_HARD, RATING_HARDER, RATING_INSANE, RATING_DEMON, RATING_UNBEATEN,
       RATING_COUNT };

extern const char *const RATING_NAMES[RATING_COUNT];

typedef struct {
    int state;
    int phase;                  // 0 the fine search, 1..6 the 100 ms grid, 7..16 the 170 ms grid
    int period, sp;
    long budget, steps;
    int solved, comfort, relaxed, presses, held, mode_frames, max_speed, frames;
    int rating;                 // RATING_ value once done
    int score10;                // the score, times ten
    uint32_t hash;              // level_hash of the level rated
} Rate;

// Starts rating a copy of lv.
void rate_start(Rate *r, const Level *lv);
// Searches up to max steps; returns the state.
int rate_step(Rate *r, long max);
int rate_percent(const Rate *r);
// The fdlib score (times ten) and rating for these facts.
int rate_score10(int comfort, int relaxed, int max_speed, int modes, int presses, int frames, int hazards, int orbs,
                 int pads);
int rate_from_score10(int score10);

#endif
