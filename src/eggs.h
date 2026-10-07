// Fritz Dash - easter eggs: the eggs hidden in built-in levels (named in the
// level files, so placing one is a map edit), the game eggs found by doing
// things around the game (each with a riddle), the rewards they unlock, the
// cracking-egg toast and the SECRETS book and page. Found eggs are saved in
// eggs.txt, one id per line in the order found ("03:2" is egg 2 of level 03,
// "g:konami" a game egg), then a "+<reward>" line for every reward turned on.
// docs/EGGS.md.
#ifndef FRITZ_EGGS_H
#define FRITZ_EGGS_H

#include <stdint.h>
#include "draw.h"
#include "play.h"

#define EGGS_FILE    "/data/fritzdash/eggs.txt"
#define EGG_ID       16
#define MAX_FOUND    600         // ids eggs.txt keeps
#define MAX_REG_EGGS 1024        // level eggs the book has room for (128 levels x MAX_EGGS)
#define EGG_TOASTS   6
#define EGG_TOAST_FRAMES 230
#define VAULT_EGGS   100         // the VAULT world opens at this many eggs
#define GOLDEN_EGGS  200
#define EGG_IDLE_FRAMES (2 * 60 * 60)   // the title's screensaver (and an egg) after 2 minutes untouched

// What a reward is. One of each kind up to RK_TITLE is worn at a time (the
// SECRETS page picks it); PARTY rewards are each on or off; ICON finds a
// secret stock icon; VAULT opens the VAULT world.
enum { RK_BG, RK_TRAIL, RK_DEATH, RK_FINISH, RK_FILTER, RK_PET, RK_SOUNDS, RK_SONG, RK_TITLE, RK_PICKED,
       RK_PARTY = RK_PICKED, RK_ICON, RK_VAULT, RK_COUNT };
enum { BGX_WARP = 1, BGX_AURORA, BGX_BUBBLES, BGX_CODE, BGX_SNOW, BGX_FIREWORKS, BGX_RETRO, BGX_CLOUDS, BGX_PLANETS };
enum { TRX_RAINBOW = 1, TRX_HEARTS, TRX_NOTES, TRX_LIGHTNING, TRX_DUST, TRX_SNOW, TRX_EGGS, TRX_CONFETTI };
enum { DEX_POOF = 1, DEX_GHOST, DEX_PIXELS, DEX_JELLY, DEX_EGG, DEX_RAINBOW };
enum { FIX_CHROME = 1, FIX_GLITCH, FIX_RAINBOW, FIX_GALAXY, FIX_NEON, FIX_CANDY, FIX_GOLDEN };
enum { TIX_NIGHT = 1, TIX_RAINBOW, TIX_EGGS, TIX_PARTY };
enum { PA_DISCO = 1, PA_BIGHEAD = 2, PA_TINY = 4, PA_CONFETTI = 8, PA_SLOWMO = 16, PA_FIREWORKS = 32, PA_HAT = 64,
       PA_GOOGLY = 128, PA_FLOOR = 256 };

typedef struct {
    const char *key;        // saved in eggs.txt; never change one
    const char *name;
    int kind, value;        // RK_ and its value (BGX_.., a PA_ bit, a pet, the n-th secret icon, ...)
} Reward;

typedef struct {
    const char *key;        // the id is "g:" key
    const char *name, *riddle;
    const char *reward;     // the key of the reward it unlocks
} GameEgg;

extern const Reward REWARDS[];
extern const int REWARD_COUNT;
extern const GameEgg GAME_EGGS[];
extern const int GAME_EGG_COUNT;
extern const char *const REWARD_KIND_NAMES[RK_COUNT];

// One registered level egg: its built-in level (BUILTIN_IDS index), number,
// name and hint.
typedef struct {
    int builtin;
    LevelEgg e;
    uint32_t paint;
    char id[EGG_ID];
} RegEgg;

// The built-in levels' eggs, registered as they are parsed at start-up.
void eggs_register(int builtin, const Level *lv);
int eggs_reg_count(void);
const RegEgg *eggs_reg(int i);
// "03:2" for egg n of built-in level b (its id's first two characters).
void eggs_level_id(int builtin, int n, char *out);

