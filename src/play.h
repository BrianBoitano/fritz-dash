// Fritz Dash - playing a level: the modes (mode.h, one file each), objects,
// practice checkpoints and rendering. Platform independent, like the rest of the game.
#ifndef FRITZ_PLAY_H
#define FRITZ_PLAY_H

#include <stdint.h>
#include "level.h"

#define GROUND_Y   840     // screen y of the camera's floor line
#define PLAYER_X   420     // screen x of the player's left edge
#define PLAYER_S   76

#define MAX_TOUCH       16
#define MAX_CHECKPOINTS 64
#define MAX_PARTS       40
#define TRAIL_N         14
#define MORPH_FRAMES    12      // a portal's morph from one form to the next

// A group's move: its offset (world pixels) when the move started and where
// it ends, eased over n frames.
typedef struct {
    float x0, y0, x1, y1;
    int16_t t, n;
} GroupMove;

// Level transformations (transform.c). Drawing only, but kept in the body so
// a checkpoint brings them back. Each one eases toward its target.
typedef struct {
    uint8_t mirror, flip, rainbow, invert;      // targets: 0 off, 1 on
    uint8_t zoom_from, zoom_to;                 // ZOOMS indexes
    uint8_t theme_from, theme_to;               // THEMES indexes
    int16_t mirror_t, flip_t, rainbow_t, invert_t;  // 0..TF_EASE: how far on each one is
    int16_t zoom_t, theme_t;                    // frames since the zoom or the theme sweep began
    int16_t shake_t, shake_n;                   // shake frames left and how hard
    LevelColors sweep_from;                     // the colours a theme sweep is wiping away
} Transform;

// Player position and motion, and where the moving groups are. Saved whole
// as a practice checkpoint.
typedef struct {
    float x, y, vy, rot;    // centre of the player in world pixels, y up
    int mode, grav, speed;  // grav 1 pulls down, -1 pulls up; speed 0..3 is 0.5x..3x
    int mini;               // half size after a mini portal
    int on_ground;
    float lo, hi;           // floor and ceiling of the ship, ball, UFO and wave corridor (world y)
    float cam_y;            // world y shown on the screen's floor line
    LevelColors col_from, col_to;
    int col_t;              // 0..256 through a colour trigger's fade
    int t;                  // frames since the attempt started
    int coins;              // coins picked up this attempt, one bit per coin number
    GroupMove grp[MAX_GROUPS];
    int ms[4];              // the mode's own state (mode.h); cleared when the mode changes
    float mf[2];
    int morph_from, morph_t;    // drawing: the mode before the last change and frames since it
    float fx_y;             // drawing: a mode's own effect (the spider's zip starts here)
    int fx_t;               // and frames since it
    Transform tf;
    uint8_t mb_to[MAX_GROUPS];      // morph blocks: what group g + 1 turns into (MB_), 0 for nothing
    uint8_t mb_every[MAX_GROUPS];   // 0: once; n: back and forth every n beats
    int16_t mb_beat[MAX_GROUPS];    // the beat the change starts from
    int16_t pb_broke[6];    // crates and targets the blaster broke: 1 + object index, newest last
    uint8_t pb_groups;      // groups a target blew up (bit g - 1)
    uint8_t eggs;           // easter eggs taken this attempt: bit n - 1 for egg n (never read by physics)
    uint8_t zone_ok, zone_n;        // the egg zone being crossed: still earned, and orbs hit in it
    int16_t zone;           // 1 + that zone's object index, 0 for none
} Body;

typedef struct {
    float x, y, vx, vy;
    int life, max_life, size;
    color_t color;
} Particle;

// How the player looks (Customize). Each field indexes the LOOK_* lists.
enum { LOOK_COL1, LOOK_COL2, LOOK_FACE, LOOK_SHIP, LOOK_TRAIL, LOOK_DEATH, LOOK_PARTS };
typedef struct Look {
    int v[LOOK_PARTS];
    int stock;              // 0, or 1 + the stock icon worn instead of the FACE (stock.h)
} Look;
// Face codes for run_draw_face: LOOK_FACE values (built-in faces, MY ICONs),
// and FACE_STOCK0 + k for stock icon k.
#define FACE_STOCK0 64

