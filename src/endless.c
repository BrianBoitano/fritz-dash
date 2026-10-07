// Fritz Dash - Endless mode: random obstacle patterns, rising speed, best score.
#include "draw.h"
#include "endless.h"
#include "input.h"

// Physics, in pixels per frame at 60 fps.
#define GRAVITY     1.5f
#define JUMP_V      (-26.0f)
#define MAX_FALL    30.0f
#define ROT_STEP    0.0905f   // half a turn per jump, like the real thing
#define LAND_TOL    14.0f     // forgiving: catching a block's top corner still lands

#define SPEED_BASE  10.5f
#define SPEED_INC   0.07f
#define SPEED_MAX   17.5f

#define PI_F        3.14159265f
#define HALF_PI_F   1.57079633f

typedef struct {
    color_t top, bottom, ground, line;
} Palette;

static const Palette PALETTES[] = {
    { RGB(10, 4, 40),  RGB(62, 10, 92),  RGB(20, 6, 52),  RGB(0, 240, 255) },
    { RGB(2, 14, 48),  RGB(10, 62, 124), RGB(4, 20, 62),  RGB(140, 255, 40) },
    { RGB(40, 2, 40),  RGB(112, 10, 92), RGB(42, 4, 42),  RGB(255, 230, 0) },
    { RGB(2, 30, 22),  RGB(10, 92, 62),  RGB(4, 34, 26),  RGB(255, 70, 210) },
    { RGB(40, 6, 2),   RGB(122, 32, 10), RGB(42, 10, 4),  RGB(0, 240, 255) },
};
#define NPAL ((int)(sizeof(PALETTES) / sizeof(PALETTES[0])))

static const char *CHEERS[] = {
    "NICE!", "GO FRITZ!", "AWESOME!", "SUPER!", "AMAZING!", "WOW!", "LEGEND!", "UNSTOPPABLE!",
};
#define NCHEERS ((int)(sizeof(CHEERS) / sizeof(CHEERS[0])))

#define COL_CUBE      RGB(90, 255, 40)
#define COL_CUBE_CORE RGB(0, 230, 255)
#define COL_PINK      RGB(255, 50, 200)
#define COL_YELLOW    RGB(255, 235, 40)
#define COL_WHITE     RGB(255, 255, 255)

static uint32_t rnd(Endless *g)
{
    // xorshift32
    uint32_t x = g->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g->rng = x;
    return x;
}

static int rnd_range(Endless *g, int lo, int hi)
{
    return lo + (int)(rnd(g) % (uint32_t)(hi - lo + 1));
}

static void add_obs(Endless *g, int type, float x, int level)
{
    if (g->nobs >= MAX_OBS)
        return;
    Obstacle *o = &g->obs[g->nobs++];
    o->type = (uint8_t)type;
    o->x = x;
    o->w = TILE;
    o->h = TILE;
    o->y = GROUND_Y - TILE * (level + 1);
    o->counted = 0;
}

enum { P_SPIKE1, P_BLOCK1, P_SPIKE2, P_BLOCK3, P_SPIKE3, P_BLOCK_SPIKE, P_STAIRS, P_COUNT };

