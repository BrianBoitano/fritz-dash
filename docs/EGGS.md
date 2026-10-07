# Easter eggs in Fritz Dash 3.2

Fritz finds eggs hidden in the built-in levels and eggs for doing things around the
game. Each one cracks open with a toast and a sound, is saved at once to
`/data/fritzdash/eggs.txt`, and most unlock a reward: a background, a trail, a crash
effect, a cube finish, a screen filter, a pet, a sound pack, a secret song, a title,
a party trick, a secret stock icon or the VAULT world. The SECRETS book (title menu)
has a slot for every egg; SECRETS in Settings turns each reward on and off.

Code: `src/obj_eggs.c` (the objects), `src/eggs.c` (registry, game eggs, rewards,
save, toast), `src/secrets.c` (book and page), `src/rewards.c` (what rewards draw).
Tests: the easter egg block at the end of `tools/sim.c`.

Today: 3 eggs in each of the 14 built-in levels (00 to 08 and the VAULT, 42) and 33 game
eggs, 75 in all. Levels 10 to 69 get theirs the same way as they arrive.

## Placing an egg in a level (a map edit, no C)

Every egg has a number N, 1 to 8, unique in its level. Its id is the level's
two-digit number and N: egg 2 of `03_gravity_grotto` is `03:2`. The id is what
eggs.txt saves, so never renumber an egg or a level once it has shipped.

1. One header line per egg, with its name (up to 20 characters) and its hint (up to
   60). Capitals, digits, spaces and `! : - . % / ? , ( ) + &` only (no apostrophes:
   the font has none).

       egg: 1 SECRET HALL | RUN INTO THE WALL THAT LOOKS TOO CLEAN

