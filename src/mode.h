// Fritz Dash - modes and newer objects as tables. Each mode lives in its own
// src/mode_<name>.c and defines a ModeDef (physics, hitbox, vehicle art,
// buttons, portal); each object added since 3.2 defines an ObjDef. MODES and
// OBJECTS in modes.c list them. docs/MODES.md says how to add one.
#ifndef FRITZ_MODE_H
#define FRITZ_MODE_H

#include "draw.h"
#include "play.h"

#define SUB       4             // physics substeps a frame
#define MINI_JUMP 0.78f         // mini jumps, flaps, pads and orbs are this much as strong
#define PI_F      3.14159265f
#define HALF_PI_F 1.57079633f

// Collision kinds. K_PORTAL objects act once when first touched, K_TRIGGER
// ones while touched, K_PASS ones when the player's centre passes their column.
enum { K_NONE, K_SOLID, K_HAZARD, K_TRIGGER, K_PORTAL, K_SAW, K_PASS };

enum {
    MF_SLIDE = 1,       // slides along the underside of blocks (ship, UFO)
    MF_FRAGILE = 2,     // dies on any block it touches; spikes have no inset (wave)
    MF_GROUNDED = 4,    // in its corridor, checkpoints still need it on a floor or ceiling (ball)
};

typedef struct ModeDef {
    const char *name;           // level files' "mode", map headers and the `portal <name>` directive
    const char *label;          // the editor and the HUD say this ("ROBOT")
    const char *portal_name;    // "portal_" + name: its portal's type in level files
    int flags;                  // MF_
    int corridor;               // cells of the corridor it flies in, centred on its portal; 0: none (cube)
    float half_w, half_h;       // hitbox half sizes at full size (mini halves them)
    float boost;                // pads and orbs push it this much as hard; 0: they do nothing
    int inputs;                 // buttons it reads (IN_ bits); solvers try each of them
    color_t color;              // its portal
    // Physics, each substep, before the body moves SPEED / SUB to the right
    // and by vy / SUB up: set b->vy (and b->ms, b->mf) from held (X is down)
    // and r->buffered (a press no jump or orb has used yet). input holds
    // every button down. Times are in substeps, distances in world pixels.
    // A step may set r->state = RUN_DEAD to die (the twin's other half hit something).
    void (*step)(Run *r, Body *b, int held, int input);
    void (*enter)(Body *b);     // just turned into this mode (NULL: nothing)
    void (*turn)(Body *b);      // once a frame after moving: b->rot and drawing state
    // The vehicle with his icon as its pilot, centred on screen (x, y). It
    // reads b->rot, b->grav, b->ms and b->fx_; mini is already in the scale.
    void (*draw)(const Body *b, float x, float y, int frame);
    void (*trail)(const float *tx, const float *ty);   // its own trail (NULL: the one he picked)
    int noutline;
    const float *outline;       // up to 8 points (pixels from the centre, y down): its shape in a morph
} ModeDef;

extern const ModeDef *const MODES[MODE_COUNT];
// MODES[mode], or the cube for a mode not built yet.
const ModeDef *mode_def(int mode);

// Editor palette groups.
enum { PAL_BLOCKS, PAL_DANGER, PAL_JUMPS, PAL_GRAVITY, PAL_MODES, PAL_TRANSFORM, PAL_MOVING, PAL_DECO,
       PAL_SPECIAL, PAL_COUNT };

enum {
    OF_TIMED = 1,               // acts by the frame clock: solvers key on time in levels with it
    OF_COSMETIC = 2,            // never changes how the level plays: left out of level_hash
};

typedef struct ObjDef {
    const char *name;           // its type in level files and the `obj <name>` map directive
    const char *label;          // the editor palette's name for it
    int cat;                    // editor palette group (PAL_)
    int kind;                   // K_
    float x0, y0, x1, y1;       // collision box in cells, before turning
    int flags;                  // OF_
    void (*act)(Run *r, const LevelObj *o);     // touched (K_PORTAL, K_TRIGGER) or passed (K_PASS)
    void (*draw)(const LevelObj *o, float x, float y, int frame);   // centre of its cell on screen
    void (*init)(LevelObj *o);  // a new one's settings (NULL: none)
    // Settings Triangle opens in the editor (nset 0: Triangle turns it).
    int nset;
    const char *set_names[6];
    void (*set_step)(LevelObj *o, int k, int step);
    void (*set_text)(const LevelObj *o, int k, char *buf, int n);
    void (*info)(const LevelObj *o, char *buf, int n);   // what it does, for the editor
    // The kind it has in this run right now (a broken crate, a colour block of
    // the other colour, a ghost wall while he is see-through); NULL: always kind.
    int (*kind_now)(const Run *r, const LevelObj *o);
} ObjDef;