// Adds one obstacle pattern starting at screen x. Returns its width in pixels.
static float spawn_pattern(Endless *g, float x)
{
    int s = g->score;
    int w[P_COUNT] = {
        [P_SPIKE1] = 10,
        [P_BLOCK1] = 6,
        [P_SPIKE2] = s >= 4 ? 7 : 0,
        [P_BLOCK3] = s >= 8 ? 4 : 0,
        [P_SPIKE3] = s >= 25 ? 4 : 0,      // tightest timing, so it comes later
        [P_BLOCK_SPIKE] = s >= 12 ? 4 : 0,
        [P_STAIRS] = s >= 32 ? 3 : 0,
    };
    int total = 0;
    for (int i = 0; i < P_COUNT; i++)
        total += w[i];
    int pick = rnd_range(g, 0, total - 1), p = 0;
    while (pick >= w[p]) {
        pick -= w[p];
        p++;
    }

    switch (p) {
    case P_SPIKE1:
        add_obs(g, OBS_SPIKE, x, 0);
        return TILE;
    case P_BLOCK1:
        add_obs(g, OBS_BLOCK, x, 0);
        return TILE;
    case P_SPIKE2:
        add_obs(g, OBS_SPIKE, x, 0);
        add_obs(g, OBS_SPIKE, x + TILE, 0);
        return 2 * TILE;
    case P_BLOCK3:
        for (int i = 0; i < 3; i++)
            add_obs(g, OBS_BLOCK, x + i * TILE, 0);
        return 3 * TILE;
    case P_SPIKE3:
        for (int i = 0; i < 3; i++)
            add_obs(g, OBS_SPIKE, x + i * TILE, 0);
        return 3 * TILE;
    case P_BLOCK_SPIKE:
        add_obs(g, OBS_BLOCK, x, 0);
        add_obs(g, OBS_SPIKE, x + TILE, 0);
        return 2 * TILE;
    default: // P_STAIRS: a block, then a two-high column
        add_obs(g, OBS_BLOCK, x, 0);
        add_obs(g, OBS_BLOCK, x + TILE, 0);
        add_obs(g, OBS_BLOCK, x + TILE, 1);
        return 2 * TILE;
    }
}

static float gap_px(Endless *g)
{
    // Measured in frames so a faster game never gives less time to land.
    int lo = g->score < 30 ? 50 : 46;
    int hi = g->score < 30 ? 86 : 74;
    return g->speed * (float)rnd_range(g, lo, hi);
}

static void start_run(Endless *g)
{
    g->state = EN_PLAY;
    g->state_frames = 0;
    g->py = GROUND_Y - PLAYER_S;
    g->vy = 0;
    g->rot = 0;
    g->on_ground = 1;
    g->speed = SPEED_BASE;
    g->gap_left = 1000;
    g->nobs = 0;
    g->score = 0;
    g->new_best = 0;
    g->shake = 0;
    g->cheer_frames = 0;
    g->pal_from = g->pal_to = 0;
    g->pal_t = 256;
    for (int i = 0; i < TRAIL_N; i++)
        g->trail_y[i] = g->py;
    for (int i = 0; i < MAX_PARTS; i++)
        g->parts[i].life = 0;
}

void endless_init(Endless *g, int best, uint32_t seed)
{
    char *p = (char *)g;
    for (unsigned i = 0; i < sizeof(*g); i++)
        p[i] = 0;
    g->best = best;
    g->rng = seed ? seed : 0x12345678u;
    start_run(g);
    g->prev_input = ~0; // the X that picked Endless must be released first
    g->jump_lock = 1;
}

static void go_title(Endless *g)
{
    start_run(g);
    g->exit = 1;
}

// Small reusable menu: up/down move with wrap-around, X picks. Returns the
// picked index, or -1.
static int menu_update(int *sel, int count, int edges)
{
    if (edges & IN_UP)
        *sel = (*sel + count - 1) % count;
    if (edges & IN_DOWN)
        *sel = (*sel + 1) % count;
    return (edges & IN_CROSS) ? *sel : -1;
}

enum { PAUSE_RESUME, PAUSE_RESTART, PAUSE_TITLE, PAUSE_COUNT };
static const char *PAUSE_ITEMS[PAUSE_COUNT] = { "RESUME", "RESTART", "TITLE" };

static void explode(Endless *g)
{
    float cx = PLAYER_X + PLAYER_S / 2.0f, cy = g->py + PLAYER_S / 2.0f;
    static const color_t cols[] = { COL_CUBE, COL_CUBE_CORE, COL_WHITE, COL_YELLOW };
    for (int i = 0; i < MAX_PARTS; i++) {
        Particle *p = &g->parts[i];
        p->x = cx;
        p->y = cy;
        p->vx = (float)rnd_range(g, -160, 160) / 10.0f;
        p->vy = (float)rnd_range(g, -220, 40) / 10.0f;
        p->max_life = p->life = rnd_range(g, 30, 60);
        p->size = rnd_range(g, 10, 26);
        p->color = cols[i % 4];
    }
}

