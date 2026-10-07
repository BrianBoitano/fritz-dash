// Fritz Dash - badges: what earns them, the badge screen and the toast that
// pops up when one is earned. Saved in badges.txt as "<key> 1" lines.
#ifndef FRITZ_BADGES_H
#define FRITZ_BADGES_H

#include "draw.h"

#define BADGE_TOAST_FRAMES 200

enum {
    B_FIRST_WIN, B_STUDENT, B_ATTEMPTS100, B_ATTEMPTS1000, B_JUMPS, B_ORBS, B_NO_PRACTICE, B_FIRST_COIN,
    B_LEVEL_COINS, B_ALL_COINS, B_STAR_STORM, B_ALL_LEVELS, B_STARS10, B_BUILDER, B_MASTER_BUILDER,
    B_CREATOR, B_HOUR, B_PRACTICE_WIN, B_ENDLESS, B_COMEBACK, B_RACER, B_FASHION, B_WAVE,
    B_ISLES, B_CORAL, B_FORGE, B_NEON, B_COSMIC,    // every level of world 2..6 beaten (in level select order)
    B_WORLD_COINS, B_STARS100, B_STARS200, B_CORE,
    // 3.2: easter egg badges, in Game.badges2 (badge 32 + n is bit n).
    B_EGG_FIRST, B_EGG50, B_EGG100, B_EGG200, B_EGG_GAME,
    B_SHAPESHIFT,   // every level of world 7 SHAPESHIFT beaten
    BADGE_COUNT     // at most 64
};

typedef struct {
    const char *key, *name, *how;
    const char *label;      // drawn in the medal
    color_t color;
} BadgeDef;

extern const BadgeDef BADGES[BADGE_COUNT];

struct Game;
void badges_load(struct Game *g);
// Awards a badge (saved at once, with a toast) if it is not earned yet.
void badge_award(struct Game *g, int badge);
// Earned yet.
int badge_got(const struct Game *g, int badge);
// Checks the badges that follow from saved progress and stats.
void badges_update(struct Game *g);
int badges_count(const struct Game *g);
void badges_step_screen(struct Game *g, int edges);
void badges_draw_screen(const struct Game *g);
// The pop-up for a new badge, drawn over any screen.
void badges_draw_toast(const struct Game *g);
void draw_badge_medal(int cx, int cy, int r, int badge, int got);

#endif
