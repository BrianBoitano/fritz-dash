// Fritz Dash - the icon maker and his icons.
#include <stdio.h>
#include <string.h>

#include "draw.h"
#include "icons.h"
#include "input.h"
#include "platform.h"
#include "play.h"
#include "synth.h"

#define COL_WHITE  RGB(255, 255, 255)
#define COL_YELLOW RGB(255, 235, 40)
#define COL_CYAN   RGB(0, 230, 255)
#define COL_GREEN  RGB(90, 255, 40)
#define COL_PINK   RGB(255, 50, 200)
#define COL_DIM    RGB(150, 150, 190)

#define GRID_X 600
#define GRID_Y 170
#define CELL   88

static uint8_t px[NUM_ICONS][ICON_N * ICON_N];

// Icons to learn from, one idea each (0 cube colour, 1 colour 2, 2 white, 3 black).
static const struct { const char *name, *caption, *rows; } EXAMPLES[NUM_ICON_EXAMPLES] = {
    { "MIRROR", "MIRROR: THE RIGHT SIDE COPIES THE LEFT",
      "00000000" "11033011" "12133121" "11133111" "01133110" "12133121" "11033011" "00000000" },
    { "OUTLINE", "OUTLINE: BLACK ROUND THE EDGE MAKES IT POP",
      "03300330" "31133113" "31211113" "31111113" "03111130" "00311300" "00033000" "00000000" },
    { "SHADING", "SHADING: A LIGHT SIDE AND A DARK SIDE",
      "00111100" "01221110" "12211113" "12111113" "11111133" "11111333" "01133330" "00333300" },
    { "FACE", "A FACE: EYES AND A MOUTH SHOW A FEELING",
      "00000000" "02200220" "02300230" "00000000" "30000003" "03111130" "00333300" "00000000" },
    { "ROCKET", "A TINY ROCKET: A FEW DOTS MAKE A SHIP",
      "00022000" "00222200" "00233200" "00233200" "00222200" "01222210" "11011011" "00011000" },
};
static uint8_t example_px[NUM_ICON_EXAMPLES][ICON_N * ICON_N];

const uint8_t *icon_example(int e)
{
    e = e < 0 || e >= NUM_ICON_EXAMPLES ? 0 : e;
    for (int i = 0; i < ICON_N * ICON_N; i++)
        example_px[e][i] = (uint8_t)(EXAMPLES[e].rows[i] - '0');
    return example_px[e];
}

const char *icon_example_name(int e)
{
    return EXAMPLES[e < 0 || e >= NUM_ICON_EXAMPLES ? 0 : e].name;
}

const char *icon_example_caption(int e)
{
    return EXAMPLES[e < 0 || e >= NUM_ICON_EXAMPLES ? 0 : e].caption;
}

#define ALL_ICONS (NUM_ICONS + NUM_ICON_EXAMPLES)

// What the grid shows: his icon, or an example.
static const uint8_t *shown(const IconMaker *m)
{
    return m->icon < NUM_ICONS ? px[m->icon] : icon_example(m->icon - NUM_ICONS);
}
static const char *const PEN_NAMES[PX_COLOURS] = { "CUBE COLOUR", "COLOUR 2", "WHITE", "BLACK" };

void icons_load(void)
{
    char buf[1024], key[8];
    memset(px, 0, sizeof(px));
    int n = plat_read_file(ICONS_FILE, buf, (int)sizeof(buf) - 1);
    buf[n > 0 ? n : 0] = 0;
    for (int k = 0; k < NUM_ICONS; k++) {
        snprintf(key, sizeof(key), "icon%d ", k + 1);
        const char *p = strstr(buf, key);
        if (!p)
            continue;
        p += strlen(key);
        for (int i = 0; i < ICON_N * ICON_N && p[i] >= '0' && p[i] <= '3'; i++)
            px[k][i] = (uint8_t)(p[i] - '0');
    }
}

int icons_save(void)
{
    char buf[1024];
    int n = 0;
    for (int k = 0; k < NUM_ICONS; k++) {
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "icon%d ", k + 1);
        for (int i = 0; i < ICON_N * ICON_N; i++)
            buf[n++] = (char)('0' + px[k][i]);
        buf[n++] = '\n';
    }
    return plat_write_file(ICONS_FILE, buf, n);
}

