// Fritz Dash - levels: object types, the level data and the JSON loader.
//
// A level is a list of objects on a grid of TILE-pixel cells. Cell (x, y) has
// its bottom-left corner at world pixel (x * TILE, y * TILE), with y counting
// up from the ground. Level files are JSON:
//
//   { "format": "fritzdash-level", "version": 1, "name": "MY LEVEL",
//     "author": "FRITZ", "speed": 1, "mode": "cube",
//     "colors": { "top": "#0a0428", "bottom": "#3e0a5c",
//                 "ground": "#140634", "line": "#00f0ff" },
//     "stars": 2, "song": 1,                (or "song": "user:3", his song 3)
//     "objects": [ { "t": "spike", "x": 12, "y": 0 },
//                  { "t": "spike", "x": 14, "y": 3, "r": 2 } ] }
//
// "mode" is cube, ship, ball, ufo, wave or a newer mode's name (MODES in
// modes.c). "speed" is the starting speed
// (0 = 0.5x, 1 = 1x, 2 = 2x, 3 = 3x), "stars"
// is what finishing the level earns, and "r" turns an object clockwise in
// quarter turns. A colour trigger's "c" is the palette it fades to. "g" puts
// an object in group 1..8; a move trigger's "g" is the group it moves, by
// "dx", "dy" cells over "f" frames when the player passes it. "verified" and
// "rated" hold the level_hash of the objects the level was beaten and rated
// with ("rating" is the difficulty), so any edit undoes them. "m" is a list
// of mode names (a SHIFTER or MYSTERY portal's modes, up to MAX_LIST).
// "eggs" names the easter eggs hidden in it, by number:
//   "eggs": [ { "n": 1, "name": "SECRET HALL", "hint": "RUN INTO A WALL" } ]
// (an egg object's or egg zone's "c" is its number; docs/EGGS.md).
// Unknown keys and object types are skipped, so newer files still load in
// older builds.
#ifndef FRITZ_LEVEL_H
#define FRITZ_LEVEL_H

#include <stdint.h>
#include "draw.h"

#define TILE 80
#define MAX_LEVEL_OBJS 6000
#define LEVEL_MAX_X 30000
#define MAX_GROUPS 8
#define MAX_COINS 8         // coins after the eighth do nothing
#define MAX_MOVING 512      // objects in groups that move collide and draw from this list

enum {
    OBJ_NONE,
    OBJ_BLOCK, OBJ_SPIKE, OBJ_HALF_SPIKE, OBJ_PLATFORM,
    OBJ_PAD_YELLOW, OBJ_PAD_PINK, OBJ_ORB_YELLOW, OBJ_ORB_PINK,
    OBJ_GRAVITY_UP, OBJ_GRAVITY_DOWN,
    OBJ_SPEED_05, OBJ_SPEED_1, OBJ_SPEED_2, OBJ_SPEED_3,
    OBJ_CUBE_PORTAL, OBJ_SHIP_PORTAL, OBJ_END, OBJ_COLOR,
    OBJ_BALL_PORTAL, OBJ_UFO_PORTAL, OBJ_WAVE_PORTAL, OBJ_MINI_PORTAL, OBJ_BIG_PORTAL,
    OBJ_MOVE, OBJ_SAW, OBJ_SAW_BIG, OBJ_PULSE, OBJ_PULSE_BARS,
    OBJ_DECO_GLOW, OBJ_DECO_STAR, OBJ_DECO_ARROW, OBJ_DECO_BLOCK, OBJ_DECO_SPIKES,
    OBJ_COIN, OBJ_SIGN,
    // 3.2: each new mode's portal (named by its ModeDef), reserved for all 25 modes.
    OBJ_PORTAL_ROBOT, OBJ_PORTAL_SPIDER, OBJ_PORTAL_SWING, OBJ_PORTAL_JETPACK, OBJ_PORTAL_BUNNY,
    OBJ_PORTAL_GLIDER, OBJ_PORTAL_FROG, OBJ_PORTAL_DASHER, OBJ_PORTAL_BLINK, OBJ_PORTAL_GRAPPLE,
    OBJ_PORTAL_DRILL, OBJ_PORTAL_SURFER, OBJ_PORTAL_BALLOON, OBJ_PORTAL_HELI, OBJ_PORTAL_POGO,
    OBJ_PORTAL_MAGNET, OBJ_PORTAL_SLIME, OBJ_PORTAL_CANNON, OBJ_PORTAL_TWIN, OBJ_PORTAL_GHOST,
    OBJ_PORTAL_COLOR_SWAP, OBJ_PORTAL_BLASTER, OBJ_PORTAL_TIME_BENDER, OBJ_PORTAL_SKATER, OBJ_PORTAL_SHAPESHIFTER,
    // 3.2 objects with an ObjDef (OBJECTS in modes.c), named there. The
    // numbers are part of level_hash: never reorder, only add in a block.
    OBJ_SHIFTER, OBJ_MYSTERY,
    OBJ_MIRROR, OBJ_FLIP, OBJ_ZOOM, OBJ_SHAKE, OBJ_RAINBOW, OBJ_INVERT, OBJ_MORPH_BLOCKS, OBJ_THEME,
    // Each block's objects go on the lines after its first, in order
    // (OBJ_FAN = OBJ_PACK_A, OBJ_DASH_CRYSTAL, ...). See docs/MODES.md.
    OBJ_PACK_A = 100,       // mode pack A's objects: 100..129
    OBJ_FAN = OBJ_PACK_A, OBJ_DASH_CRYSTAL, OBJ_ANCHOR, OBJ_DIRT, OBJ_WATER,

