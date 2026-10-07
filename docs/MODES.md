# Modes and objects in Fritz Dash 3.2

A mode is the form the player is in (cube, ship, robot, ...). Since 3.2 every mode is one
file, `src/mode_<name>.c`, that fills in a `ModeDef`, and every object added since 3.2
fills in an `ObjDef`. The engine (`src/play.c`, `src/play_draw.c`), the editor, the
solvers (`tools/solver.c`, `src/hint.c`, `src/rate.c`) and the level loader all read
those tables, so a new mode or object never needs a `switch` case anywhere else.

The interface is `src/mode.h`. The two tables are in `src/modes.c`.

## The 30 modes

One button (X) unless noted. "Name" is what level files (`"mode"`, `portal_<name>`) and maps
(`portal <name> X Y`, `mode: <name>`) use. A corridor is the band of cells a flying mode
stays in, centred on its portal; its edges never kill.

| Mode | Name | How it plays |
| --- | --- | --- |
| CUBE | `cube` | X jumps; holding keeps jumping whenever it lands. |
| SHIP | `ship` | Holding X flies up, letting go sinks (10 cell corridor). |
| BALL | `ball` | X flips gravity while it rolls on a floor or ceiling; held, it flips on every landing (8 cells). |
| UFO | `ufo` | Every press flaps, in the air too (10 cells). |
| WAVE | `wave` | Straight diagonals, up while X is held and down when not; dies on any block, pads and orbs do nothing (10 cells). |
| ROBOT | `robot` | Holding X keeps the boost going: a tap hops about 1.4 cells, a full hold about 3.5. |
| SPIDER | `spider` | A press snaps it straight to the opposite surface and turns its gravity round (8 cells). |
| SWING | `swing` | Flies; every press turns its gravity round, so it swings in smooth arcs (10 cells). |
| JETPACK | `jetpack` | Holding X gives a gentle thrust, softer and slower than the ship (10 cells). |
| BUNNY | `bunny` | Jumps like the cube, plus one more jump in the air with a flip and a puff; landing gives it back. |
| GLIDER | `glider` | Holding X spreads the wing and drifts down slowly, each press is a small flap, letting go drops fast; updraft fans lift it (10 cells). |
| FROG | `frog` | Hold X on the ground to crouch and charge, let go to leap: a hop, a jump or a big leap, 12 frames per step. |
| DASHER | `dasher` | X jumps; a press in the air dashes straight ahead at double speed with no gravity. One dash per jump; landing or a dash crystal gives it back. |
| BLINK | `blink` | Runs but cannot jump; a press teleports 3 cells ahead through spikes and thin walls, stopping short of a block. Every press on the ground, once in the air. |
| GRAPPLE | `grapple` | Falls; holding X shoots a rope to the nearest anchor ring ahead and above and swings round it; letting go flies off with the swing (10 cells). |
| DRILL | `drill` | Runs like the cube; on dirt a press dives in and tunnels along under the surface, and a press inside bursts out with a big jump. Blocks stay hard. |
| SURFER | `surfer` | Rides the top of water; holding X dives under, letting go shoots him up and out, higher the deeper he dived. On land a press hops. |
| BALLOON | `balloon` | Floats up on its own; holding X lets air out and it sinks. Slow and floaty; spikes pop it (10 cells). |
| HELI | `heli` | Holding X lifts slowly, letting go sinks slowly; heavy, so it moves in smooth curves (10 cells). |
| POGO | `pogo` | Bounces on every landing, big if X is held as it lands; a press in the air slams straight down. |
| MAGNET | `magnet` | Holding X pulls it up to the ceiling, letting go drops it to the floor, with sparks (8 cells). |
| SLIME | `slime` | Jumps like the cube, but sticks to a wall in front and runs straight up it; a press on a wall splat-jumps 3 cells up it. |
| CANNON | `cannon` | A cannonball that rolls until a cannon barrel loads it; a press fires it along the barrel's aim in an arc to the next barrel (the fuse fires it anyway). |
| TWIN | `twin` | Two of him, one on the floor and one on the ceiling; X jumps both, and he dies if either hits something (8 cells). |
| GHOST | `ghost` | Holding X turns him see-through: he floats up and passes through ghost walls (spikes still hurt). A meter runs down while see-through and fills on the ground. |
| COLOR SWAP | `color_swap` | Runs but cannot jump; a press swaps pink and blue. Pink and blue blocks and gates are solid only in his own colour. |
| BLASTER | `blaster` | X jumps like the cube; Square fires a bolt that breaks the first crate or target ahead. In practice Square fires here, so its checkpoints are the automatic ones. |
| TIME BENDER | `time_bender` | A flying pocket watch: every press flaps like the UFO; holding X slows him to half speed for up to 2 seconds while the level's beat goes on (10 cells). |
| SKATER | `skater` | A press ollies; holding X on a rail keeps grinding it, a third faster with sparks, and the next press ollies to the next rail. |
| SHAPESHIFTER | `shapeshifter` | Every press switches between cube and ship on the spot with a morph: the cube falls and rolls, the ship climbs while X is held (10 cells). |