static void die(Endless *g)
{
    g->state = EN_DYING;
    g->state_frames = 0;
    g->shake = 14;
    explode(g);
    if (g->score > g->best) {
        g->best = g->score;
        g->new_best = 1;
        g->save_pending = 1;
    }
}

static void play_step(Endless *g, int cross)
{
    g->speed = SPEED_BASE + SPEED_INC * (float)g->score;
    if (g->speed > SPEED_MAX)
        g->speed = SPEED_MAX;

    // Holding X keeps jumping: jump whenever we are standing on something.
    if (g->on_ground && cross && !g->jump_lock) {
        g->vy = JUMP_V;
        g->on_ground = 0;
    }
    g->vy += GRAVITY;
    if (g->vy > MAX_FALL)
        g->vy = MAX_FALL;
    float prev_bottom = g->py + PLAYER_S;
    g->py += g->vy;

    // Scroll the world.
    for (int i = 0; i < g->nobs; i++)
        g->obs[i].x -= g->speed;
    int n = 0;
    for (int i = 0; i < g->nobs; i++)
        if (g->obs[i].x + g->obs[i].w > -200)
            g->obs[n++] = g->obs[i];
    g->nobs = n;
    g->bg_scroll += g->speed * 0.25f;
    g->ground_scroll += g->speed;

    g->gap_left -= g->speed;
    if (g->gap_left <= 0) {
        float x = SCREEN_W + 40 + g->gap_left;
        g->gap_left += spawn_pattern(g, x) + gap_px(g);
    }

    // Collisions. Player hitbox is a little smaller than the drawn cube.
    float pl = PLAYER_X + 6, pr = PLAYER_X + PLAYER_S - 6;
    g->on_ground = 0;
    for (int i = 0; i < g->nobs; i++) {
        Obstacle *o = &g->obs[i];
        if (o->type != OBS_BLOCK || pr <= o->x || pl >= o->x + o->w)
            continue;
        float top = (float)o->y, bottom = (float)(o->y + o->h);
        if (g->py + PLAYER_S > top && g->py < bottom) {
            if (prev_bottom <= top + LAND_TOL && g->vy >= 0) {
                g->py = top - PLAYER_S;
                g->vy = 0;
                g->on_ground = 1;
            } else {
                die(g);
                return;
            }
        }
    }
    if (g->py + PLAYER_S >= GROUND_Y) {
        g->py = GROUND_Y - PLAYER_S;
        g->vy = 0;
        g->on_ground = 1;
    }
    for (int i = 0; i < g->nobs; i++) {
        Obstacle *o = &g->obs[i];
        if (o->type != OBS_SPIKE)
            continue;
        float sx0 = o->x + 26, sx1 = o->x + o->w - 26;
        float sy0 = (float)o->y + 30, sy1 = (float)(o->y + o->h);
        if (pr > sx0 && pl < sx1 && g->py + PLAYER_S - 2 > sy0 && g->py + 8 < sy1) {
            die(g);
            return;
        }
    }

    // Spin in the air, settle flat on landing.
    if (!g->on_ground) {
        g->rot += ROT_STEP;
    } else {
        float target = (float)(int)((g->rot + HALF_PI_F * 0.5f) / HALF_PI_F) * HALF_PI_F;
        float d = target - g->rot;
        if (d > 0.25f) d = 0.25f;
        if (d < -0.25f) d = -0.25f;
        g->rot += d;
    }
    if (g->rot > 2 * PI_F * 8)
        g->rot -= 2 * PI_F * 8;

    // Score: every obstacle that passes behind the player.
    for (int i = 0; i < g->nobs; i++) {
        Obstacle *o = &g->obs[i];
        if (!o->counted && o->x + o->w < PLAYER_X) {
            o->counted = 1;
            g->score++;
            if (g->score % 10 == 0) {
                g->cheer_frames = 70;
                g->cheer_idx = rnd_range(g, 0, NCHEERS - 1);
            }
            int pal = (g->score / 15) % NPAL;
            if (pal != g->pal_to) {
                g->pal_from = g->pal_to;
                g->pal_to = pal;
                g->pal_t = 0;
            }
        }
    }

    for (int i = TRAIL_N - 1; i > 0; i--)
        g->trail_y[i] = g->trail_y[i - 1];
    g->trail_y[0] = g->py;
}