int icon_empty(int k)
{
    if (k < 0 || k >= NUM_ICONS)
        return 1;
    for (int i = 0; i < ICON_N * ICON_N; i++)
        if (px[k][i])
            return 0;
    return 1;
}

const uint8_t *icon_pixels(int k)
{
    return px[k < 0 || k >= NUM_ICONS ? 0 : k];
}

static void toast(IconMaker *m, const char *s)
{
    snprintf(m->msg, sizeof(m->msg), "%s", s);
    m->msg_frames = 100;
}

void iconmaker_open(IconMaker *m, int icon)
{
    memset(m, 0, sizeof(*m));
    m->icon = icon < 0 || icon >= NUM_ICONS ? 0 : icon;
    m->cx = m->cy = 3;
    m->pen = PX_WHITE;
    m->hold_dir = -1;
}

static void push_undo(IconMaker *m)
{
    memcpy(m->undo[m->undo_head], px[m->icon], ICON_N * ICON_N);
    m->undo_head = (m->undo_head + 1) % ICON_UNDO;
    if (m->undo_count < ICON_UNDO)
        m->undo_count++;
}

static void switch_icon(IconMaker *m, int icon)
{
    icons_save();
    m->icon = icon;
    m->undo_count = 0;
    m->clear_armed = 0;
    synth_sfx(SND_MOVE);
}

// COPY TO MY SLOT: the example into his first empty icon, which opens to
// change (Triangle undoes the copy). With all six drawn nothing is touched.
static void copy_example(IconMaker *m)
{
    int k = 0;
    while (k < NUM_ICONS && !icon_empty(k))
        k++;
    if (k == NUM_ICONS) {
        toast(m, "ALL 6 ICONS ARE DRAWN - CLEAR ONE FIRST");
        synth_sfx(SND_BACK);
        return;
    }
    const uint8_t *e = icon_example(m->icon - NUM_ICONS);
    m->icon = k;
    m->undo_count = 0;
    m->clear_armed = 0;
    m->x_lock = 1;
    push_undo(m);
    memcpy(px[k], e, ICON_N * ICON_N);
    icons_save();
    char msg[48];
    snprintf(msg, sizeof(msg), "COPIED TO MY ICON %d - NOW CHANGE IT!", k + 1);
    toast(m, msg);
    synth_sfx(SND_SELECT);
}

int iconmaker_step(IconMaker *m, int input, int edges)
{
    if (m->msg_frames > 0)
        m->msg_frames--;
    if (edges & IN_CIRCLE) {
        icons_save();
        return ICR_BACK;
    }
    if (!(input & IN_CROSS))
        m->x_lock = 0;
    if (edges & (IN_L2 | IN_R2)) {
        switch_icon(m, (m->icon + ((edges & IN_R2) ? 1 : ALL_ICONS - 1)) % ALL_ICONS);
        return ICR_NONE;
    }
    if (m->icon >= NUM_ICONS) {
        // An example: look, and X copies it; nothing changes it.
        if (edges & IN_CROSS)
            copy_example(m);
        else if (edges & (IN_SQUARE | IN_TOUCH | IN_TRIANGLE | IN_START))
            toast(m, "AN EXAMPLE CAN'T BE CHANGED - X COPIES IT");
        return ICR_NONE;
    }
    if (edges & IN_START) {
        if (icon_empty(m->icon)) {
            toast(m, "DRAW SOMETHING FIRST!");
            return ICR_NONE;
        }
        icons_save();
        toast(m, "YOU ARE WEARING IT!");
        return ICR_WEAR;
    }
    if (edges & (IN_L1 | IN_R1)) {
        m->pen = (m->pen + ((edges & IN_R1) ? 1 : PX_COLOURS - 1)) % PX_COLOURS;
        synth_sfx(SND_MOVE);
    }
    if (edges & IN_TOUCH) {
        if (!m->clear_armed) {
            m->clear_armed = 1;
            toast(m, "TOUCHPAD AGAIN CLEARS THIS ICON");
        } else {
            push_undo(m);
            memset(px[m->icon], 0, ICON_N * ICON_N);
            m->clear_armed = 0;
            toast(m, "CLEARED - TRIANGLE UNDOES IT");
            synth_sfx(SND_DELETE);
        }
    }
    if (edges & IN_TRIANGLE) {
        if (!m->undo_count) {
            toast(m, "NOTHING TO UNDO");
        } else {
            m->undo_head = (m->undo_head + ICON_UNDO - 1) % ICON_UNDO;
            m->undo_count--;
            memcpy(px[m->icon], m->undo[m->undo_head], ICON_N * ICON_N);
            toast(m, "UNDO");
        }
    }
    // Moving: a press moves at once, holding repeats; X or Square held paints as it goes.
    const int dirs = IN_LEFT | IN_RIGHT | IN_UP | IN_DOWN;
    int dir = input & dirs, move = 0;
    if (dir != m->hold_dir) {
        m->hold_dir = dir;
        m->hold_timer = 14;
        move = dir != 0;
    } else if (dir && --m->hold_timer <= 0) {
        m->hold_timer = 5;
        move = 1;
    }
    if (move) {
        if ((dir & IN_LEFT) && m->cx > 0) m->cx--;
        if ((dir & IN_RIGHT) && m->cx < ICON_N - 1) m->cx++;
        if ((dir & IN_UP) && m->cy > 0) m->cy--;
        if ((dir & IN_DOWN) && m->cy < ICON_N - 1) m->cy++;
    }
    uint8_t *cell = &px[m->icon][m->cy * ICON_N + m->cx];
    if (m->x_lock)
        return ICR_NONE;
    if (edges & (IN_CROSS | IN_SQUARE)) {
        push_undo(m);
        m->clear_armed = 0;
        synth_sfx((edges & IN_CROSS) ? SND_PLACE : SND_DELETE);
    }
    if (input & IN_CROSS)
        *cell = (uint8_t)m->pen;
    else if (input & IN_SQUARE)
        *cell = PX_CUBE;
    return ICR_NONE;
}