## The objects since 3.2

"Name" is the type in level files and in the map directive `obj <name> X Y [R] [c=.. g=.. f=.. m=..]`;
dirt and water also have map characters `D` and `Z`. Numbers: stage 1 below 100, pack A
100..104, pack B 130..138 (see `src/level.h`; never renumber).

| Object | Name | Editor group | How it plays |
| --- | --- | --- | --- |
| SHIFTER PORTAL | `shifter` | MODES | Cycles its list of modes (`m`) on the song's beat (`c`: every 1, 2 or 4 beats); he becomes the one lit as he enters. |
| MYSTERY PORTAL | `mystery` | MODES | Shows "?" and picks one of its modes by the attempt's frame clock. |
| MIRROR, FLIP, ZOOM, SHAKE, RAINBOW, INVERT, THEME MORPH | `mirror` .. `theme_morph` | TRANSFORM | Change only the drawing when he passes them (see `README.md`, Levels). |
| MORPH BLOCKS | `morph_blocks` | TRANSFORM | Turns a group into spikes, platforms, orbs, air or blocks, once or on the beat, blinking first. |
| UPDRAFT FAN | `fan` | JUMPS | Blows for 5 cells the way it is turned; lifts the glider fully and other modes by their pad strength (not the wave). |
| DASH CRYSTAL | `dash_crystal` | JUMPS | Touching it gives the dasher his air dash back. |
| ANCHOR RING | `anchor_ring` | JUMPS | The grapple's rope catches it; nothing else touches it. |
| DIRT | `dirt` | MODES | Soft ground: every mode can stand on it, and the drill dives in and tunnels through it. |
| WATER | `water` | MODES | The surfer rides and dives in it; every other mode passes through. |
| CANNON BARREL | `barrel` | MODES | Loads the cannon and fires it along its aim; Triangle sets AIM (15 degree steps), SWING (fixed, a little, a lot) and POWER (short, medium, far). |
| GHOST WALL | `ghost_wall` | MODES | A solid block, except for the ghost while he is see-through. |
| PINK BLOCK, BLUE BLOCK | `pink_block`, `blue_block` | MODES | Solid for every mode; for colour swap only in his own colour. |
| PINK GATE, BLUE GATE | `pink_gate`, `blue_gate` | MODES | A thin wall, solid the same way as the blocks of its colour. |
| CRATE | `crate` | MODES | A solid block that a blaster bolt breaks, or its group's target when shot. |
| TARGET | `target` | MODES | A solid block; shoot it to break it and blow up every crate in its group (`g`). |
| RAIL | `rail` | MODES | One way: it holds what lands on it from above and lets the rest by; the skater grinds it faster. Triangle: posts or no posts. |

## Adding a mode

1. Write `src/mode_<name>.c` (template below). The Makefile and
   `server/build_render.sh` pick up every `src/mode_*.c` on their own.
2. In `src/modes.c`, uncomment its line in your pack's block:
   `[MODE_GLIDER] = &MODE_GLIDER_DEF,`.
3. That is all the registration. Its numbers already exist: `MODE_GLIDER` in
   `src/level.h` and `OBJ_PORTAL_GLIDER` for its portal, reserved for all 25 planned
   modes in the order of the plan. Do not renumber them.

Then it is everywhere: its portal (`"t": "portal_<name>"` in level files, drawn as an
oval in its colour with its vehicle inside), `"mode": "<name>"` as a level's start
mode, the editor's MODES group ("GLIDER PORTAL", after the wave portal), the map
directive `portal <name> X Y`, the header `mode: <name>`, SHIFTER and MYSTERY lists,
the portal morph, practice checkpoints, ghosts, 2 player, "show me how" and the
difficulty rating.

### The ModeDef

```c
const ModeDef MODE_GLIDER_DEF = {
    .name = "glider", .label = "GLIDER", .portal_name = "portal_glider",
    .flags = MF_SLIDE, .corridor = 10, .half_w = PLAYER_S / 2.0f, .half_h = 30.0f,
    .boost = 0.72f, .inputs = IN_CROSS, .color = RGB(120, 220, 255),
    .step = glider_step, .enter = NULL, .turn = glider_turn, .draw = glider_draw,
    .trail = NULL, .noutline = 6, .outline = OUTLINE,
};
```

