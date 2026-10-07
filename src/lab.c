// Fritz Dash - MODE LAB: every mode with its vehicle and a line on how to
// play it. X tries the mode on its demo map (samples/mode_<name>.txt, built
// in as LAB_JSON), a trial that never counts for stars, coins, badges, eggs,
// stats or world progress.
#include <stdio.h>
#include <string.h>

#include "draw.h"
#include "game.h"
#include "input.h"
#include "mode.h"
#include "play.h"
#include "synth.h"

#define LAB_COLS 6
#define COL_CUBE   RGB(90, 255, 40)
#define COL_YELLOW RGB(255, 235, 40)
#define COL_WHITE  RGB(255, 255, 255)
#define COL_DIM    RGB(130, 130, 170)

static const char *const HOW[MODE_COUNT] = {
    [MODE_CUBE] = "X JUMPS. HOLD IT TO KEEP JUMPING",
    [MODE_SHIP] = "HOLD X TO FLY UP, LET GO TO SINK",
    [MODE_BALL] = "X FLIPS GRAVITY WHILE IT ROLLS",
    [MODE_UFO] = "EVERY PRESS IS A FLAP, IN THE AIR TOO",
    [MODE_WAVE] = "HOLD X TO ZIG UP, LET GO TO ZAG DOWN. NEVER TOUCH A BLOCK",
    [MODE_ROBOT] = "HOLD X LONGER TO JUMP HIGHER",
    [MODE_SPIDER] = "TAP X TO SNAP TO THE OTHER SIDE",
    [MODE_SWING] = "EVERY TAP TURNS GRAVITY ROUND: SWING UP AND DOWN",
    [MODE_JETPACK] = "HOLD X FOR A GENTLE LIFT",
    [MODE_BUNNY] = "PRESS X AGAIN IN THE AIR FOR A SECOND JUMP",
    [MODE_GLIDER] = "HOLD X TO GLIDE, TAP TO FLAP. FANS LIFT YOU",
    [MODE_FROG] = "HOLD X TO CHARGE, LET GO TO LEAP",
    [MODE_DASHER] = "JUMP, THEN PRESS X IN THE AIR TO DASH",
    [MODE_BLINK] = "TAP X TO TELEPORT 3 STEPS AHEAD",
    [MODE_GRAPPLE] = "HOLD X TO ROPE A RING AND SWING, LET GO TO FLY",
    [MODE_DRILL] = "TAP X ON DIRT TO DIG IN, TAP AGAIN TO POP OUT",
    [MODE_SURFER] = "HOLD X TO DIVE, LET GO TO SPLASH UP HIGH",
    [MODE_BALLOON] = "IT FLOATS UP. HOLD X TO SINK",
    [MODE_HELI] = "HOLD X TO LIFT SLOWLY, LET GO TO SINK SLOWLY",
    [MODE_POGO] = "HOLD X AS YOU LAND FOR A BIG BOUNCE, TAP TO SLAM",
    [MODE_MAGNET] = "HOLD X TO STICK TO THE CEILING",
    [MODE_SLIME] = "RUN UP WALLS. TAP X ON A WALL TO SPLAT UP",
    [MODE_CANNON] = "TAP X TO FIRE TO THE NEXT BARREL",
    [MODE_TWIN] = "X JUMPS BOTH TWINS. KEEP BOTH SAFE",
    [MODE_GHOST] = "HOLD X TO GO SEE-THROUGH PAST GHOST WALLS",
    [MODE_COLOR_SWAP] = "TAP X TO SWAP PINK AND BLUE",
    [MODE_BLASTER] = "X JUMPS, SQUARE FIRES AT CRATES",
    [MODE_TIME_BENDER] = "TAP TO FLAP, HOLD X TO SLOW TIME",
    [MODE_SKATER] = "TAP X TO OLLIE, HOLD IT TO GRIND A RAIL",
    [MODE_SHAPESHIFTER] = "EVERY TAP: CUBE, THEN SHIP, THEN CUBE",
};