typedef struct {
    const char *name;
    int stars, coins;       // stars and secret coins needed to unlock it
    int badge;              // 0, or 1 + the badge that unlocks it
} LookOption;

extern const int LOOK_COUNT[LOOK_PARTS];
extern const LookOption *const LOOK_OPTIONS[LOOK_PARTS];
extern const color_t LOOK_COLORS[];
// Sets the look used by every cube, ship and trail drawn after this.
void look_set(const Look *l);
// Customize preview: a hopping cube with its trail at screen (x, y), and a ship.
void look_draw_preview(int x, int y, int ship_y, int frame);
// The face code a look wears (a stock icon when it has one).
int look_face(const Look *l);
// The ship alone at screen (cx, cy), scale 1 = its size in play.
void look_draw_ship(float cx, float cy, float scale, float rot);

enum { RUN_PLAY, RUN_DEAD, RUN_DONE };
// Sounds the last run_step asked for (Run.sfx bits).
enum { SFX_JUMP = 1, SFX_ORB = 2, SFX_PAD = 4, SFX_PORTAL = 8, SFX_DEATH = 16, SFX_DONE = 32, SFX_CHECKPOINT = 64,
       SFX_COIN = 128, SFX_MORPH = 256, SFX_EGG = 512 };
// Mode pack B's sounds (high bits, so other packs can take the ones after SFX_MORPH).
enum { SFX_BLAST = 1 << 20, SFX_BREAK = 1 << 21, SFX_BOOM = 1 << 22, SFX_TICK = 1 << 23, SFX_SPLAT = 1 << 24,
       SFX_SWAP = 1 << 25, SFX_WHOO = 1 << 26 };
enum { EV_NONE, EV_DIED, EV_RESPAWN, EV_COMPLETE };

// Plain data that points at a level it never changes, so it can be copied by
// value (the simulator's solver copies it to look ahead).
typedef struct {
    const Level *lv;
    int state, state_frames, frame;
    Body b;
    Body start;             // where attempts begin: the level start, or a playtest cell
    int practice, auto_cp;
    int attempt, jumps;
    int pct;                // progress through the level, 0..100
    int buffered;           // X was pressed and no jump or orb has used it yet
    int held_lock;          // X is ignored until it is released
    int prev_input;
    int16_t touch[MAX_TOUCH];
    uint8_t touch_used[MAX_TOUCH];
    int ntouch;
    Body cps[MAX_CHECKPOINTS];
    int ncp, cp_timer, since_respawn;
    float trail_x[TRAIL_N], trail_y[TRAIL_N];
    Particle parts[MAX_PARTS];
    int shake;
    Look look;              // colours and death effect for the particles
    int sfx;
    uint32_t rng;
    int song_t, bpm;        // frames since the song started and its tempo: pulses beat with it
    int lv_bpm;             // the level's own song's tempo: the beat clock things act on (run_start sets it)
    int coins_had;          // coins already saved for this level (drawn faded)
    int eggs_had;           // its easter eggs already found (drawn faded), bit n - 1 for egg n
    int egg_new;            // eggs taken during the last run_step
    int coins_silver;       // a player-made level: its coins are silver
    int race;               // a 2 player race: quick silent checkpoints that are not drawn
    Body ground;            // race: the body the last time it stood on something
    int ground_frame;       // the frame that was in
} Run;