| Field | Meaning |
| --- | --- |
| `name` | Its name in level files and maps. `portal_name` must be `"portal_" name` (the sim checks). |
| `label` | What the editor and its portal say, in capitals. |
| `flags` | `MF_SLIDE`: slides along the underside of blocks (ship, UFO, swing, jetpack). `MF_FRAGILE`: dies on any block it touches, spikes have no inset (wave). `MF_GROUNDED`: in its corridor, checkpoints still need it on a floor or ceiling (ball, spider). |
| `corridor` | Cells of the corridor it flies in, centred on its portal and never below the ground; its edges never kill. 0: no corridor, it runs on the ground and the camera follows it like the cube. |
| `half_w`, `half_h` | Hitbox half sizes at full size; mini halves them. |
| `boost` | How hard pads and orbs push it (cube 1, ship 0.35). 0: pads and orbs do nothing to it (wave). |
| `inputs` | Buttons it reads, `IN_CROSS` for one-button modes. Every solver tries each button you list. |
| `color` | Its portal's colour. Pick one far from the others. |
| `step` | Physics, see below. |
| `enter` | Called when the body turns into this mode, after `ms`/`mf` are cleared. May be NULL. |
| `turn` | Once a frame after moving: set `b->rot` (drawing only). `run_turn_cube` gives the cube's spin. |
| `draw` | The vehicle, see below. |
| `trail` | Its own trail (the wave's zig-zag, the jetpack's smoke) or NULL for the one he picked. |
| `outline` | Up to 8 points around its shape (pixels from the centre, y down, facing right). The portal morph flies the old form's outline pixels to the new one's. |

### Physics: `step(Run *r, Body *b, int held, int input)`

Called `SUB` (4) times a frame, before the engine moves the body `run_speed_px(speed) / SUB`
to the right and `vy / SUB` up, then clamps it to its corridor and collides it. So a
mode only sets `b->vy` (or moves `b->y` itself, as the spider does) from:

- `held`: X is down (and not locked after a respawn). Holding behaviours use this.
- `r->buffered`: a press that no jump or orb has used yet. Tap behaviours use this, and
  must call `run_jumped(r)` when they use it (it clears it, counts a jump, plays the sound).
- `input`: every button down, for modes with more `inputs`.
- `b->on_ground`: it stood on something at the end of the last substep.
- `b->grav` (1 down, -1 up), `b->mini` (scale jumps by `MINI_JUMP`), `b->speed`.

Helpers: `run_orb_waiting(r)` (an orb wants this press, so do not use it),
`run_surface(r, b, dir)` (the nearest block face or corridor edge straight up or down),
`run_set_mode(r, m, o)` (turn into another mode), `run_half_h(b)`,
`run_obj_now(r, i, ...)` (object i's kind and box as it acts now, for a mode that looks
ahead, like the slime finding a wall) and `run_body_hits(r, o, prev_y)` (a second body,
like the twin's other half, against the level). A step may set `r->state = RUN_DEAD` to
die (the twin does when its other half hits something). `src/packb.h` has small helpers
pack B's modes share (objects near a column, cosmetic sparks from `r->parts`).

State a mode needs between frames goes in the body, never in a static:

- `b->ms[4]` (ints) and `b->mf[2]` (floats) are the mode's own physics state (the
  robot's boost left, the bunny's air jump). They are cleared when the mode changes,
  saved in practice checkpoints, copied by every solver and hashed into `run_key`.
- `b->fx_y` and `b->fx_t` are drawing-only state (`fx_t` counts frames on its own; set it
  to 0 to start an effect). They are not in `run_key`, so never let physics read them.