static const LevelColors LAB_COLORS = { RGB(6, 20, 30), RGB(20, 90, 110), RGB(4, 14, 22), RGB(120, 255, 200) };

const char *lab_how(int mode)
{
    return mode >= 0 && mode < MODE_COUNT && HOW[mode] ? HOW[mode] : "";
}

int lab_level(int mode)
{
    char want[40];
    if (mode < 0 || mode >= MODE_COUNT || !MODES[mode])
        return -1;
    snprintf(want, sizeof(want), "mode_%s", MODES[mode]->name);
    for (int i = 0; i < LAB_COUNT; i++)
        if (!strcmp(LAB_IDS[i], want))
            return i;
    return -1;
}

int lab_step(Game *g, int edges)
{
    int s = g->lab_sel, was = s;
    if (edges & IN_LEFT)
        s = (s + MODE_COUNT - 1) % MODE_COUNT;
    if (edges & IN_RIGHT)
        s = (s + 1) % MODE_COUNT;
    if ((edges & IN_UP) && s >= LAB_COLS)
        s -= LAB_COLS;
    if ((edges & IN_DOWN) && s + LAB_COLS < MODE_COUNT)
        s += LAB_COLS;
    if (s != was)
        synth_sfx(SND_MOVE);
    g->lab_sel = s;
    if (edges & IN_CIRCLE) {
        synth_sfx(SND_BACK);
        return LAB_BACK;
    }
    if ((edges & IN_CROSS) && lab_level(s) >= 0) {
        synth_sfx(SND_SELECT);
        return s;
    }
    return -1;
}

void lab_draw(const Game *g)
{
    run_draw_backdrop(LAB_COLORS, (float)g->frame * 3.0f, 0);
    draw_text_glow_centered(40, 9, "MODE LAB", COL_WHITE, LAB_COLORS.line);
    const char *sub = "TRY EVERY MODE. NOTHING HERE COUNTS, SO PLAY AS MUCH AS YOU LIKE";
    draw_text((SCREEN_W - text_width(sub, 3)) / 2, 124, 3, sub, RGB(170, 220, 230));
    int tw = 280, th = 136, gap = 18, x0 = (SCREEN_W - LAB_COLS * tw - (LAB_COLS - 1) * gap) / 2, y0 = 170;
    for (int m = 0; m < MODE_COUNT; m++) {
        int x = x0 + (m % LAB_COLS) * (tw + gap), y = y0 + (m / LAB_COLS) * (th + gap), on = m == g->lab_sel;
        if (on)
            draw_frame(x - 10, y - 10, tw + 20, th + 20, 6, (g->frame / 10) % 2 ? COL_CUBE : RGB(200, 255, 170));
        draw_rect(x, y, tw, th, on ? RGB(24, 60, 74) : RGB(8, 22, 32));
        if (!MODES[m])
            continue;
        draw_mode_icon(m, (float)(x + tw / 2), (float)(y + 52), on ? 1.0f : 0.85f, on ? g->frame : 0);
        const char *label = MODES[m]->label;
        int s = ui_fit_scale(label, tw - 20, 4);
        draw_text_glow(x + (tw - text_width(label, s)) / 2, y + th - 8 - 7 * s, s, label, on ? COL_WHITE : COL_DIM,
                       on ? MODES[m]->color : RGB(0, 0, 0));
    }
    const char *how = lab_how(g->lab_sel);
    int hs = ui_fit_scale(how, SCREEN_W - 120, 5);
    draw_text_glow_centered(y0 + 5 * (th + gap) + 14, hs, how, COL_YELLOW, RGB(160, 100, 0));
    draw_text_glow_centered(SCREEN_H - 56, 4, "D-PAD PICK   X TRY IT   CIRCLE BACK", RGB(220, 220, 255), RGB(90, 90, 200));
}
