// Fritz Dash - ghost runs: the X presses of the best run on a level, saved in
// GHOSTS_DIR/<key>.ghost. Play is deterministic, so replaying the presses from
// the start plays the same run again.
#ifndef FRITZ_GHOST_H
#define FRITZ_GHOST_H

#include <stddef.h>
#include <stdint.h>

#define GHOST_MAX_FRAMES 36000      // ten minutes
#define MAX_DEATHS 200              // deaths remembered per level

typedef struct {
    int frames, pct;
    uint32_t hash;                  // level_hash of the level it was played on
    uint8_t bits[GHOST_MAX_FRAMES / 8];
    uint8_t square[GHOST_MAX_FRAMES / 8];   // Square held (the blaster fires with it)
} Ghost;

int ghost_bit(const Ghost *gh, int i);
int ghost_square(const Ghost *gh, int i);
// Adds one frame; returns 0 when the ghost is full. ghost_mark_square says
// Square was held in the frame just added.
int ghost_push(Ghost *gh, int x_held);
void ghost_mark_square(Ghost *gh);
// Reads the ghost for a level key; returns 1 if there is one for this level.
int ghost_load(Ghost *gh, const char *key, uint32_t hash);
int ghost_save(const Ghost *gh, const char *key);
// Where the player died on a level, oldest first: GHOSTS_DIR/<key>.deaths,
// one "<x> <y>" line (world pixels) per death.
int deaths_load(int (*xy)[2], const char *key);
void deaths_save(const int (*xy)[2], int n, const char *key);
// The file a level key's ghost lives in (also used for its death list).
void ghost_path(const char *key, const char *ext, char *out, size_t n);

#endif
