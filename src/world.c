// Fritz Dash - FRITZ WORLD.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "draw.h"
#include "game.h"
#include "platform.h"
#include "rate.h"
#include "synth.h"
#include "world.h"

#define COL_WHITE  RGB(255, 255, 255)
#define COL_YELLOW RGB(255, 235, 40)
#define COL_GREEN  RGB(90, 255, 40)
#define COL_PINK   RGB(255, 50, 200)
#define COL_CYAN   RGB(0, 230, 255)
#define COL_DIM    RGB(150, 150, 190)
#define COL_RED    RGB(255, 90, 90)

void world_load(World *w)
{
    char buf[2048];
    int sel = w->sel;
    memset(w, 0, sizeof(*w));
    w->sel = sel;
    int n = plat_read_file(WORLD_FILE, buf, (int)sizeof(buf) - 1);
    buf[n > 0 ? n : 0] = 0;
    for (char *line = buf; line && *line;) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = 0;
        if (!strncmp(line, "reached ", 8))
            w->reached = atoi(line + 8);
        else if (!strncmp(line, "level ", 6) && w->n < WORLD_MAX)
            snprintf(w->files[w->n++], WORLD_NAME, "%s", line + 6);
        line = nl ? nl + 1 : NULL;
    }
    w->reached = w->reached < 0 ? 0 : w->reached > w->n ? w->n : w->reached;
}

void world_save(const World *w)
{
    char buf[2048];
    int n = snprintf(buf, sizeof(buf), "reached %d\n", w->reached);
    for (int i = 0; i < w->n; i++)
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "level %s\n", w->files[i]);
    plat_write_file(WORLD_FILE, buf, n);
}

void world_open(World *w)
{
    world_load(w);
    w->sel = w->reached < w->n ? w->reached : w->n > 0 ? w->n - 1 : 0;
    w->build = 0;
    w->del_armed = w->reset_armed = 0;
}

int world_beaten(World *w, int node)
{
    if (node != w->reached || node >= w->n)
        return 0;
    w->reached++;
    world_save(w);
    return w->reached == w->n ? 2 : 1;
}

static void toast(World *w, const char *m)
{
    snprintf(w->msg, sizeof(w->msg), "%s", m);
    w->msg_frames = 110;
}

// His levels: the editor's slot files in the level list.
static int is_his(const LevelInfo *li)
{
    return li->builtin < 0 && !strncmp(li->file, "slot", 4);
}

static const LevelInfo *info_of(const Game *g, const char *file)
{
    for (int i = 0; i < g->nlevels; i++)
        if (g->levels[i].builtin < 0 && !strcmp(g->levels[i].file, file))
            return &g->levels[i];
    return NULL;
}

// The next of his levels after `file` (dir 1) or before it (dir -1).
static const char *next_level(const Game *g, const char *file, int dir)
{
    int at = -1, n = g->nlevels;
    for (int i = 0; i < n; i++)
        if (!strcmp(g->levels[i].file, file))
            at = i;
    for (int k = 1; k <= n; k++) {
        int i = ((at < 0 ? 0 : at) + dir * k + n * 2) % n;
        if (is_his(&g->levels[i]))
            return g->levels[i].file;
    }
    return NULL;
}

static int in_world(const World *w, const char *file)
{
    for (int i = 0; i < w->n; i++)
        if (!strcmp(w->files[i], file))
            return 1;
    return 0;
}