// ---------------------------------------------------------------------------
// Drawing

static color_t pen_color(int pen, const Look *l)
{
    switch (pen) {
    case PX_COL2: return LOOK_COLORS[l->v[LOOK_COL2] % LOOK_COUNT[LOOK_COL2]];
    case PX_WHITE: return COL_WHITE;
    case PX_BLACK: return RGB(0, 0, 0);
    }
    return LOOK_COLORS[l->v[LOOK_COL1] % LOOK_COUNT[LOOK_COL1]];
}

void iconmaker_draw(const IconMaker *m, const Look *l, int frame)
{
    draw_set_offset(0, 0);
    run_draw_backdrop(LEVEL_PALETTES[11], (float)frame * 2.0f, 0);
    draw_text_glow_centered(30, 8, "ICON MAKER", COL_WHITE, COL_PINK);

    // His six icons, then the examples, two to a row down the left; the one
    // on the grid is boxed.
    draw_text_glow(50, 112, 3, "MY ICONS", COL_WHITE, COL_PINK);
    draw_text_glow(50, 486, 3, "EXAMPLES TO LEARN FROM", COL_YELLOW, RGB(200, 120, 0));
    for (int k = 0; k < ALL_ICONS; k++) {
        int ex = k >= NUM_ICONS, j = ex ? k - NUM_ICONS : k;
        int x = 50 + (j % 2) * 265, y = (ex ? 520 : 146) + (j / 2) * 112, on = k == m->icon;
        ui_panel(x, y, 245, 96, on ? COL_GREEN : ex ? RGB(200, 140, 30) : RGB(80, 60, 140));
        const uint8_t *p = ex ? icon_example(j) : px[k];
        int s = 9, x0 = x + 12, y0 = y + 12;
        draw_rect(x0 - 2, y0 - 2, ICON_N * s + 4, ICON_N * s + 4, pen_color(PX_CUBE, l));
        for (int i = 0; i < ICON_N * ICON_N; i++)
            if (p[i])
                draw_rect(x0 + (i % ICON_N) * s, y0 + (i / ICON_N) * s, s, s, pen_color(p[i], l));
        char lab[16];
        snprintf(lab, sizeof(lab), "%d", k + 1);
        if (ex)
            draw_text(x + 100, y + 38, ui_fit_scale(icon_example_name(j), 136, 3), icon_example_name(j),
                      on ? COL_WHITE : COL_DIM);
        else
            draw_text_glow(x + 140, y + 20, 8, lab, on ? COL_WHITE : COL_DIM, on ? COL_PINK : RGB(40, 30, 70));
    }
    draw_text(60, 870, 4, "L2 R2 ICON", COL_YELLOW);

    // The big grid.
    int gw = ICON_N * CELL;
    draw_rect(GRID_X - 12, GRID_Y - 12, gw + 24, gw + 24, COL_PINK);
    draw_rect(GRID_X - 6, GRID_Y - 6, gw + 12, gw + 12, RGB(10, 6, 26));
    const uint8_t *cur = shown(m);
    int ex = m->icon >= NUM_ICONS;
    for (int i = 0; i < ICON_N * ICON_N; i++) {
        int x = GRID_X + (i % ICON_N) * CELL, y = GRID_Y + (i / ICON_N) * CELL;
        draw_rect(x, y, CELL, CELL, RGB(30, 26, 50));
        draw_rect(x + 2, y + 2, CELL - 4, CELL - 4, pen_color(cur[i], l));
    }
    int cx = GRID_X + m->cx * CELL, cy = GRID_Y + m->cy * CELL;
    if (!ex)
        draw_frame(cx - 6, cy - 6, CELL + 12, CELL + 12, 6, (frame / 6) % 2 ? COL_YELLOW : COL_WHITE);

    // Colours to paint with, and the cube wearing it.
    ui_panel(1400, 150, 460, 480, COL_CYAN);
    draw_text_glow(1430, 172, 4, "PAINT WITH", COL_WHITE, COL_CYAN);
    for (int p = 0; p < PX_COLOURS; p++) {
        int y = 236 + p * 96, on = p == m->pen;
        if (on)
            draw_frame(1420, y - 10, 420, 86, 5, COL_GREEN);
        draw_rect(1436, y, 66, 66, RGB(255, 255, 255));
        draw_rect(1440, y + 4, 58, 58, pen_color(p, l));
        draw_text(1520, y + 22, 4, PEN_NAMES[p], on ? COL_WHITE : COL_DIM);
    }
    draw_text(1430, 610 - 6, 3, "< L1   R1 >", COL_YELLOW);
    if (ex) {
        static const char *const COPY[3] = { "PRESS X TO", "COPY IT INTO", "YOUR ICONS" };
        ui_panel(1400, 660, 460, 200, COL_PINK);
        for (int i = 0; i < 3; i++)
            draw_text_glow(1630 - text_width(COPY[i], 5) / 2, 684 + i * 60, 5, COPY[i], COL_WHITE, COL_PINK);
    } else {
        ui_panel(1400, 660, 460, 300, COL_PINK);
        Look wear = *l;
        wear.v[LOOK_FACE] = FACE_ICON0 + m->icon;
        wear.stock = 0;
        look_set(&wear);
        int t = frame % 40;
        run_draw_cube(1630.0f, 880.0f - (float)(t * (40 - t)) * 0.3f, (float)t * (3.14159265f / 40.0f));
        look_set(l);
    }

    if (m->msg_frames > 0)
        draw_text_glow_centered(GRID_Y + gw + 20, 5, m->msg, COL_YELLOW, COL_PINK);
    else if (ex)
        draw_text_glow_centered(GRID_Y + gw + 20, ui_fit_scale(icon_example_caption(m->icon - NUM_ICONS), SCREEN_W - 120, 5),
                                icon_example_caption(m->icon - NUM_ICONS), COL_WHITE, RGB(200, 140, 30));
    if (ex) {
        draw_text_glow_centered(SCREEN_H - 70, 4, "L2 R2 ICON   X COPY TO MY ICONS   CIRCLE GO BACK", RGB(220, 220, 255),
                                RGB(90, 90, 200));
        return;
    }
    draw_text_glow_centered(SCREEN_H - 90, 3, "D-PAD MOVE   X PAINT (HOLD TO KEEP PAINTING)   SQUARE RUB OUT   L1 R1 COLOUR",
                            RGB(220, 220, 255), RGB(90, 90, 200));
    draw_text_glow_centered(SCREEN_H - 50, 3, "TRIANGLE UNDO   OPTIONS WEAR IT   TOUCHPAD CLEAR   CIRCLE SAVE AND GO BACK",
                            RGB(220, 220, 255), RGB(90, 90, 200));
}