2. One object carrying `c=N`, as a directive inside any `map:` section. Directive
   cells count from that section's left edge, so putting all of them in the first
   section lets you use the level's own columns (the ones `SIM_PATH` prints):

       obj egg X Y c=N                 the egg itself, taken on touch
       obj fake_wall X Y c=N           a FAKE WALL with egg N inside it
       obj fake_floor X Y c=N          a FAKE FLOOR with egg N inside it
       obj egg_nojump X Y c=N dx=L     EGG ZONE: L cells from column X with no jump and no orb
       obj egg_mini X Y c=N dx=L       EGG ZONE: crossed at mini size all the way
       obj egg_upside X Y c=N dx=L     EGG ZONE: crossed upside down all the way
       obj egg_orbs X Y c=N dx=L       EGG ZONE: three orbs hit inside it

   Plain fake walls and floors (no `c`) hide a passage or a ledge:

       obj fake_wall X Y               looks like a block (#), he goes straight through
       obj fake_floor X Y [R]          looks like a platform (=), he falls through
       fake X0 Y0 X1 Y1                a rectangle of fake walls

`tools/gen_levels.py` refuses a level whose headers and egg objects do not match one to
one, a zone without `dx=1..40`, or a name or hint that is too long. Zones are invisible
in play and show as a dashed band in the editor. Fake walls glint once every few
seconds, so a sharp eye can spot them.

### What makes a good egg

Three per level, never on the way (the plain solver path should miss it), never
frame-perfect, and fair: the hint names the trick. Good spots: behind or inside a fake
wall, high above the path off a pad or orb, under a fake floor, on the other route of
a fork, inside a cluster of decoration, a zone for doing something unusual (not
jumping through a stretch, staying mini, staying upside down, three orbs).

### Proving it

The simulator proves every egg of every built-in level: for each one it searches for
a run that takes it and finishes the level, at frame precision and on the 100 ms press
grid (at least 4 of the 6 grid offsets), and checks that no spike or saw sits within 2
cells behind a fake wall (or 2 cells under a fake floor). Eggs, fake walls and zones
never change how a level plays: they are left out of `level_hash` and out of
`run_key`, so ghosts, VERIFIED ticks, ratings and every solver trace stay as they were.

To try spots before writing them into the map, build the simulator and run

    SIM_EGGS="03_gravity_grotto 40,5 F:61,0 n:80,0,6 W:60,0" build/sim

Each spot prints `fine` (1: a finishing run takes it), `grid` (offsets of 6) and `auto`
(1: the plain solution takes it anyway, so it is not a secret). `X,Y` is an egg,
`F:X,Y` a fake wall hiding one, `n: m: u: o:X,Y,L` the four zones, and `W:X,Y` adds a
plain fake wall for the spots after it. `SIM_PATH=<id>` prints the level with the
solver's path, with eggs as `E`, fake walls `F`, fake floors `f` and zones `Z`.

## Game eggs

`GAME_EGGS` in `src/eggs.c`: 33 eggs around the game, each with a riddle the book
always shows and its own reward. Adding one is a line there and a check in
`eggs_step` or `eggs_after_run`.

| Egg | How | Reward |
| --- | --- | --- |
| THE CODE | UP UP DOWN DOWN LEFT RIGHT LEFT RIGHT on the title | RETRO GRID background |
| LOGO TAPPER | touchpad 10 times on the title | RAINBOW LETTERS title |
| RAINBOW MAKER, DISCO FEVER, SNOW DAY | a level named RAINBOW, DISCO or SNOW | RAINBOW RIBBON trail, DISCO LIGHTS, SNOWFALL background |
| EGG WORDS, IT IS ME | EGG or FRITZ in his words under the logo | LITTLE EGGS trail, secret icon FRITZ |
| HAPPY FACE, FULL ICON | a smiley, or one colour all over, in the icon maker | GOOGLY EYES, PASTEL filter |
| DRUM ROLL, MELODY MAKER | a song with a drum, or a lead note, on every step | CHIPTUNE sounds, secret song MOON BOUNCE |
| SLEEPY TITLE | 2 minutes untouched on the title (a screensaver plays) | STARFIELD WARP background |
| SPOOKY NIGHT, PRESENT DAY, HAPPY NEW YEAR | playing on 31 October, 25 December, 1 January | GHOST FLOAT crash, SNOWFLAKES trail, FIREWORKS SKY |
| NIGHT OWL, EARLY BIRD | playing after 8 pm, or before 7 am | NIGHT title, AURORA SKY background |
| BOING X 1000 | 1000 jumps | BOING sounds |
| NEVER GIVE UP | 100 tries on one level | SLOW-MO CRASHES |
| SO CLOSE | crashing at 99% | GLITCH finish |
| SILENT WIN | beating a level with the music at 0 | secret song EGG HUNT |
| COIN SWEEP | every coin of a level in one run | FIREWORKS FINISH |
| FASHION SHOW, TWIN COLOURS | every Customize row changed; colour 1 = colour 2 | CANDY STRIPES finish, HANDHELD GREEN filter |
| ICON TOUR, BADGE TOUR | every STOCK ICONS set; the cursor on every badge | QUACK sounds, PARTY title |
| ENDLESS 25, PLAYER 2 WINS | 25 in Endless; player 2 winning a race | ROBOT sounds, CONFETTI JUMPS |
| WORLD BUILDER, BIG BUILDER | a FRITZ WORLD path of 5; 100 things in one level | CANDY CLOUDS background, NEON OUTLINE finish |
| CHECKPOINT KING, SHOW ME HOW, SHOULDER PRESS | 20 checkpoints in one practice run; SHOW ME HOW; L2+R2 held on the title | OLD MOVIE filter, LIGHTNING trail, THERMAL VISION filter |

## Rewards

81 rewards (`REWARDS` in `src/eggs.c`; keys are saved, never change one). The game
eggs unlock their own; the rest unlock as eggs are counted (`LADDER`): one at nearly
every count up to 9, then every few eggs, and every tenth egg a milestone.

- Backgrounds (9): STARFIELD WARP, AURORA SKY, UNDERWATER BUBBLES, CODE RAIN, SNOWFALL,
  FIREWORKS SKY, RETRO GRID, CANDY CLOUDS, PLANETS.
- Trails (8): RAINBOW RIBBON, HEARTS, MUSIC NOTES, LIGHTNING, PIXEL DUST, SNOWFLAKES,
  LITTLE EGGS, CONFETTI TRAIL.
- Crash effects (6): CARTOON POOF, GHOST FLOAT, PIXEL SHATTER, JELLY SPLAT, EGG CRACK,
  RAINBOW SPLASH.
- Cube finishes (7): CHROME, GLITCH, RAINBOW CYCLE, GALAXY, NEON OUTLINE, CANDY STRIPES,
  and the GOLDEN CUBE at 200 eggs.
- Screen filters (6): OLD TV LINES, HANDHELD GREEN, OLD MOVIE, NEON GLOW, THERMAL VISION,
  PASTEL. They map each colour as it is drawn, so they cost almost nothing.
- Party (9, each on or off): DISCO LIGHTS, BIG HEAD, TINY WORLD, CONFETTI JUMPS, SLOW-MO
  CRASHES, FIREWORKS FINISH, PARTY HAT, GOOGLY EYES, RAINBOW FLOOR.
- Pets (8, one at a time, following the cube): ROBO BUDDY, BIRDY, BOO, DRAGGY, MINI UFO,
  KITTY, SLIMEY, STARRY, at 10, 20 ... 80 eggs.
- Sound packs (4): BOING, QUACK, CHIPTUNE, ROBOT.
- Secret songs (3, played as the menu music): EGG HUNT, MOON BOUNCE, GOLDEN.
- Titles (4): NIGHT, RAINBOW LETTERS, EGG HUNT, PARTY.
- The 16 secret stock icons, each unlocked by an egg, then worn from STOCK ICONS.
- The VAULT world (levels 70 to 74: GOLD RUSH, CRYSTAL CAVERN, RUBY RUMBLE, EMERALD
  EXPRESS, THE GOLDEN EGG; 6 to 10 stars, every 3.2 mode) at 100 eggs; its card shows,
  locked, from the first egg.

Milestones: pets at 10 to 80, EGG HUNT title at 90, the VAULT at 100, then secret icons,
PLANETS, RAINBOW SPLASH, the GOLDEN song, NEON GLOW and GALAXY up to the GOLDEN CUBE at
200. Egg badges: EGG HUNTER (1), EGG COLLECTOR (50), VAULT KEY (100), GOLDEN EGG (200),
RIDDLE MASTER (every game egg).

Rewards are drawing and sound only: nothing physics or the solvers read changes.

## The book and the hints

The SECRETS book has a page of game eggs, then a page per world with a row of slots for
each level. A found egg is painted (its own colours and pattern, the same as in the
level) and says its name and what it unlocked. An unfound level egg is a grey "?" egg
until its level has been beaten; then the panel shows its hint. Game eggs always show
their riddle. L1/R1 turn the page, the D-pad moves, Circle goes back.
