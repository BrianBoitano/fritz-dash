// Fritz Dash - level transformations (transform.c): the trigger objects,
// easing their state each frame, morph blocks and the themes.
#ifndef FRITZ_TRANSFORM_H
#define FRITZ_TRANSFORM_H

#include "play.h"

#define TF_EASE      20         // frames a mirror, flip, rainbow or invert takes to turn
#define ZOOM_FRAMES  40
#define SWEEP_FRAMES 40         // a theme sweeping across the screen
#define NUM_ZOOMS    5
#define NUM_THEMES   8

// What a MORPH BLOCKS trigger turns its group into (its "c").
enum { MB_NONE, MB_SPIKES, MB_PLATFORMS, MB_ORBS, MB_AIR, MB_BLOCKS, MB_COUNT };

typedef struct {
    const char *name;
    LevelColors colors;
} Theme;

extern const float ZOOMS[NUM_ZOOMS];
extern const Theme THEMES[NUM_THEMES];     // 0 NEON (no decoration), DAY, NIGHT, SPACE, SNOW, LAVA, OCEAN, CANDY

// One frame of easing; and every change finished at once (playtest start).
void transform_step(Body *b);
void transform_settle(Body *b);
float transform_zoom(const Body *b);
// The type a grouped object acts as now; and for drawing, 1 while it blinks
// a warning, with next set to what it turns into next.
int morph_type(const Run *r, const Body *b, const LevelObj *o);
int morph_view(const Run *r, const Body *b, const LevelObj *o, int *next);

#endif
