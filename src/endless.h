// Fritz Dash - Endless mode: the original one-button runner. Random obstacle
// patterns, speed that rises with the score, and a best score.
#ifndef FRITZ_ENDLESS_H
#define FRITZ_ENDLESS_H

#include <stdint.h>
#include "play.h"

#define MAX_OBS 48

enum { OBS_SPIKE = 1, OBS_BLOCK = 2 };
enum { EN_PLAY, EN_DYING, EN_OVER, EN_PAUSE };

typedef struct {
    float x;
    int y, w, h;
    uint8_t type;
    uint8_t counted;
} Obstacle;

// Plain data only, so the whole game can be copied by value (the simulator's
// autopilot copies it to look ahead).
typedef struct {
    int state;
    int frame, state_frames;

    float py, vy, rot;
    int on_ground;
    float speed;
    float gap_left;
    float bg_scroll, ground_scroll;

    Obstacle obs[MAX_OBS];
    int nobs;
    Particle parts[MAX_PARTS];
    float trail_y[TRAIL_N];

    int score, best, new_best, save_pending;
    int prev_input;     // buttons held last step, for press edges
    int jump_lock;      // ignore X until released (after a menu pick)
    int pause_sel;
    int shake;
    int cheer_frames, cheer_idx;
    int pal_from, pal_to, pal_t;
    int exit;           // the player chose to go back to the title menu
    uint32_t rng;
} Endless;

// X jumps (hold to keep jumping), Options pauses, X or Options retries after
// a game over and Circle leaves. Starts playing straight away.
void endless_init(Endless *g, int best, uint32_t seed);
void endless_step(Endless *g, int input);
void endless_draw(const Endless *g);

#endif