extern const ObjDef *const OBJECTS[OBJ_COUNT];
// OBJECTS[type] or NULL (3.0's objects and mode portals have none).
const ObjDef *obj_def(int type);

// Shared physics (play.c).
// An orb is being touched that X has not used yet: it takes the press.
int run_orb_waiting(const Run *r);
// A jump used the press: counts it and plays the jump sound.
void run_jumped(Run *r);
// Where the body's top (dir 1) or bottom (dir -1) would stop moving straight
// up or down: the nearest block face in its column, or its corridor's edge.
float run_surface(const Run *r, const Body *b, int dir);
// Turns the body into mode m (a portal at o sets its corridor; o may be NULL).
void run_set_mode(Run *r, int m, const LevelObj *o);
// Object i of the level as it acts in this run now (moved with its group,
// morphed, kind_now): returns its kind (K_) and sets its box in world pixels.
int run_obj_now(const Run *r, int i, float *x0, float *y0, float *x1, float *y1);
// A second body (the twin's other half) after it moved from prev_y, against
// the level's blocks and hazards: it lands like the player but never uses
// pads, orbs or portals. Returns 1 if it dies.
int run_body_hits(Run *r, Body *o, float prev_y);
// The cube's turn (half a turn a jump in the air, settling square on the
// ground), and keeping rot in range after a mode's own turning.
void run_turn_cube(Body *b);
void run_wrap_rot(Body *b);

// Easter eggs (obj_eggs.c): egg n taken at world point (x, y) (saved by the
// game, never read by physics), and the egg zones after a frame that moved
// the player's centre from x0 to x1 (jumped: he jumped or used an orb).
void egg_take(Run *r, int n, float x, float y);
void egg_zones(Run *r, float x0, float x1, int jumped);
// A sparkle of particles (coins and eggs).
void fx_sparkle(Particle *parts, float x, float y, uint32_t *rng);
// A block or a platform (turned by its rot) in the level's colours in o's cell
// (fake walls and floors look exactly like them).
void draw_cell_block(const LevelObj *o, int platform);
// An egg (a painted one when paint is not 0) centred on screen (x, y), w wide,
// leaning by lean (pixels at its top), dim fading it into colour bg (0..256).
void draw_egg(float x, float y, float w, float lean, uint32_t paint, int dim, color_t bg);
// Rows r0..r1 - 1 of its 32 (a cracked egg in two halves).
void draw_egg_rows(float x, float y, float w, float lean, uint32_t paint, int dim, color_t bg, int r0, int r1);
// The paint of egg n of the level called name (the same in play and in the book).
uint32_t egg_paint(const char *name, int n);

// Vehicle art (play_draw.c). Points are pixels from the player's centre,
// y down, facing right, scaled for mini and turned by rot; flip mirrors
// them top to bottom (gravity upside down).
enum { VC_1, VC_2, VC_WHITE, VC_DARK };
color_t veh_color(int k);       // his colours (and white and dark), faded for a ghost
int veh_look(int part);         // his Customize choice (LOOK_)
float veh_scale(void);
void veh_set_scale(float s);
void veh_poly(float cx, float cy, float rot, int flip, int n, const float *pts, color_t c);
void veh_rect(float cx, float cy, float rot, int flip, float x0, float y0, float x1, float y1, color_t c);
void veh_square(float cx, float cy, float rot, int flip, float x, float y, float size, color_t c);
void veh_circle(float cx, float cy, float rot, int flip, float x, float y, float r, color_t c);
// His cube with its face (built-in, MY ICON or stock) as the pilot at
// (x, y) in the vehicle, size pixels across before scaling.
void veh_pilot(float cx, float cy, float rot, int flip, float x, float y, float size);
// Screen position of a world point (trails), and the background colour
// faded things fade into.
float veh_sx(float wx);
float veh_sy(float wy);
color_t veh_bg(void);
// The run being drawn (NULL in the editor and previews).
const Run *draw_run(void);
// A mode's vehicle on its own (portals, editor), and a portal's glowing
// frame in colour c centred on screen (x, y).
void draw_mode_icon(int mode, float x, float y, float scale, int frame);
void draw_portal_frame(float x, float y, color_t c, int frame);
// The modes in an object's list (LevelObj.list), and settings helpers for
// it: a list entry changed to the next built mode (entry 0 never empty).
int obj_list_len(const LevelObj *o);
void obj_list_step(LevelObj *o, int k, int step);
void obj_list_text(const LevelObj *o, int k, char *buf, int n);

#endif