    OBJ_PACK_B = 130,       // mode pack B's objects: 130..159
    OBJ_BARREL = OBJ_PACK_B, OBJ_GHOST_WALL, OBJ_PINK_BLOCK, OBJ_BLUE_BLOCK, OBJ_PINK_GATE, OBJ_BLUE_GATE,
    OBJ_CRATE, OBJ_TARGET, OBJ_RAIL,

    OBJ_EGGS = 160,         // easter egg objects: 160..189
    OBJ_EGG = OBJ_EGGS, OBJ_FAKE_WALL, OBJ_FAKE_FLOOR,
    OBJ_EGG_NOJUMP, OBJ_EGG_MINI, OBJ_EGG_UPSIDE, OBJ_EGG_ORBS,

    OBJ_COUNT = 192
};

// Modes 0..4 are 3.0's; the rest are reserved for the 25 3.2 modes (a mode
// not built yet plays as the cube). MODES in modes.c says which exist.
enum {
    MODE_CUBE, MODE_SHIP, MODE_BALL, MODE_UFO, MODE_WAVE,
    MODE_ROBOT, MODE_SPIDER, MODE_SWING, MODE_JETPACK, MODE_BUNNY,
    MODE_GLIDER, MODE_FROG, MODE_DASHER, MODE_BLINK, MODE_GRAPPLE,
    MODE_DRILL, MODE_SURFER, MODE_BALLOON, MODE_HELI, MODE_POGO,
    MODE_MAGNET, MODE_SLIME, MODE_CANNON, MODE_TWIN, MODE_GHOST,
    MODE_COLOR_SWAP, MODE_BLASTER, MODE_TIME_BENDER, MODE_SKATER, MODE_SHAPESHIFTER,
    MODE_COUNT
};
// A mode's name in level files ("cube"), or NULL for a mode not built yet;
// and the mode with that name, or -1.
const char *mode_name(int mode);
int mode_from_name(const char *s);

#define MAX_LIST 4          // modes in a SHIFTER or MYSTERY portal's list

typedef struct {
    int16_t x, y;
    uint8_t type, rot, param;
    uint8_t group;          // 0, or the group it belongs to (a move trigger: the group it moves)
    int8_t dx, dy;          // move trigger: cells to move
    uint16_t time;          // move trigger: frames the move takes
    uint8_t list[MAX_LIST]; // "m": 1 + a mode for each entry, 0 after the last
} LevelObj;

typedef struct {
    color_t top, bottom, ground, line;
} LevelColors;

#define MAX_EGGS 8          // easter eggs a level can hide, numbered 1..MAX_EGGS
#define EGG_NAME 24
#define EGG_HINT 64
typedef struct {
    int n;
    char name[EGG_NAME], hint[EGG_HINT];
} LevelEgg;

#define NUM_SIGNS 24        // a sign's "c" picks its words from SIGN_TEXTS
extern const char *const SIGN_TEXTS[NUM_SIGNS];

#define NUM_PALETTES 12
extern const LevelColors LEVEL_PALETTES[NUM_PALETTES];
extern const char *const PALETTE_NAMES[NUM_PALETTES];

typedef struct {
    char name[32];
    char author[24];
    int speed, mode, stars, song;
    int user_song;      // 0, or 1..USER_SONGS: his song N plays instead ("song": "user:N")
    uint32_t verified;  // level_hash when it was beaten from the start in the editor ("verified")
    uint32_t rated;     // level_hash the suggested difficulty is for ("rated"), and the
    int rating;         // difficulty, RATING_ in rate.h ("rating")
    LevelColors colors;
    int end_x;          // world x (pixels) where the level is complete
    int nobj;
    int skipped;        // objects dropped: unknown type, out of range, or over the limit
    int ncoins;         // coins, numbered left to right in their param (0..MAX_COINS-1)
    int timed;          // something in it acts by the frame clock (OF_TIMED objects): solvers key on time
    int neggs;          // "eggs": names and hints of its easter eggs
    LevelEgg eggs[MAX_EGGS];
    int nmoving;
    int16_t moving[MAX_MOVING];      // indexes of objects in a group (see obj_grouped)
    LevelObj objs[MAX_LEVEL_OBJS];   // sorted by x, then y
} Level;

enum { LEVEL_OK = 0, LEVEL_BAD_JSON = -1, LEVEL_BAD_FORMAT = -2 };

const char *obj_name(int type);
int obj_type_from_name(const char *s);
// The object moves with its group (triggers stay where they are).
int obj_grouped(const LevelObj *o);
int level_parse(Level *lv, const char *text, int len);
// Index of the first object whose cell x is >= x.
int level_first_at(const Level *lv, int x);
// Editing, keeping objects sorted: the object in cell (x, y) or -1; put
// replaces whatever is in that cell (returns 0, or -1 when the level is full).
int level_find(const Level *lv, int x, int y);
int level_put(Level *lv, LevelObj o);
void level_remove(Level *lv, int index);
// A fingerprint of everything that decides how the level plays (ghosts);
// eggs, fake walls and egg zones (OF_COSMETIC) are left out.
uint32_t level_hash(const Level *lv);
// Beaten from the start in the editor since its last change.
int level_verified(const Level *lv);
// The suggested difficulty (RATING_ in rate.h), or -1 if it is out of date.
int level_rating(const Level *lv);
// Works out end_x, the coin numbers and the moving list again after editing.
void level_update_end(Level *lv);
// Writes the level as JSON. Returns the length, or -1 if max is too small.
int level_write(const Level *lv, char *buf, int max);
// The egg called n in "eggs", or NULL.
const LevelEgg *level_egg(const Level *lv, int n);

#endif
