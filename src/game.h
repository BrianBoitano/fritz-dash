// Fritz Dash - the whole app: title menu, level select, playing a level,
// pause and completion screens, Endless mode, the level editor, Customize,
// Stats and Settings. Platform independent: the PS4 main loop and the
// desktop simulator (tools/sim.c) both drive this same code.
#ifndef FRITZ_GAME_H
#define FRITZ_GAME_H

#include <stdint.h>
#include "editor.h"
#include "eggs.h"
#include "endless.h"
#include "ghost.h"
#include "hint.h"
#include "icons.h"
#include "input.h"
#include "play.h"
#include "songs.h"
#include "stock.h"
#include "world.h"

#define DATA_DIR      "/data/fritzdash"
#define LEVELS_DIR    "/data/fritzdash/levels"
#define BEST_FILE     "/data/fritzdash/best.txt"
#define PROGRESS_FILE "/data/fritzdash/progress.txt"
#define SETTINGS_FILE "/data/fritzdash/settings.txt"
#define CUSTOM_FILE   "/data/fritzdash/custom.txt"
#define STATS_FILE    "/data/fritzdash/stats.txt"
#define COINS_FILE    "/data/fritzdash/coins.txt"
#define BADGES_FILE   "/data/fritzdash/badges.txt"
#define GHOSTS_DIR    "/data/fritzdash/ghosts"
#define TITLE_FILE    "/data/fritzdash/title.txt"
#define LAST_FILE     "/data/fritzdash/last.txt"

#define MAX_LEVELS    128
#define HINT_PATH     12
// The level select's worlds: built-in levels by the first digit of their id
// (0x is WORLD 1 FIRST JUMPS, 1x..5x the five after it, 6x SHAPESHIFT, 7x the
// VAULT, locked until VAULT_EGGS easter eggs), then his own files.
#define NUM_LWORLDS   9
#define LWORLD_SHAPESHIFT 6
#define LWORLD_VAULT  7
#define LWORLD_MINE   8

enum { ST_TITLE, ST_LEVELS, ST_PLAY, ST_PAUSE, ST_COMPLETE, ST_ENDLESS, ST_SETTINGS, ST_EDITOR, ST_CUSTOMIZE,
       ST_STATS, ST_BADGES, ST_RACE, ST_RACE_DONE, ST_SONGS, ST_ICONS, ST_WORLD, ST_TAGLINE, ST_STOCK, ST_BOOK,
       ST_SECRETS, ST_LAB };
// The title menu is two columns, PLAY (to TITLE_STATS) and MAKE; Up and Down
// go through them in this order, Left and Right jump across.
enum { TITLE_LEVELS, TITLE_WORLD, TITLE_TWO_PLAYER, TITLE_ENDLESS, TITLE_BADGES, TITLE_STATS, TITLE_LAB,
       TITLE_EDITOR, TITLE_SONGS, TITLE_ICONS, TITLE_CUSTOMIZE, TITLE_SETTINGS, TITLE_SECRETS, TITLE_COUNT };
#define TITLE_MAKE TITLE_EDITOR
#define TAGLINE_MAX 30
enum { STAT_ATTEMPTS, STAT_JUMPS, STAT_DEATHS, STAT_WINS, STAT_PRACTICE_WINS, STAT_SECONDS, STAT_ORBS, STAT_COUNT };

typedef struct {
    char name[32], author[24];
    char key[80];           // progress key: "builtin:<id>" or "file:<file name>"
    char file[64];          // file name in LEVELS_DIR; empty for built-in levels
    int builtin;            // index into BUILTIN_JSON, or -1
    int nobj, speed, stars;
    LevelColors colors;
    int attempts, best, practice_best;
    int ncoins, coins;      // secret coins in the level, and the ones saved (one bit each)
    int verified;           // beaten from the start in the editor since it last changed
    int rating;             // suggested difficulty (RATING_), or -1
    int world;              // 0..NUM_LWORLDS-1
} LevelInfo;

typedef struct {
    char id[EGG_ID];
    int reward, reward2;    // what it unlocked (REWARDS index), or -1
    int count;              // eggs found with it
} EggToast;