void endless_step(Endless *g, int input)
{
    int cross = (input & IN_CROSS) != 0;
    int edges = input & ~g->prev_input;
    int start_pressed = (edges & (IN_CROSS | IN_START)) != 0;
    g->prev_input = input;
    if (!cross)
        g->jump_lock = 0;
    g->frame++;
    g->state_frames++;

    if (g->pal_t < 256)
        g->pal_t += 4;
    if (g->cheer_frames > 0)
        g->cheer_frames--;
    if (g->shake > 0)
        g->shake--;
    for (int i = 0; i < MAX_PARTS; i++) {
        Particle *p = &g->parts[i];
        if (p->life <= 0)
            continue;
        p->life--;
        p->x += p->vx;
        p->y += p->vy;
        p->vy += 0.8f;
    }

    switch (g->state) {
    case EN_PLAY:
        if (edges & IN_START) {
            g->state = EN_PAUSE;
            g->pause_sel = PAUSE_RESUME;
            break;
        }
        play_step(g, cross);
        break;
    case EN_PAUSE: {
        // The world is frozen; Options or Resume continues the same run.
        int pick = menu_update(&g->pause_sel, PAUSE_COUNT, edges);
        if ((edges & IN_START) || pick == PAUSE_RESUME) {
            g->state = EN_PLAY;
            g->jump_lock = cross;
        } else if (pick == PAUSE_RESTART) {
            start_run(g);
            g->jump_lock = cross;
        } else if (pick == PAUSE_TITLE) {
            go_title(g);
        }
        break;
    }
    case EN_DYING:
        if (g->state_frames >= 55) {
            g->state = EN_OVER;
            g->state_frames = 0;
        }
        break;
    case EN_OVER:
        // Needs a fresh press after a short pause, so a held X cannot skip it.
        if (g->state_frames > 30) {
            if (start_pressed)
                start_run(g);
            else if (edges & IN_CIRCLE)
                go_title(g);
        }
        break;
    }
}

// ---------------------------------------------------------------------------
// Rendering