static int step_build(Game *g, int edges)
{
    World *w = &g->world;
    int last = w->n < WORLD_MAX ? w->n : w->n - 1;     // the "+" node after the others
    if (edges & (IN_CIRCLE | IN_TRIANGLE)) {
        world_save(w);
        w->build = 0;
        w->sel = w->reached < w->n ? w->reached : w->n > 0 ? w->n - 1 : 0;
        synth_sfx(SND_SELECT);
        toast(w, "SAVED! NOW PLAY IT IN ORDER");
        return WR_NONE;
    }
    if (edges & (IN_LEFT | IN_RIGHT)) {
        w->sel = (w->sel + ((edges & IN_RIGHT) ? 1 : last)) % (last + 1);
        w->del_armed = 0;
        synth_sfx(SND_MOVE);
    }
    if (w->sel < w->n && (edges & (IN_UP | IN_DOWN))) {
        const char *f = next_level(g, w->files[w->sel], (edges & IN_UP) ? 1 : -1);
        if (f)
            snprintf(w->files[w->sel], WORLD_NAME, "%s", f);
        synth_sfx(SND_MOVE);
    }
    if (w->sel < w->n && (edges & (IN_L1 | IN_R1))) {
        int to = w->sel + ((edges & IN_R1) ? 1 : -1);
        if (to >= 0 && to < w->n) {
            char t[WORLD_NAME];
            memcpy(t, w->files[to], WORLD_NAME);
            memcpy(w->files[to], w->files[w->sel], WORLD_NAME);
            memcpy(w->files[w->sel], t, WORLD_NAME);
            w->sel = to;
            synth_sfx(SND_PLACE);
        }
    }
    if ((edges & IN_CROSS) && w->sel == w->n && w->n < WORLD_MAX) {
        const char *f = NULL;
        for (int i = 0; i < g->nlevels && !f; i++)
            if (is_his(&g->levels[i]) && !in_world(w, g->levels[i].file))
                f = g->levels[i].file;
        if (!f)
            f = next_level(g, w->n ? w->files[w->n - 1] : "", 1);
        if (!f) {
            toast(w, "MAKE A LEVEL IN THE EDITOR FIRST");
            return WR_NONE;
        }
        snprintf(w->files[w->n++], WORLD_NAME, "%s", f);
        synth_sfx(SND_PLACE);
        toast(w, "UP DOWN CHANGES WHICH LEVEL IT IS");
    }
    if ((edges & IN_SQUARE) && w->sel < w->n) {
        if (!w->del_armed) {
            w->del_armed = 1;
            toast(w, "SQUARE AGAIN TAKES IT OFF THE PATH");
        } else {
            memmove(w->files[w->sel], w->files[w->sel + 1], (size_t)(w->n - w->sel - 1) * WORLD_NAME);
            w->n--;
            if (w->reached > w->n)
                w->reached = w->n;
            w->del_armed = 0;
            synth_sfx(SND_DELETE);
        }
    }
    return WR_NONE;
}

int world_step(Game *g, int input, int edges)
{
    World *w = &g->world;
    (void)input;
    if (w->msg_frames > 0)
        w->msg_frames--;
    if (w->build)
        return step_build(g, edges);
    if (edges & IN_CIRCLE)
        return WR_BACK;
    if (edges & IN_TRIANGLE) {
        w->build = 1;
        w->sel = w->n;
        w->del_armed = 0;
        synth_sfx(SND_SELECT);
        return WR_NONE;
    }
    if (edges & IN_START) {
        if (!w->reset_armed) {
            w->reset_armed = 1;
            toast(w, "OPTIONS AGAIN STARTS OVER FOR A NEW PLAYER");
        } else {
            w->reached = 0;
            w->sel = 0;
            w->reset_armed = 0;
            world_save(w);
            toast(w, "STARTED OVER!");
        }
        return WR_NONE;
    }
    int open = w->reached < w->n ? w->reached : w->n - 1;
    if (w->n == 0)
        return WR_NONE;
    if (edges & (IN_LEFT | IN_UP)) {
        if (w->sel > 0) {
            w->sel--;
            synth_sfx(SND_MOVE);
        }
    }
    if (edges & (IN_RIGHT | IN_DOWN)) {
        if (w->sel < open) {
            w->sel++;
            synth_sfx(SND_MOVE);
        } else if (w->sel < w->n - 1) {
            toast(w, "BEAT THIS ONE TO OPEN THE NEXT!");
            synth_sfx(SND_BACK);
        }
    }
    if (edges & (IN_CROSS | IN_SQUARE)) {
        if (!info_of(g, w->files[w->sel])) {
            toast(w, "THIS LEVEL IS GONE - TRIANGLE FIXES THE PATH");
            synth_sfx(SND_BACK);
            return WR_NONE;
        }
        synth_sfx(SND_SELECT);
        return (edges & IN_CROSS) ? WR_PLAY : WR_PRACTICE;
    }
    return WR_NONE;
}

// ---------------------------------------------------------------------------
// Drawing: the nodes snake along two rows of six.

static void node_pos(int i, int *x, int *y)
{
    int row = i / 6, col = i % 6;
    *x = 260 + (row % 2 ? 5 - col : col) * 280;
    *y = 400 + row * 270;
}

static void draw_lock(int cx, int cy)
{
    draw_rect(cx - 22, cy - 6, 44, 34, RGB(90, 90, 110));
    draw_frame(cx - 14, cy - 30, 28, 28, 7, RGB(90, 90, 110));
}