void run_start(Run *r, const Level *lv, int practice, int auto_cp);
// Starts from cell column x instead, with the speed, gravity, mode and colours
// the portals and triggers before it would have set (editor playtest).
void run_start_at(Run *r, int x);
// One 60 fps frame. Input is IN_CROSS held, plus IN_SQUARE (place) and
// IN_TRIANGLE (remove) checkpoints in practice. Returns an EV_ value.
int run_step(Run *r, int input);
void run_restart(Run *r);
// Back to the last practice checkpoint (or the start) without a death.
void run_rewind(Run *r);
// Carries on from body b (the race brings a trailing player back on screen).
void run_teleport(Run *r, const Body *b);
void run_set_practice(Run *r, int on);
// Standing on something, or well inside a flying corridor (race checkpoints).
int run_safe_spot(const Body *b);
float run_speed_px(int speed);
// Half the height of the player's hitbox at full size, and for a body (mini included).
float run_half_height(int mode);
float run_half_h(const Body *b);
float run_half_w(const Body *b);
// Where group g (1..MAX_GROUPS) has moved to, in world pixels.
void run_group_offset(const Body *b, int g, float *ox, float *oy);
// 1.0 on each beat of the song, falling to 0 just before the next.
float run_beat(const Run *r);
// The beat of the level's song frame t of an attempt is in (the beat clock
// SHIFTER portals and morph blocks act on), and how far through it (0..1).
int run_beat_at(const Run *r, int t);
float run_beat_frac(const Run *r, int t);
// Everything that decides how a run plays on from here, hashed: solvers use
// it to skip states already shown to fail.
uint64_t run_key(const Run *r);
// The corridor (floor and ceiling, world y) a mode portal sets, and the mode
// a portal type switches to (-1 for portals that do not change it), and back.
void run_corridor(int mode, const LevelObj *o, float *lo, float *hi);
int run_portal_mode(int type);
int run_mode_portal(int mode);
// The buttons a body's mode reads, and the inputs a solver tries next after
// in (in itself first): returns how many (2 for modes that only read X).
int run_choices(const Run *r, int in, int *out);
// A level object's collision kind (K_ in mode.h).
int obj_kind(int type);
// Turns cell point (x, y) clockwise by r quarter turns about the cell centre.
void cell_rotate(int r, float x, float y, float *ox, float *oy);

// Level colours right now, part way through any colour trigger fade.
LevelColors run_colors(const Run *r);
// Particle bursts (death effects), shared with the Customize preview.
void fx_burst(Particle *parts, const Look *look, float x, float y, uint32_t *rng);
void fx_update(Particle *parts);
void fx_draw(const Particle *parts, float cam_x, float cam_y, color_t bg);

enum { HUD_PROGRESS = 1, HUD_TEXT = 2 };
void run_draw(const Run *r, int hud);
// A second run drawn faded in the next run_draw (a ghost), or NULL; a hint
// ghost is brighter and says so.
void run_set_ghost(const Run *ghost, int hint);
// Where the hint ghost goes next (world x, y), drawn as dots ahead of it.
void run_set_hint_path(const float (*xy)[2], int n);
// The hint ghost's ring colours (it pulses between them).
#define HINT_RING_A RGB(120, 230, 255)
#define HINT_RING_B RGB(255, 255, 255)
// Where the player died before (world x, y pairs): small X marks in practice
// and a heat strip under the progress bar. Applies to every run_draw until changed.
void run_set_deaths(const int (*xy)[2], int n);
// The other racer, drawn in its own look in the next run_draw, or NULL.
void run_set_rival(const Run *rival);
// Where world point (x, y) was on screen in the last run_draw.
void run_screen_pos(float x, float y, int *sx, int *sy);
void run_draw_cube(float cx, float cy, float rot);
// The cube in the current look at any size (scale 1 = PLAYER_S): the pilot
// every vehicle carries. run_draw_face draws face code `face` alone.
void run_draw_pilot(float cx, float cy, float scale, float rot);
void run_draw_face(float cx, float cy, float scale, float rot, int face);
// A level with no player (editor view), and one object with that camera.
void level_draw(const Level *lv, LevelColors c, float cam_x, float cam_y, int frame);
void level_draw_object(const LevelObj *o, int frame);

// Backdrop only (gradient, grid, ground), also used behind the menus.
void run_draw_backdrop(LevelColors c, float cam_x, float cam_y);

#endif