static void itoa_simple(int v, char *out)
{
    char tmp[12];
    int n = 0;
    if (v < 0) v = 0;
    do {
        tmp[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < 11);
    for (int i = 0; i < n; i++)
        out[i] = tmp[n - 1 - i];
    out[n] = 0;
}

static void label_num(char *out, const char *label, int v)
{
    int i = 0;
    while (label[i]) {
        out[i] = label[i];
        i++;
    }
    itoa_simple(v, out + i);
}

static Palette current_palette(const Endless *g)
{
    const Palette *a = &PALETTES[g->pal_from], *b = &PALETTES[g->pal_to];
    Palette p;
    p.top = col_lerp(a->top, b->top, g->pal_t);
    p.bottom = col_lerp(a->bottom, b->bottom, g->pal_t);
    p.ground = col_lerp(a->ground, b->ground, g->pal_t);
    p.line = col_lerp(a->line, b->line, g->pal_t);
    return p;
}

static void draw_world_bg(const Endless *g, const Palette *pal)
{
    draw_gradient(pal->top, pal->bottom);

    // Parallax grid of big squares.
    color_t grid = col_lerp(pal->bottom, pal->line, 40);
    int off = (int)g->bg_scroll % 192;
    for (int x = -off; x < SCREEN_W; x += 192)
        draw_rect(x, 0, 3, GROUND_Y, grid);
    for (int y = 120; y < GROUND_Y; y += 192)
        draw_rect(0, y, SCREEN_W, 3, grid);

    // Ground with a neon edge and scrolling tiles.
    draw_rect(0, GROUND_Y, SCREEN_W, SCREEN_H - GROUND_Y, pal->ground);
    color_t tile = col_lerp(pal->ground, pal->line, 50);
    int goff = (int)g->ground_scroll % 160;
    for (int x = -goff; x < SCREEN_W; x += 160)
        draw_rect(x, GROUND_Y + 6, 4, SCREEN_H - GROUND_Y - 6, tile);
    draw_rect(0, GROUND_Y + 110, SCREEN_W, 4, tile);
    draw_rect(0, GROUND_Y - 6, SCREEN_W, 6, col_lerp(pal->bottom, pal->line, 90));
    draw_rect(0, GROUND_Y, SCREEN_W, 6, pal->line);
    draw_rect(0, GROUND_Y + 6, SCREEN_W, 4, col_lerp(pal->ground, pal->line, 110));
}

static void draw_spike(float x, int y, color_t glow)
{
    float b = (float)(y + TILE);
    float gx[3] = { x - 6, x + 40, x + 86 };
    float gy[3] = { b, (float)y - 14, b };
    draw_poly(3, gx, gy, col_scale(glow, 1, 2));
    float ox[3] = { x, x + 40, x + 80 };
    float oy[3] = { b, (float)y, b };
    draw_poly(3, ox, oy, COL_WHITE);
    float ix[3] = { x + 11, x + 40, x + 69 };
    float iy[3] = { b - 4, (float)y + 17, b - 4 };
    draw_poly(3, ix, iy, RGB(10, 4, 22));
}

static void draw_block(float fx, int y, color_t line)
{
    int x = (int)fx;
    draw_rect(x, y, TILE, TILE, line);
    draw_rect(x + 6, y + 6, TILE - 12, TILE - 12, RGB(8, 4, 20));
    draw_frame(x + 22, y + 22, TILE - 44, TILE - 44, 4, col_scale(line, 2, 3));
}

static void draw_cube(float x, float y, float rot)
{
    run_draw_cube(x + PLAYER_S / 2.0f, y + PLAYER_S / 2.0f, rot);
}

static void draw_obstacles(const Endless *g, const Palette *pal)
{
    for (int i = 0; i < g->nobs; i++) {
        const Obstacle *o = &g->obs[i];
        if (o->x > SCREEN_W + 100 || o->x + o->w < -100)
            continue;
        if (o->type == OBS_SPIKE)
            draw_spike(o->x, o->y, pal->line);
        else
            draw_block(o->x, o->y, pal->line);
    }
}

static void draw_trail(const Endless *g, const Palette *pal)
{
    for (int i = TRAIL_N - 1; i >= 2; i -= 2) {
        int sz = 30 - i * 2;
        float x = PLAYER_X + PLAYER_S / 2.0f - g->speed * (float)i;
        float y = g->trail_y[i] + PLAYER_S / 2.0f;
        color_t c = col_lerp(COL_CUBE, pal->bottom, 60 + i * 12);
        draw_rect((int)x - sz / 2, (int)y - sz / 2, sz, sz, c);
    }
}

static void draw_particles(const Endless *g, const Palette *pal)
{
    for (int i = 0; i < MAX_PARTS; i++) {
        const Particle *p = &g->parts[i];
        if (p->life <= 0)
            continue;
        int fade = 256 - p->life * 256 / p->max_life;
        color_t c = col_lerp(p->color, pal->bottom, fade);
        draw_rect((int)p->x - p->size / 2, (int)p->y - p->size / 2, p->size, p->size, c);
    }
}

static void draw_text_centered_plain(int y, int scale, const char *s, color_t c)
{
    draw_text((SCREEN_W - text_width(s, scale)) / 2, y, scale, s, c);
}

static void draw_panel(int x, int y, int w, int h, color_t edge)
{
    draw_rect(x - 14, y - 14, w + 28, h + 28, col_scale(edge, 1, 4));
    draw_rect(x - 6, y - 6, w + 12, h + 12, edge);
    draw_rect(x, y, w, h, RGB(10, 6, 26));
}

static void draw_hud(const Endless *g)
{
    char buf[32];
    itoa_simple(g->score, buf);
    draw_text_glow_centered(50, 10, buf, COL_WHITE, COL_CUBE_CORE);
    label_num(buf, "BEST ", g->best);
    draw_text_glow(SCREEN_W - 60 - text_width(buf, 5), 56, 5, buf, COL_YELLOW, RGB(200, 120, 0));

    if (g->state == EN_PLAY && g->cheer_frames > 0) {
        int rise = 70 - g->cheer_frames;
        draw_text_glow_centered(230 - rise, 11, CHEERS[g->cheer_idx], COL_YELLOW, COL_PINK);
    }
}

static void draw_game_over(const Endless *g)
{
    int w = 1060, h = 600;
    int x = (SCREEN_W - w) / 2, y = 190;
    draw_panel(x, y, w, h, COL_PINK);
    draw_text_glow_centered(y + 60, 14, "GAME OVER", RGB(255, 200, 240), COL_PINK);

    char buf[32];
    label_num(buf, "SCORE ", g->score);
    draw_text_glow_centered(y + 220, 9, buf, COL_WHITE, COL_CUBE_CORE);
    label_num(buf, "BEST ", g->best);
    draw_text_glow_centered(y + 320, 9, buf, COL_YELLOW, RGB(200, 120, 0));
    if (g->new_best && (g->frame / 15) % 2 == 0)
        draw_text_glow_centered(y + 420, 7, "NEW BEST!", COL_CUBE, RGB(30, 140, 10));
    if (g->state_frames > 30 && (g->frame / 30) % 2 == 0)
        draw_text_glow_centered(y + 490, 6, "PRESS X TO TRY AGAIN", COL_WHITE, COL_CUBE_CORE);
    if (g->state_frames > 30)
        draw_text_centered_plain(y + 556, 4, "CIRCLE FOR TITLE", RGB(170, 170, 210));
}

// Draws menu entries centred, one per row; the selected one is boxed and bright.
static void draw_menu(const char *const *items, int count, int sel, int y, int scale, int spacing)
{
    for (int i = 0; i < count; i++) {
        int yy = y + i * spacing;
        if (i == sel) {
            int w = text_width(items[i], scale) + 6 * scale;
            int h = 7 * scale + 4 * scale;
            draw_frame((SCREEN_W - w) / 2, yy - 2 * scale, w, h, 5, COL_CUBE);
            draw_text_glow_centered(yy, scale, items[i], COL_WHITE, COL_CUBE_CORE);
        } else {
            draw_text_centered_plain(yy, scale, items[i], RGB(130, 130, 170));
        }
    }
}

static void draw_pause(const Endless *g)
{
    int w = 1000, h = 640;
    int x = (SCREEN_W - w) / 2, y = 190;
    draw_panel(x, y, w, h, COL_CUBE_CORE);
    draw_text_glow_centered(y + 50, 13, "PAUSED", RGB(200, 250, 255), COL_CUBE_CORE);
    draw_menu(PAUSE_ITEMS, PAUSE_COUNT, g->pause_sel, y + 210, 8, 120);
    draw_text_glow_centered(y + h - 60, 4, "D-PAD MOVE   X SELECT   OPTIONS RESUME",
                            RGB(220, 220, 255), RGB(90, 90, 200));
}

void endless_draw(const Endless *g)
{
    Palette pal = current_palette(g);
    int sx = 0, sy = 0;
    if (g->shake > 0) {
        sx = (g->frame * 7) % 17 - 8;
        sy = (g->frame * 11) % 13 - 6;
    }

    draw_set_offset(0, 0);
    draw_world_bg(g, &pal);

    draw_set_offset(sx, sy);
    draw_obstacles(g, &pal);
    if (g->state == EN_PLAY || g->state == EN_PAUSE) {
        draw_trail(g, &pal);
        draw_cube(PLAYER_X, g->py, g->rot);
    }
    draw_particles(g, &pal);
    draw_set_offset(0, 0);

    if (g->state == EN_OVER) {
        draw_game_over(g);
    } else {
        draw_hud(g);
        if (g->state == EN_PAUSE)
            draw_pause(g);
    }
}