// The save: eggs_load reads eggs.txt (and turns on what it says); missing
// means nothing found yet.
void eggs_load(void);
int eggs_found(const char *id);
// Eggs found that the game still has (level eggs and game eggs), and all of them.
int eggs_count(void);
int eggs_total(void);
// Eggs found, of built-in level b, one bit each (bit n - 1 for egg n).
int eggs_level_mask(int builtin);
int eggs_game_found(void);
// Rewards: unlocked yet; the one of a kind being worn (0 for none); party bits.
int reward_unlocked(int r);
int reward_worn(int kind);
int reward_party(void);
// The reward the count-th egg found unlocks, or -1; the count it needs.
int reward_at_count(int count);
int reward_needs(int r);
// Turns reward r on (picking it for its kind) or off, and saves.
void reward_wear(int r, int on);
int reward_is_on(int r);
// The VAULT world is open.
int eggs_vault_open(void);
// The menu song: SONG_MENU, or the secret song worn.
int eggs_menu_song(void);
// The game's clock (month 1..12, day, hour), from the platform; 0 unknown.
void eggs_set_clock(int month, int day, int hour);

struct Game;
// Finds an egg (saved at once, with a toast). Returns 1 if it is new.
int eggs_find(struct Game *g, const char *id);
// Watching for game eggs, once a frame before the screen's own step
// (edit is the level in the editor), and after each frame of play.
void eggs_step(struct Game *g, int input, int edges, const Level *edit);
void eggs_after_run(struct Game *g, int ev);
void eggs_step_toast(struct Game *g);
void eggs_draw_toast(const struct Game *g);

// Where id is in the order eggs were found (1 for the first), 0 if not found.
int eggs_found_rank(const char *id);
// The name of the egg called id (and its paint), "?" if unknown.
const char *eggs_name(const char *id, uint32_t *paint);

// The SECRETS book (from the title) and the SECRETS page (Settings).
void secrets_open_book(struct Game *g);
int secrets_step_book(struct Game *g, int edges);       // 1: back
void secrets_draw_book(const struct Game *g);
void secrets_open_page(struct Game *g);
int secrets_step_page(struct Game *g, int edges);       // 1: back
void secrets_draw_page(const struct Game *g);

// --- rewards.c: what the worn rewards draw -------------------------------------------
// rewards_frame sets the clock their animations run on; rw_scene says what is
// being drawn: menus (finishes, hats and trails show), his own run (all of
// it), or a ghost or player 2 (none of it).
enum { RW_MENU, RW_PLAYER, RW_MUTED };
enum { RW_JUMP = 1 };
void rewards_frame(int frame);
void rw_scene(int s);
void rw_backdrop(LevelColors c, float cx, float cy);   // the worn background, behind the grid
int rw_floor(int gy, int x0, int x1, float cx);       // RAINBOW FLOOR's ground edge (1: drawn)
int rw_trail(const float *tx, const float *ty, color_t c1, color_t c2, color_t bg);  // 1: drew the worn trail
float rw_big(void);                                   // BIG HEAD's scale for his cube
color_t rw_body(color_t c1);                          // the cube's colour under a finish
void rw_cube(float cx, float cy, float s, float rot, color_t c1, color_t c2, int over);  // finish; hat and eyes (over)
void rw_pet(const float *tx, const float *ty, int mini);
int rw_burst(Particle *parts, const Look *look, float x, float y, uint32_t *rng);  // 1: a worn crash
int rw_slowmo(void);
void rw_dead(const Run *r);                    // the worn crash while he is dead
void rw_event(int what, float x, float y);            // RW_JUMP at world (x, y): confetti
void rw_tick(void);
float rw_zoom(void);                                  // TINY WORLD
float rw_hue(int beat);                               // DISCO's colour turn on this beat
int rw_filter(void);
void rw_overlay(const Run *r, float beat);     // confetti, disco, slow mo, fireworks, filter lines
void rw_title_back(void);
int rw_title_logo(int x, int y, int scale, int gap);  // 1: drew FRITZ DASH itself
void rw_screensaver(void);

#endif