typedef struct Game {
    int state, state_frames, frame;
    int prev_input;
    int title_sel, level_sel, pause_sel, settings_sel, custom_row;
    int show_diag;
    int show_progress, auto_checkpoints, clean_hud, music_vol, sfx_vol, show_ghost, practice_song;
    int endless_best;
    int bad_files;          // level files in LEVELS_DIR that could not be loaded
    int nlevels;
    LevelInfo levels[MAX_LEVELS];
    Run run;
    Endless endless;
    Editor ed;
    SongMaker sm;
    IconMaker icons;
    StockBrowser stock;
    World world;
    char tagline[TAGLINE_MAX + 1];  // his words under the logo
    char newest[64];                // the level file he saved last
    Keyboard kb;
    int world_run;          // the run is a FRITZ WORLD level
    int world_result;       // what its finish did: 1 opened the next level, 2 finished the world
    int playtest;           // the run is an editor playtest (1) or a MODE LAB trial (PLAYTEST_LAB)
    int lab_sel;            // MODE LAB: the mode the cursor is on
    int verifying, just_verified;   // the playtest is a verify run; it has just verified the level
    Look look;              // what is saved and worn
    Look browse;            // what the Customize screen is showing
    int new_stars;          // stars the last finish earned, for the complete screen
    int new_coins;          // coins the last finish saved for the first time
    int song;               // song requested from the synth, -1 for none
    int stats[STAT_COUNT];
    int stat_frames;
    Particle preview[MAX_PARTS];
    uint32_t preview_rng;
    char msg[48];
    int msg_frames;
    uint32_t seed;
    Ghost ghost;            // the best run on this level, if any
    Ghost rec;              // this attempt's presses, from its first frame
    Run ghost_run;          // the ghost replaying alongside the player
    int ghost_t;            // frames the ghost has played; -1 when no ghost is shown
    Hint hint;              // "show me how" in practice
    Run hint_run;
    int hint_t, hint_cp;    // frames the hint has played (-1: not shown); checkpoints when it was asked for
    float hint_path[HINT_PATH][2];  // where the hint ghost goes next (world x, y)
    int hint_npath;
    int deaths[MAX_DEATHS][2];      // where the player died on this level (world x, y), oldest first
    int ndeaths;
    int two;                // the level select is choosing a 2 player race
    int picking;            // the level select shows the world cards
    int lworld_sel;         // the world card the cursor is on
    char last_key[NUM_LWORLDS][80];     // the level last played in each world (last.txt)
    int remix_ask;          // the level select is asking whether to remix the level
    int racing;             // the run being played (or paused) is a race
    int p2_here;            // a second controller is connected
    Run run2;               // player 2 in a race
    int race_t, winner, race_end;   // frames since the countdown began; 0 tie, 1 or 2, -1 racing
    uint32_t badges;        // one bit per badge earned (the first 32)
    uint32_t badges2;       // and the ones after them (badge 32 + n is bit n)
    int badge_toasts[8], nbadge_toasts, badge_toast_frames;
    int badge_sel;
    EggToast egg_toast[EGG_TOASTS];     // eggs found, popping up one at a time
    int negg_toasts, egg_toast_frames;
    int book_page, book_sel;            // the SECRETS book: its page and the egg chosen there
    int secret_row;                     // the SECRETS page's row
    int egg_keys[8];                    // the title's last d-pad presses (the code)
    int egg_taps, egg_idle, egg_hold;   // touchpad taps on the title, frames untouched there, frames L2+R2 held
    int egg_rows, egg_sets;             // Customize rows changed and STOCK ICONS sets visited (bits)
    uint64_t egg_badge_seen;            // badges the cursor has been on
    Look egg_look;                      // the look a frame ago
} Game;

void game_init(Game *g, uint32_t seed);
void game_step(Game *g, int input);
void game_draw(const Game *g);
// Small diagnostic text shown at the bottom-left of the title screen.
void game_set_diag(const char *text);
// Stars earned so far: built-in levels finished in normal mode.
int game_stars(const Game *g);
// Secret coins saved in built-in levels (coins in player-made levels do not count).
int game_coins(const Game *g);
// A world's levels: how many, beaten, stars and coins earned and in all, and
// its boss (slot 9, or the most stars) by index into Game.levels, or -1.
typedef struct {
    int count, beaten, stars, max_stars, coins, max_coins, boss;
} LWorldStats;
void game_lworld_stats(const Game *g, int w, LWorldStats *s);
extern const char *const LWORLD_NAMES[NUM_LWORLDS];

// Built-in levels, generated from levels/*.txt by tools/gen_levels.py.
extern const int BUILTIN_COUNT;
extern const char *const BUILTIN_IDS[];
extern const char *const BUILTIN_JSON[];

// MODE LAB (lab.c): every mode's demo, samples/mode_<name>.txt, built in by
// tools/gen_levels.py as LAB_*. A trial plays like a playtest (PLAYTEST_LAB):
// no stars, coins, eggs, ghosts, stats, badges or world progress.
#define PLAYTEST_LAB 2
#define LAB_BACK (-2)
extern const int LAB_COUNT;
extern const char *const LAB_IDS[];
extern const char *const LAB_JSON[];
// The LAB_JSON index of a mode's demo, or -1; its one-line "how to play".
int lab_level(int mode);
const char *lab_how(int mode);
// The MODE LAB screen: returns the mode X picked, LAB_BACK for Circle, or -1.
int lab_step(Game *g, int edges);
void lab_draw(const Game *g);

#endif