Rules: no libm (use `fsin`/`fcos` from `draw.h`), no randomness, no reading the song or
the wall clock. If physics depends on time, use the attempt clock `b->t` and
`run_beat_at(r, b->t)` (the beat of the level's own song) and give some object of yours
`OF_TIMED`, so solvers key on time in levels that use it.

### Art: `draw(const Body *b, float x, float y, int frame)`

Draw the vehicle centred on screen point (x, y) with the `veh_` helpers from `mode.h`.
Points are pixels from the centre, y down, facing right; they are scaled for mini (and
for the portal icon and the morph), turned by `rot`, and `flip` mirrors them top to
bottom (pass `b->grav < 0`).

- `veh_pilot(x, y, rot, flip, px, py, size)` draws his cube, with its face (built-in, MY
  ICON, and the stock icons once v32-icons merges), as the pilot. Every vehicle has one.
- `veh_poly`, `veh_rect`, `veh_square`, `veh_circle` for shapes.
- `veh_color(VC_1)`, `veh_color(VC_2)` are his two colours (faded for ghosts); `VC_WHITE`,
  `VC_DARK`. `veh_look(LOOK_SHIP)` and friends read the rest of his Customize choices.
- `veh_sx`/`veh_sy` turn world points into screen points (trails, streaks); `veh_bg()` is
  the colour things fade into.

Drawing is free to read anything in the body. It must not change it.

### Template

```c
// Fritz Dash - the glider: hold X to glide down slowly, tap for a small flap.
#include "input.h"
#include "mode.h"

#define GLIDE_G 0.6f

static void glider_step(Run *r, Body *b, int held, int input)
{
    (void)input;
    float g = (float)b->grav;
    if (r->buffered && !run_orb_waiting(r)) {
        b->vy = 9.0f * (b->mini ? MINI_JUMP : 1.0f) * g;
        run_jumped(r);
    }
    b->vy -= (held ? GLIDE_G * 0.4f : GLIDE_G) * g * (1.0f / SUB);
}

static void glider_turn(Body *b)
{
    b->rot = -b->vy * 0.02f;
}

static void glider_draw(const Body *b, float x, float y, int frame)
{
    (void)frame;
    int flip = b->grav < 0;
    static const float WING[8] = { -60, -10, 60, -10, 40, 4, -40, 4 };
    veh_poly(x, y, b->rot, flip, 4, WING, veh_color(VC_2));
    veh_pilot(x, y, b->rot, flip, 0, 10, 40);
}

static const float OUTLINE[8] = { -60, -10, 60, -10, 20, 30, -20, 30 };

const ModeDef MODE_GLIDER_DEF = {
    .name = "glider", .label = "GLIDER", .portal_name = "portal_glider", .flags = MF_SLIDE,
    .corridor = 10, .half_w = PLAYER_S / 2.0f, .half_h = 30.0f, .boost = 0.72f, .inputs = IN_CROSS,
    .color = RGB(120, 220, 255),
    .step = glider_step, .turn = glider_turn, .draw = glider_draw,
    .noutline = 4, .outline = OUTLINE,
};
```

### More than one button

List the extra buttons in `inputs` (for example `IN_CROSS | IN_R1`); solvers then try them
too, and their traces are 16 bit so any button fits. In practice Square places a
checkpoint, except in a mode that reads Square (the blaster fires with it; its
checkpoints come from the automatic ones). Ghost files (`src/ghost.c`) keep X and Square
(Square on a `square` line of its own that older files and builds do without); another
button would need its own line the same way.

## Adding an object

1. Take the next free number in your block in `src/level.h`: pack A uses
   `OBJ_PACK_A + 0 .. 29` (100..129), pack B `OBJ_PACK_B + 0 .. 29` (130..159), the eggs
   `OBJ_EGGS + 0 .. 29` (160..189). Add `OBJ_FAN = OBJ_PACK_A,` style lines under the
   block's comment. The numbers are part of `level_hash` (ghosts, VERIFIED, ratings), so
   never renumber an object once it exists.
2. Write its `ObjDef`, in its mode's file or a file of its own (`src/obj_<name>.c`; a
   new non-mode file also needs adding to `GAME_SRCS` in the Makefile, one word).
3. Add `[OBJ_FAN] = &OBJ_FAN_DEF,` (and its `extern`) under your block's comment in
   `OBJECTS` in `src/modes.c`.

Then it loads and saves by name (`"t": "fan"`), takes `obj fan X Y [R] [c=.. g=.. f=.. m=..]`
in maps, appears in its editor group, and collides, acts and draws through its ObjDef.