void world_draw(const Game *g)
{
    const World *w = &g->world;
    run_draw_backdrop(LEVEL_PALETTES[9], (float)g->frame * 1.5f, 0);
    draw_text_glow_centered(36, 9, "FRITZ WORLD", COL_WHITE, COL_GREEN);
    draw_text_glow_centered(140, 4, w->build ? "BUILD THE PATH: PUT YOUR LEVELS IN ORDER" : "PLAY THE LEVELS IN ORDER",
                            w->build ? COL_YELLOW : COL_WHITE, w->build ? RGB(200, 120, 0) : RGB(30, 140, 10));
    int count = w->build && w->n < WORLD_MAX ? w->n + 1 : w->n;
    if (count == 0) {
        draw_text_glow_centered(440, 6, "FRITZ WORLD IS EMPTY", COL_WHITE, COL_PINK);
        draw_text_glow_centered(540, 5, "PRESS TRIANGLE TO PUT YOUR LEVELS IN IT", COL_YELLOW, RGB(200, 120, 0));
    }
    for (int i = 0; i + 1 < count; i++) {
        int x0, y0, x1, y1;
        node_pos(i, &x0, &y0);
        node_pos(i + 1, &x1, &y1);
        color_t c = !w->build && i < w->reached ? RGB(255, 210, 60) : RGB(70, 90, 70);
        if (y0 == y1)
            draw_rect(x0 < x1 ? x0 : x1, y0 - 8, x0 < x1 ? x1 - x0 : x0 - x1, 16, c);
        else
            draw_rect(x0 - 8, y0, 16, y1 - y0, c);
    }
    for (int i = 0; i < count; i++) {
        int x, y, on = i == w->sel;
        node_pos(i, &x, &y);
        char num[8];
        if (i == w->n) {            // the "+" to add one
            draw_circle(x, y, 58, on ? COL_GREEN : RGB(60, 80, 60));
            draw_circle(x, y, 50, RGB(10, 30, 10));
            draw_text(x - text_width("+", 9) / 2, y - 30, 9, "+", on ? COL_WHITE : COL_DIM);
            draw_text(x - text_width("ADD", 3) / 2, y + 74, 3, "ADD", COL_DIM);
            continue;
        }
        const LevelInfo *li = info_of(g, w->files[i]);
        int beaten = i < w->reached, locked = !w->build && i > w->reached;
        color_t ring = !li ? COL_RED : beaten ? RGB(255, 200, 0) : locked ? RGB(70, 70, 90) : COL_GREEN;
        if (on)
            draw_circle(x, y, 74 + (g->frame / 6) % 2 * 4, COL_WHITE);
        draw_circle(x, y, 64, ring);
        draw_circle(x, y, 54, locked ? RGB(30, 30, 40) : RGB(12, 8, 30));
        if (locked) {
            draw_lock(x, y);
        } else if (beaten) {
            ui_star((float)x, (float)y, 40, RGB(255, 210, 40));
        } else {
            snprintf(num, sizeof(num), "%d", i + 1);
            draw_text(x - text_width(num, 7) / 2, y - 24, 7, num, COL_WHITE);
        }
        const char *name = li ? li->name : "MISSING";
        int ns = ui_fit_scale(name, 260, 4);
        draw_text(x - text_width(name, ns) / 2, y + 78, ns, name, locked ? COL_DIM : COL_WHITE);
    }
    // His cube waits on the chosen node.
    if (count > 0 && w->sel < count) {
        int x, y, t = g->frame % 40;
        node_pos(w->sel, &x, &y);
        run_draw_cube((float)x, (float)(y - 112) - (float)(t * (40 - t)) * 0.15f, 0);
    }
    // What the chosen node is.
    int py = 860;
    if (count > 0)
        ui_panel(160, py, SCREEN_W - 320, 120, w->build ? COL_YELLOW : COL_GREEN);
    char buf[96];
    if (w->sel < w->n) {
        const LevelInfo *li = info_of(g, w->files[w->sel]);
        if (li) {
            snprintf(buf, sizeof(buf), "%d. %s", w->sel + 1, li->name);
            draw_text_glow(190, py + 22, ui_fit_scale(buf, 900, 6), buf, COL_WHITE, COL_GREEN);
            snprintf(buf, sizeof(buf), "%s%s   BEST %d%%", li->rating >= 0 ? RATING_NAMES[li->rating] : "",
                     li->verified ? "   VERIFIED" : "", li->best);
            draw_text(1120, py + 44, 4, buf, COL_YELLOW);
        } else {
            draw_text_glow(190, py + 30, 5, "THIS LEVEL IS GONE", COL_RED, RGB(100, 20, 20));
        }
    } else if (w->build) {
        draw_text_glow(190, py + 30, 5, "X ADDS ONE OF YOUR LEVELS HERE", COL_WHITE, COL_GREEN);
    }
    if (w->msg_frames > 0)
        draw_text_glow_centered(805, 5, w->msg, COL_YELLOW, COL_PINK);
    draw_text_glow_centered(SCREEN_H - 70, 3, w->build ?
                            "LEFT RIGHT PICK   UP DOWN CHANGE LEVEL   L1 R1 MOVE IT   X ADD   SQUARE TAKE OFF   TRIANGLE DONE" :
                            "LEFT RIGHT PICK   X PLAY   SQUARE PRACTICE   TRIANGLE BUILD THE PATH   OPTIONS START OVER   CIRCLE BACK",
                            RGB(220, 220, 255), RGB(90, 90, 200));
}