| Field | Meaning |
| --- | --- |
| `name` | Its type in level files and the `obj` directive (lower case, `_` for spaces). |
| `label` | The editor palette's name. |
| `cat` | Editor group: `PAL_BLOCKS`, `PAL_DANGER`, `PAL_JUMPS`, `PAL_GRAVITY`, `PAL_MODES`, `PAL_TRANSFORM`, `PAL_MOVING`, `PAL_DECO`, `PAL_SPECIAL`. |
| `kind` | `K_SOLID` (a block), `K_HAZARD` (kills), `K_SAW` (kills, round), `K_PORTAL` (acts once when first touched), `K_TRIGGER` (acts every substep while touched), `K_PASS` (acts when his centre passes its column, like the colour trigger; never touched), `K_NONE`. |
| `x0, y0, x1, y1` | Its box in cells, turned with the object (portals do not turn). |
| `flags` | `OF_TIMED` if it acts by the frame clock (solvers then key on time). |
| `act(Run *r, const LevelObj *o)` | What it does. Change only `r->b` (and `r->sfx`). |
| `draw(o, x, y, frame)` | Draws it; (x, y) is its cell's centre on screen. `draw_run()` is NULL in the editor, so triggers can draw a marker there and nothing in play. |
| `init(o)` | Settings of a newly placed one (NULL: none). |
| `nset`, `set_names`, `set_step`, `set_text` | Up to 6 settings rows. Triangle on it (or on its brush) opens a panel; left/right call `set_step(o, row, -1 or 1)`, `set_text` shows the value. `nset` 0: Triangle turns it. |
| `info(o, buf, n)` | The editor's "HERE:" line about it (keep it short: about 30 letters). |
| `kind_now(r, o)` | The kind it has in this run right now, or NULL for always `kind`: a crate the blaster broke (`K_NONE`), a colour block of the other colour, a ghost wall while he is see-through, a one-way rail. Collisions and `run_surface` use it. |

Settings live in the object's fields: `param` ("c", 0..255), `group` ("g", 1..8), `dx`,
`dy`, `time` ("dx", "dy", "f") and `list` ("m", up to 4 mode names). A trigger's "g" names
the group it acts on; triggers (`K_PASS`) are never members of a group themselves.

An object that needs state of its own during a run (a crate that breaks, a crystal that is
used up) needs a new field at the end of `Body` in `src/play.h`, hashed in `run_key` only
when it is non-zero (so older levels search exactly as before). Keep it small: bodies are
copied 64 times for checkpoints and thousands of times by the solvers. The blaster's
`pb_broke` (the last six crates it broke) and `pb_groups` (groups a target blew up) are
the example.

New editor objects go in a group the tests do not count: BLOCKS must keep its two
brushes and SPECIAL must keep SIGN third from the end, so pack B's objects are in MODES.

## Tests and demos (every mode and object)

- `tools/sim.c`: put your tests in your own `test_<pack>()` function and call it at the
  marked place at the end of `main()` (pack A, pack B and egg lines are separate so the
  branches merge cleanly). Test the physics (heights, what a press does, what holding
  does), the art (a screenshot with a face look, `shot_run`), and the editor if it has
  settings (`editor_brush_type` finds a brush by type).
- `samples/mode_<name>.txt`: a 20-40 s demo map. MODE LAB builds every one into the game
  (`gen_levels.py lab`) and plays it when he picks that mode, so give the mode a line in
  `HOW` in `src/lab.c` too. The sim requires one for every built
  mode, requires the solver to finish it on the 100 ms grid for all 6 offsets
  (comfort 6/6) and to spend at least 5 s in that mode, and screenshots it at 20/40/60/80 %.
  Put portals at y 1 so the cube reaches them without jumping.
- `make sim` must end `SIM OK (0 failures)`, and the "unchanged built-in levels replay the
  same solver traces" line must pass: a new mode or object must not change any existing
  level by a single frame. `tools/traces_base.txt` holds those traces from before 3.2;
  only remake it when physics change on purpose (`tools/traces.c` says how).
- Look at the screenshots (convert the PPMs with the builder's python3-pil) before
  calling it done.

## How the 3.2 pieces fit

- `src/mode.h`: ModeDef, ObjDef, the physics helpers and the `veh_` art helpers.
- `src/modes.c`: `MODES` and `OBJECTS`, with commented, reserved lines for every pack.
- `src/mode_cube.c` .. `mode_wave.c`: 3.0's modes, moved here unchanged.
- `src/mode_robot.c`, `mode_spider.c`, `mode_swing.c`, `mode_jetpack.c`, `mode_bunny.c`.
- `src/obj_portals.c`: SHIFTER (cycles its modes on the beat of the level's own song, every
  1, 2 or 4 beats; he becomes the one lit as he enters) and MYSTERY (shows "?", picks by a
  hash of the attempt clock), and the mode-list helpers `obj_list_*`.
- `src/transform.c`: the level transformation triggers (MIRROR, FLIP, ZOOM, SHAKE,
  RAINBOW, INVERT, THEME MORPH draw only; MORPH BLOCKS is physics) and the themes.
- `src/draw.c`: `draw_set_view` (mirror, flip, zoom), `draw_set_clip` (theme sweep),
  `draw_set_tint` (rainbow, invert) applied to every primitive, so no pixel is read back.
- A mode change plays a 12 frame morph (`MORPH_FRAMES`, drawing only) and `SFX_MORPH`.
