// Fritz Dash - desktop simulator. Builds the same game code for Linux with a
// file-backed platform layer, then checks it: level loading, the physics of
// every object type, practice checkpoints, the menus and saved files, the
// Endless mode rules, and that every built-in level can be finished (a search
// over button presses plays it through). Writes screenshots as PPM files.
// Usage: sim <output_dir> [sample_level.json ...]
#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "../src/badges.h"
#include "../src/draw.h"
#include "../src/game.h"
#include "../src/mode.h"
#include "../src/rate.h"
#include "../src/platform.h"
#include "../src/transform.h"
#include "../src/synth.h"
#include "solver.h"

static uint32_t fb[SCREEN_W * SCREEN_H];
static const char *outdir = ".";
static char data_root[512];
static int failures = 0;

static void check(int ok, const char *what)
{
    printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        failures++;
}

// --- Platform layer: DATA_DIR lives in <outdir>/data ------------------------

static void map_path(const char *path, char *out, size_t n)
{
    size_t k = strlen(DATA_DIR);
    if (!strncmp(path, DATA_DIR, k))
        snprintf(out, n, "%s%s", data_root, path + k);
    else
        snprintf(out, n, "%s", path);
}

int plat_read_file(const char *path, char *buf, int max)
{
    char p[1024];
    map_path(path, p, sizeof(p));
    FILE *f = fopen(p, "rb");
    if (!f)
        return -1;
    size_t n = fread(buf, 1, (size_t)max, f);
    fclose(f);
    return (int)n;
}

int plat_write_file(const char *path, const char *buf, int len)
{
    char p[1024];
    map_path(path, p, sizeof(p));
    FILE *f = fopen(p, "wb");
    if (!f)
        return -1;
    size_t n = fwrite(buf, 1, (size_t)len, f);
    fclose(f);
    return n == (size_t)len ? 0 : -1;
}

int plat_remove_file(const char *path)
{
    char p[1024];
    map_path(path, p, sizeof(p));
    return remove(p) == 0 ? 0 : -1;
}

int plat_list_dir(const char *dir, void (*fn)(const char *name, void *ctx), void *ctx)
{
    char p[1024];
    map_path(dir, p, sizeof(p));
    DIR *d = opendir(p);
    if (!d)
        return -1;
    struct dirent *e;
    while ((e = readdir(d)) != NULL)
        fn(e->d_name, ctx);
    closedir(d);
    return 0;
}

static void write_text(const char *path, const char *text)
{
    if (plat_write_file(path, text, (int)strlen(text)) != 0) {
        perror(path);
        exit(1);
    }
}

static int read_text(const char *path, char *buf, int max)
{
    int n = plat_read_file(path, buf, max - 1);
    buf[n > 0 ? n : 0] = 0;
    return n;
}

static void reset_data(void)
{
    char cmd[1100];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s' && mkdir -p '%s/levels' '%s/ghosts' '%s/songs'", data_root, data_root, data_root,
             data_root);
    if (system(cmd) != 0)
        exit(1);
}

// --- Screenshots ------------------------------------------------------------

static void save_ppm(const char *name)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.ppm", outdir, name);
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        exit(1);
    }
    fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
    static unsigned char row[SCREEN_W * 3];
    for (int y = 0; y < SCREEN_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            uint32_t v = fb[y * SCREEN_W + x];
            row[x * 3 + 0] = (v >> 16) & 0xFF;
            row[x * 3 + 1] = (v >> 8) & 0xFF;
            row[x * 3 + 2] = v & 0xFF;
        }
        fwrite(row, 1, sizeof(row), f);
    }
    fclose(f);
    printf("wrote %s\n", path);
}

static void shot_game(const Game *g, const char *name)
{
    draw_set_target(fb);
    game_draw(g);
    save_ppm(name);
}

static void shot_run(const Run *r, const char *name)
{
    draw_set_target(fb);
    run_draw(r, HUD_PROGRESS | HUD_TEXT);
    save_ppm(name);
}

// --- Test levels from ASCII rows -------------------------------------------
// Rows are top first; the last row is y = 0. Same characters as
// tools/gen_levels.py.

static Level tl;

static int bi(const char *id)
{
    for (int i = 0; i < BUILTIN_COUNT; i++)
        if (!strcmp(BUILTIN_IDS[i], id))
            return i;
    printf("FAIL: no built-in level %s\n", id);
    exit(1);
}

static const char *map_obj(char c, int *rot)
{
    *rot = 0;
    switch (c) {
    case '#': return "block";
    case '^': return "spike";
    case 'v': *rot = 2; return "spike";
    case ',': return "half_spike";
    case '=': return "platform";
    case 'y': return "pad_yellow";
    case 'p': return "pad_pink";
    case 'o': return "orb_yellow";
    case '*': return "orb_pink";
    case 'G': return "portal_gravity_up";
    case 'g': return "portal_gravity_down";
    case '1': return "portal_speed_05";
    case '2': return "portal_speed_1";
    case '3': return "portal_speed_2";
    case '4': return "portal_speed_3";
    case 'C': return "portal_cube";
    case 'S': return "portal_ship";
    case 'B': return "portal_ball";
    case 'U': return "portal_ufo";
    case 'W': return "portal_wave";
    case 'n': return "portal_mini";
    case 'N': return "portal_big";
    case 'O': return "saw";
    case '@': return "saw_big";
    case '&': return "pulse";
    case 'H': return "pulse_bars";
    case '~': return "deco_glow";
    case '+': return "deco_star";
    case ')': return "deco_arrow";
    case '%': return "deco_block";
    case 'w': return "deco_spikes";
    case '$': return "coin";
    case '!': return "sign";
    case '|': return "end";
    }
    return NULL;
}

static const Level *mk(int speed, int n, const char *const *rows)
{
    static char json[1 << 16];
    int len = snprintf(json, sizeof(json), "{\"name\":\"T\",\"speed\":%d,\"objects\":[", speed);
    int first = 1;
    for (int i = 0; i < n; i++)
        for (int x = 0; rows[i][x]; x++) {
            int rot;
            const char *t = map_obj(rows[i][x], &rot);
            if (!t)
                continue;
            len += snprintf(json + len, sizeof(json) - (size_t)len, "%s{\"t\":\"%s\",\"x\":%d,\"y\":%d,\"r\":%d}",
                            first ? "" : ",", t, x, n - 1 - i, rot);
            first = 0;
        }
    len += snprintf(json + len, sizeof(json) - (size_t)len, "]}");
    if (level_parse(&tl, json, len) != LEVEL_OK) {
        printf("FAIL: test level did not parse\n");
        exit(1);
    }
    return &tl;
}

#define MK(speed, ...) mk(speed, (int)(sizeof((const char *const[]){ __VA_ARGS__ }) / sizeof(const char *)), \
                          (const char *const[]){ __VA_ARGS__ })

static float feet(const Run *r)
{
    return r->b.y - run_half_h(&r->b);
}

// Starts a run on the current test level and releases the start lock.
static void begin(Run *r, int practice)
{
    run_start(r, &tl, practice, 0);
    run_step(r, 0);
}

// Presses X on frame j (held for one frame) and runs until death or `frames`.
// Returns 1 if the run is still alive (or finished).
static int jump_at(int j, int frames)
{
    Run r;
    begin(&r, 0);
    for (int t = 0; t < frames; t++) {
        int ev = run_step(&r, t == j ? IN_CROSS : 0);
        if (ev == EV_DIED)
            return 0;
        if (ev == EV_COMPLETE)
            return 1;
    }
    return 1;
}

static int window_count(int frames)
{
    int n = 0;
    for (int j = 0; j < 80; j++)
        n += jump_at(j, frames);
    return n;
}

// --- Level parser -----------------------------------------------------------

static void test_parser(void)
{
    static Level lv;
    const char *good =
        "{ \"format\": \"fritzdash-level\", \"version\": 1, \"name\": \"Hi \\\"There\\\" \\u00e9\",\n"
        "  \"author\": \"FRITZ\", \"speed\": 7, \"mode\": \"ship\", \"future\": { \"a\": [1, 2, {\"b\": null}], \"c\": true },\n"
        "  \"colors\": { \"top\": \"#102030\", \"line\": \"#FFee00\", \"ground\": \"bad\" },\n"
        "  \"objects\": [ {\"t\": \"spike\", \"x\": 9, \"y\": 0, \"r\": 6},\n"
        "    {\"t\": \"block\", \"x\": 3, \"y\": 2, \"extra\": [1, 2]},\n"
        "    {\"t\": \"laser\", \"x\": 4, \"y\": 0},\n"
        "    {\"t\": \"block\", \"x\": -1, \"y\": 0},\n"
        "    {\"t\": \"end\", \"x\": 40, \"y\": 0},\n"
        "    {\"t\": \"block\", \"x\": 3.0, \"y\": 1e0} ] }\n";
    int rc = level_parse(&lv, good, (int)strlen(good));
    check(rc == LEVEL_OK, "a level with unknown keys and objects parses");
    check(!strcmp(lv.name, "Hi \"There\" ?"), "string escapes are read");
    check(lv.speed == 3 && lv.mode == MODE_SHIP, "speed is clamped to 3 and mode ship is read");
    check(lv.colors.top == RGB(0x10, 0x20, 0x30) && lv.colors.line == RGB(0xFF, 0xEE, 0x00) && lv.colors.ground == RGB(20, 6, 52),
          "colours parse; a bad colour keeps the default");
    check(lv.nobj == 4 && lv.skipped == 2, "unknown type and negative x are skipped and counted");
    check(lv.objs[0].x == 3 && lv.objs[0].y == 1 && lv.objs[1].y == 2 && lv.objs[2].x == 9 && lv.objs[3].x == 40,
          "objects are sorted by x, then y");
    check(lv.objs[2].rot == 2, "rotation is taken modulo 4");
    check(lv.end_x == 40 * TILE, "the end object sets the finish line");

    const char *bad[] = {
        "{\"name\":\"x\",\"objects\":[{\"t\":\"spike\",\"x\":1,\"y\":0}]",
        "{\"name\":\"x\"} trailing",
        "{\"name\":\"x\",\"speed\":-}",
        "{\"name\":\"unterminated}",
        "[1, 2, 3]",
        "",
    };
    int all_bad = 1;
    for (int i = 0; i < (int)(sizeof(bad) / sizeof(bad[0])); i++)
        all_bad &= level_parse(&lv, bad[i], (int)strlen(bad[i])) == LEVEL_BAD_JSON;
    check(all_bad, "broken JSON is rejected");
    const char *other = "{\"format\":\"something-else\",\"objects\":[]}";
    check(level_parse(&lv, other, (int)strlen(other)) == LEVEL_BAD_FORMAT, "a different file format is rejected");
    const char *empty = "{\"objects\":[]}";
    check(level_parse(&lv, empty, (int)strlen(empty)) == LEVEL_OK && lv.nobj == 0 && lv.end_x == 30 * TILE,
          "an empty level loads with a default length");

    for (int i = 0; i < BUILTIN_COUNT; i++) {
        char what[96];
        int ok = level_parse(&lv, BUILTIN_JSON[i], (int)strlen(BUILTIN_JSON[i])) == LEVEL_OK && lv.skipped == 0;
        snprintf(what, sizeof(what), "built-in level %s parses with no skipped objects (%d objects)",
                 BUILTIN_IDS[i], lv.nobj);
        check(ok, what);
    }
}

// --- Physics ----------------------------------------------------------------

static void test_physics(void)
{
    Run r;
    char what[160];

    // Jump arc and speeds on open ground.
    MK(1, "..........................................................................................|");
    begin(&r, 0);
    run_step(&r, IN_CROSS);
    float apex = 0;
    int air = 1;
    while (!r.b.on_ground && air < 100) {
        run_step(&r, 0);
        if (feet(&r) > apex)
            apex = feet(&r);
        air++;
    }
    snprintf(what, sizeof(what), "cube jump is about 2.1 cells high and 23 frames long (%.1f cells, %d frames)",
             apex / TILE, air);
    check(apex > 2.0f * TILE && apex < 2.3f * TILE && air >= 21 && air <= 25, what);
    begin(&r, 0);
    run_step(&r, IN_CROSS);
    int jumps = 0;
    for (int i = 0; i < 120; i++)
        run_step(&r, IN_CROSS);
    jumps = r.jumps;
    check(r.state == RUN_PLAY && jumps >= 5, "holding X keeps jumping");

    for (int s = 0; s < 4; s++) {
        static const char *rows[1] = { "............................................|" };
        mk(s, 1, rows);
        begin(&r, 0);
        float x0 = r.b.x;
        for (int i = 0; i < 10; i++)
            run_step(&r, 0);
        float v = (r.b.x - x0) / 10.0f;
        float want = run_speed_px(s);
        snprintf(what, sizeof(what), "speed %d moves %.2f px per frame", s, v);
        check(v > want - 0.01f && v < want + 0.01f && want > 0, what);
    }

    // Spikes: never jumping dies; the timing windows match the design.
    MK(1, "..............^.............|");
    begin(&r, 0);
    int ev = EV_NONE, f = 0;
    while (ev == EV_NONE && f < 400) {
        ev = run_step(&r, 0);
        f++;
    }
    check(ev == EV_DIED && r.state == RUN_DEAD && r.pct > 0 && r.pct < 99, "running into a spike dies, progress recorded");

    printf("\nSpike jump windows (frames at 60 fps where one press clears them):\n");
    printf("%-10s %8s %8s %8s %8s\n", "spikes", "0.5x", "1x", "2x", "3x");
    static const char *spikes[3] = { "........^............................|",
                                     "........^^...........................|",
                                     "........^^^..........................|" };
    int win[3][4];
    for (int k = 0; k < 3; k++) {
        printf("%-10d", k + 1);
        for (int s = 0; s < 4; s++) {
            mk(s, 1, &spikes[k]);
            win[k][s] = window_count(140);
            printf(" %8d", win[k][s]);
        }
        printf("\n");
    }
    check(win[0][0] >= 10 && win[0][1] >= 10 && win[1][1] >= 5 && win[1][3] >= 5,
          "one spike gives 10+ frames and two spikes 5+ frames at 1x and up");
    check(win[2][0] == 0 && win[2][1] > 0, "three spikes are impossible at 0.5x but possible at 1x, as in Geometry Dash");

    // Blocks: a two-high wall can be jumped onto, a three-high one cannot.
    MK(1, "..............#..................|", "..............#..................|");
    check(window_count(140) > 0, "a two-high wall can be jumped onto");
    MK(1, "..............#..", "..............#..", "..............#..................|");
    check(window_count(140) == 0, "a three-high wall cannot be jumped onto");
    MK(1, "..............#..................|");
    begin(&r, 0);
    ev = EV_NONE;
    for (f = 0; f < 200 && ev == EV_NONE; f++)
        ev = run_step(&r, 0);
    check(ev == EV_DIED, "running into the side of a block dies");
    MK(1, "..............########...........|");
    int landed = 0;
    for (int j = 0; j < 80 && !landed; j++) {
        begin(&r, 0);
        ev = EV_NONE;
        for (f = 0; f < 140 && ev == EV_NONE; f++) {
            ev = run_step(&r, f == j ? IN_CROSS : 0);
            if (r.b.on_ground && r.b.x > 15 * TILE && r.b.x < 21 * TILE && feet(&r) > TILE - 0.5f && feet(&r) < TILE + 0.5f)
                landed = 1;
        }
    }
    check(landed, "a jump lands on top of a block row and runs along it");
    MK(1, "..............#...........", "...........................", ".............................|");
    begin(&r, 0);
    ev = EV_NONE;
    for (f = 0; f < 200 && ev == EV_NONE; f++)
        ev = run_step(&r, 0);
    check(ev == EV_COMPLETE, "running under a block two cells up is safe");
    check(window_count(160) < 80, "jumping into the underside of a block dies");

    // Pads.
    MK(1, "..........y.............................|");
    begin(&r, 0);
    apex = 0;
    for (f = 0; f < 140; f++) {
        run_step(&r, 0);
        if (feet(&r) > apex)
            apex = feet(&r);
    }
    snprintf(what, sizeof(what), "yellow pad launches about 4 cells high (%.1f)", apex / TILE);
    check(apex > 3.5f * TILE && apex < 4.3f * TILE && r.state == RUN_PLAY, what);
    MK(1, "..........p.............................|");
    begin(&r, 0);
    apex = 0;
    for (f = 0; f < 140; f++) {
        run_step(&r, 0);
        if (feet(&r) > apex)
            apex = feet(&r);
    }
    snprintf(what, sizeof(what), "pink pad gives a small hop (%.1f cells)", apex / TILE);
    check(apex > 1.0f * TILE && apex < 1.8f * TILE, what);

    // Orbs: a fresh press while touching jumps again; a press held since
    // the ground jump does not; a press made before reaching it does.
    MK(1, "....o..............................|", "...................................", "...................................|");
    float jump_apex = 0, apex_a = 0, apex_b = 0, apex_c = 0;
    for (int mode = 0; mode < 4; mode++) {
        begin(&r, 0);
        int in_orb_seen = 0;
        apex = 0;
        for (f = 0; f < 120; f++) {
            int in = 0;
            int touching = r.ntouch > 0;
            if (f == 3) in = IN_CROSS;                                   // ground jump
            if (mode == 1 && touching) in = IN_CROSS, in_orb_seen = 1;   // a: press inside the orb
            if (mode == 2 && f > 3 && f < 30) in = IN_CROSS;             // b: keep holding
            if (mode == 3 && f >= 8 && f < 30) in = IN_CROSS;            // c: press again early, hold
            if (mode == 1 && in_orb_seen && !touching) in = 0;
            run_step(&r, in);
            if (feet(&r) > apex)
                apex = feet(&r);
            if (r.b.on_ground && f > 10)
                break;
        }
        if (mode == 0) jump_apex = apex;
        if (mode == 1) apex_a = apex;
        if (mode == 2) apex_b = apex;
        if (mode == 3) apex_c = apex;
    }
    snprintf(what, sizeof(what), "orb: press inside jumps again (%.0f vs %.0f px)", apex_a, jump_apex);
    check(apex_a > jump_apex + 60, what);
    check(apex_b < jump_apex + 1, "orb: X held since the ground jump does not use the orb");
    check(apex_c > jump_apex + 60, "orb: a press made before touching it is used on contact");

    // Gravity portals with a ceiling.
    MK(1, "##################################################",
          "...................................g..............",
          "..................................................",
          "..................................................",
          "..................................................",
          "..................................................",
          ".....G............................................|");
    begin(&r, 0);
    int flipped_floor = 0;
    for (f = 0; f < 60; f++) {
        run_step(&r, 0);
        if (r.b.grav < 0 && r.b.on_ground && r.b.y + run_half_height(MODE_CUBE) > 6 * TILE - 0.5f)
            flipped_floor = 1;
    }
    check(flipped_floor, "gravity up portal: the cube falls up and runs on the ceiling");
    begin(&r, 0);
    for (f = 0; f < 60; f++)
        run_step(&r, 0);
    run_step(&r, IN_CROSS);
    float low = 1e9f;
    for (f = 0; f < 25; f++) {
        run_step(&r, 0);
        if (r.b.y < low)
            low = r.b.y;
    }
    check(low < 6 * TILE - 2.0f * TILE, "jumping while upside down jumps downwards");
    ev = EV_NONE;
    for (f = 0; f < 300 && ev == EV_NONE; f++)
        ev = run_step(&r, 0);
    check(ev == EV_COMPLETE && r.b.grav == 1, "gravity down portal brings the cube back to the ground");

    // Speed portals.
    MK(1, "......1.........3.........4.........2..............|");
    begin(&r, 0);
    int seen[4] = { -1, -1, -1, -1 }, k = 0, last = r.b.speed;
    for (f = 0; f < 400 && r.state == RUN_PLAY; f++) {
        run_step(&r, 0);
        if (r.b.speed != last && k < 4)
            seen[k++] = last = r.b.speed;
    }
    check(seen[0] == 0 && seen[1] == 2 && seen[2] == 3 && seen[3] == 1, "speed portals switch 0.5x, 2x, 3x, 1x");

    // Ship: holding rises to the corridor top, releasing falls to the floor,
    // neither kills; walls do.
    MK(1, "..................................................................",
          "..................................................................",
          "..S...............................................................|");
    begin(&r, 0);
    for (f = 0; f < 20; f++)
        run_step(&r, 0);
    check(r.b.mode == MODE_SHIP && r.b.lo == 0 && r.b.hi == 10 * TILE, "ship portal: ship with a ten cell corridor");
    for (f = 0; f < 120; f++)
        run_step(&r, IN_CROSS);
    check(r.state == RUN_PLAY && r.b.y + run_half_height(MODE_SHIP) > 10 * TILE - 0.5f, "holding X flies the ship to the ceiling safely");
    for (f = 0; f < 140; f++)
        run_step(&r, 0);
    check(r.state == RUN_PLAY && r.b.y - run_half_height(MODE_SHIP) < 0.5f, "releasing lets the ship sink to the floor safely");

    MK(1, "..S..............#####.....................|", "..................#####.....................|");
    begin(&r, 0);
    ev = EV_NONE;
    for (f = 0; f < 200 && ev == EV_NONE; f++)
        ev = run_step(&r, 0);
    check(ev == EV_DIED, "a ship flying into a wall dies");
    MK(1, "..S........######################.........|");
    begin(&r, 0);
    ev = EV_NONE;
    int slid = 0;
    for (f = 0; f < 320 && ev == EV_NONE; f++) {
        ev = run_step(&r, f >= 12 && f < 34 ? IN_CROSS : 0);
        if (r.b.x > 14 * TILE && r.b.x < 30 * TILE && r.b.y - run_half_height(MODE_SHIP) > TILE - 0.5f &&
            r.b.y - run_half_height(MODE_SHIP) < TILE + 0.5f)
            slid = 1;
    }
    check(ev == EV_COMPLETE && slid, "a ship slides along the top of blocks");
    MK(1, "...........######################.........",
          "..........................................",
          "..........................................",
          "..........................................",
          "..........................................",
          "..........................................",
          "..........................................",
          "..........................................",
          "..........................................",
          "..S.......................................|");
    begin(&r, 0);
    ev = EV_NONE;
    slid = 0;
    for (f = 0; f < 320 && ev == EV_NONE; f++) {
        ev = run_step(&r, f > 10 ? IN_CROSS : 0);
        if (r.b.x > 14 * TILE && r.b.x < 30 * TILE && r.b.y + run_half_height(MODE_SHIP) > 9 * TILE - 0.5f &&
            r.b.y + run_half_height(MODE_SHIP) < 9 * TILE + 0.5f)
            slid = 1;
    }
    check(ev == EV_COMPLETE && slid, "a ship slides along the underside of blocks");

    MK(1, "..S.........C................................|");
    begin(&r, 0);
    for (f = 0; f < 100; f++)
        run_step(&r, 0);
    check(r.b.mode == MODE_CUBE && r.state == RUN_PLAY, "cube portal turns the ship back into a cube");

    // Progress and the finish line.
    MK(1, "..............................|");
    begin(&r, 0);
    int mono = 1, prev = 0;
    ev = EV_NONE;
    for (f = 0; f < 400 && ev == EV_NONE; f++) {
        ev = run_step(&r, 0);
        if (ev == EV_NONE && (r.pct < prev || r.pct > 99))
            mono = 0;
        prev = r.pct;
    }
    check(ev == EV_COMPLETE && r.pct == 100 && r.state == RUN_DONE && mono, "progress climbs to 99 and the end wall finishes at 100");
}

// --- Ball, UFO, wave and mini --------------------------------------------------

// A level drawn into a grid by cell, for levels too wide to write as rows.
#define GRID_W 200
#define GRID_H 12
static char grid[GRID_H][GRID_W + 1];
#define RACE_COUNT_TEST 180

static void grid_clear(int width)
{
    for (int y = 0; y < GRID_H; y++) {
        memset(grid[y], '.', (size_t)width);
        grid[y][width] = 0;
    }
}

static void grid_put(int x, int y, char c)
{
    grid[GRID_H - 1 - y][x] = c;
}

static const Level *grid_level(int speed)
{
    const char *rows[GRID_H];
    for (int i = 0; i < GRID_H; i++)
        rows[i] = grid[i];
    return mk(speed, GRID_H, rows);
}

// Runs n frames with the same input; returns the event that stopped it, if any.
static int run_for(Run *r, int n, int in)
{
    for (int f = 0; f < n; f++) {
        int ev = run_step(r, in);
        if (ev == EV_DIED || ev == EV_COMPLETE)
            return ev;
    }
    return EV_NONE;
}

static void test_modes(void)
{
    Run r;
    char what[160];

    // Ball: X flips gravity, held it flips on every landing; corridor of 8.
    MK(1, "..B...........................................................|");
    begin(&r, 0);
    run_for(&r, 20, 0);
    check(r.b.mode == MODE_BALL && r.b.lo == 0 && r.b.hi == 8 * TILE && r.b.on_ground && r.b.grav == 1,
          "ball portal: a ball rolling in an eight cell corridor");
    run_step(&r, IN_CROSS);
    int ev = run_for(&r, 40, 0);
    check(ev == EV_NONE && r.b.grav == -1 && r.b.on_ground && r.b.y + run_half_h(&r.b) > 8 * TILE - 0.5f,
          "ball: X flips it onto the ceiling");
    int flips = 0, g = r.b.grav;
    for (int f = 0; f < 150 && r.state == RUN_PLAY; f++) {
        run_step(&r, IN_CROSS);
        if (r.b.grav != g)
            flips++, g = r.b.grav;
    }
    snprintf(what, sizeof(what), "ball: holding X flips again on every landing (%d flips)", flips);
    check(r.state == RUN_PLAY && flips >= 4, what);

    // UFO: each press flaps, in the air too; holding does not repeat.
    MK(1, "..U.......................................................................................................................................|");
    begin(&r, 0);
    run_for(&r, 20, 0);
    check(r.b.mode == MODE_UFO && r.b.hi == 10 * TILE && feet(&r) < 0.5f, "ufo portal: a UFO on the floor");
    run_step(&r, IN_CROSS);
    float apex1 = 0;
    for (int f = 0; f < 40; f++) {
        run_step(&r, IN_CROSS);
        if (feet(&r) > apex1)
            apex1 = feet(&r);
    }
    snprintf(what, sizeof(what), "ufo: one press is one flap about 1.3 cells high (%.2f cells, %d flaps)", apex1 / TILE,
             r.jumps);
    check(r.jumps == 1 && apex1 > 1.1f * TILE && apex1 < 1.6f * TILE, what);
    run_for(&r, 30, 0);
    float apex2 = 0;
    for (int f = 0; f < 60; f++) {
        run_step(&r, (f == 0 || f == 10 || f == 20) ? IN_CROSS : 0);
        if (feet(&r) > apex2)
            apex2 = feet(&r);
    }
    check(apex2 > apex1 + 0.8f * TILE, "ufo: pressing again in the air flaps again");
    float high = 0;
    for (int f = 0; f < 300 && r.state == RUN_PLAY; f++) {
        run_step(&r, f % 8 == 0 ? IN_CROSS : 0);
        if (r.b.y + run_half_h(&r.b) > high)
            high = r.b.y + run_half_h(&r.b);
    }
    check(r.state == RUN_PLAY && high > 10 * TILE - 0.5f, "ufo: flapping into the ceiling is safe");

    // Wave: straight diagonals, any block kills, the corridor edges do not.
    MK(1, "..W...........................................................|");
    begin(&r, 0);
    run_for(&r, 12, 0);
    float x0 = r.b.x, y0 = r.b.y;
    run_for(&r, 10, IN_CROSS);
    float dx = r.b.x - x0, dy = r.b.y - y0;
    snprintf(what, sizeof(what), "wave: holding X flies up at 45 degrees (%.1f across, %.1f up)", dx, dy);
    check(r.b.mode == MODE_WAVE && dy > dx - 1.0f && dy < dx + 1.0f && dx > 0, what);
    x0 = r.b.x, y0 = r.b.y;
    run_for(&r, 5, 0);
    check(r.b.y - y0 < -(r.b.x - x0) + 1.0f && r.b.y - y0 > -(r.b.x - x0) - 1.0f, "wave: letting go flies down at 45 degrees");
    ev = run_for(&r, 120, IN_CROSS);
    check(ev == EV_NONE && r.b.y + run_half_h(&r.b) > 10 * TILE - 0.5f, "wave: sliding along the corridor ceiling is safe");
    MK(1, "..W..........#................................................|");
    begin(&r, 0);
    ev = run_for(&r, 200, 0);
    check(ev == EV_DIED, "wave: touching a block dies");
    MK(1, "..W..........y..o.............................................|");
    begin(&r, 0);
    float top = 0;
    for (int f = 0; f < 100; f++) {
        run_step(&r, f == 40 ? IN_CROSS : 0);
        if (r.b.x > 12 * TILE && r.b.x < 17 * TILE && feet(&r) > top)
            top = feet(&r);
    }
    check(top < 40.0f, "wave: pads and orbs do nothing to it");

    // Mini: half size, a lower jump; the big portal undoes it.
    MK(1, "..n...........................................................|");
    begin(&r, 0);
    run_for(&r, 20, 0);
    check(r.b.mini && run_half_h(&r.b) == PLAYER_S / 4.0f, "mini portal: the cube shrinks to half size");
    run_step(&r, IN_CROSS);
    float apex = 0;
    for (int f = 0; f < 40; f++) {
        run_step(&r, 0);
        if (feet(&r) > apex)
            apex = feet(&r);
    }
    snprintf(what, sizeof(what), "mini cube jumps about 1.3 cells (%.2f cells)", apex / TILE);
    check(apex > 1.15f * TILE && apex < 1.55f * TILE, what);
    MK(1, "..n....N......................................................|");
    begin(&r, 0);
    run_for(&r, 40, 0);
    check(!r.b.mini && feet(&r) < 0.5f, "big portal: back to full size, standing on the ground");
    MK(1, "..nW..........................................................|");
    begin(&r, 0);
    run_for(&r, 24, 0);
    x0 = r.b.x, y0 = r.b.y;
    run_for(&r, 6, IN_CROSS);
    dx = r.b.x - x0, dy = r.b.y - y0;
    check(r.b.mini && dy > 2 * dx - 1.0f && dy < 2 * dx + 1.0f, "mini wave flies twice as steep");
    MK(1, "..n.......S...................................................|");
    begin(&r, 0);
    run_for(&r, 70, 0);
    check(r.b.mode == MODE_SHIP && r.b.mini && run_half_h(&r.b) == 12.0f, "a mini portal shrinks the ship too");
}

// --- Moving, spinning and beating objects ----------------------------------------

static const Level *mkj(int speed, const char *objs)
{
    static char json[1 << 16];
    int len = snprintf(json, sizeof(json), "{\"name\":\"T\",\"speed\":%d,\"objects\":[%s]}", speed, objs);
    if (level_parse(&tl, json, len) != LEVEL_OK) {
        printf("FAIL: test level did not parse\n");
        exit(1);
    }
    return &tl;
}

static void test_moving(void)
{
    Run r;
    char buf[2048], what[160];
    static const char WALL[] = "{\"t\":\"block\",\"x\":20,\"y\":0,\"g\":1},{\"t\":\"block\",\"x\":20,\"y\":1,\"g\":1},"
                               "{\"t\":\"block\",\"x\":20,\"y\":2,\"g\":1},{\"t\":\"spike\",\"x\":34,\"y\":0},"
                               "{\"t\":\"end\",\"x\":60,\"y\":0}";
    mkj(1, WALL);
    begin(&r, 0);
    check(tl.nmoving == 3 && run_for(&r, 300, 0) == EV_DIED && r.b.x < 20 * TILE, "a wall in a group stays put with no trigger");
    snprintf(buf, sizeof(buf), "%s,{\"t\":\"move_trigger\",\"x\":8,\"y\":0,\"g\":1,\"dy\":3,\"f\":30}", WALL);
    mkj(1, buf);
    begin(&r, 1);
    while (r.b.x < 8.5f * TILE)
        run_step(&r, 0);
    float ox, oy;
    run_group_offset(&r.b, 1, &ox, &oy);
    int at_start = oy == 0;
    run_for(&r, 15, 0);
    run_group_offset(&r.b, 1, &ox, &oy);
    float half = oy;
    run_step(&r, IN_SQUARE);
    Body cp = r.cps[0];
    run_for(&r, 15, 0);
    run_group_offset(&r.b, 1, &ox, &oy);
    snprintf(what, sizeof(what), "move trigger: the group eases up 3 cells over 30 frames (%.0f px halfway, %.0f at the end)",
             half, oy);
    check(at_start && half > 100 && half < 140 && oy == 3 * TILE && ox == 0, what);
    int ev = run_for(&r, 400, 0);
    check(ev == EV_DIED && r.b.x > 30 * TILE, "the cube runs under the lifted wall (and on into a spike)");
    while (run_step(&r, 0) != EV_RESPAWN)
        ;
    check(r.b.t == cp.t && !memcmp(r.b.grp, cp.grp, sizeof(cp.grp)), "a practice checkpoint brings the moving group back with it");
    run_start(&r, &tl, 1, 0);
    run_start_at(&r, 15);
    run_group_offset(&r.b, 1, &ox, &oy);
    check(oy == 3 * TILE, "playtesting past a move trigger starts with the group moved");
    run_group_offset(&r.b, 0, &ox, &oy);
    check(ox == 0 && oy == 0, "group 0 never moves");

    // A saw in group 2 drops into the path.
    mkj(1, "{\"t\":\"saw\",\"x\":24,\"y\":4,\"g\":2},{\"t\":\"move_trigger\",\"x\":6,\"y\":0,\"g\":2,\"dy\":-4,\"f\":20},"
           "{\"t\":\"end\",\"x\":40,\"y\":0}");
    begin(&r, 0);
    check(run_for(&r, 400, 0) == EV_DIED && r.b.x > 22 * TILE && r.b.x < 25 * TILE, "a moving saw that drops into the path kills");

    // Saws: round hitboxes.
    mkj(1, "{\"t\":\"saw\",\"x\":14,\"y\":0},{\"t\":\"end\",\"x\":40,\"y\":0}");
    begin(&r, 0);
    check(run_for(&r, 300, 0) == EV_DIED, "running into a saw dies");
    mkj(1, "{\"t\":\"saw_big\",\"x\":20,\"y\":0},{\"t\":\"end\",\"x\":40,\"y\":0}");
    int died = 0, clipped = 0;
    for (int j = 0; j < 120; j++) {
        begin(&r, 0);
        int clip = 0;
        ev = EV_NONE;
        for (int f = 0; f < 300 && ev == EV_NONE; f++) {
            ev = run_step(&r, f == j ? IN_CROSS : 0);
            float x0 = r.b.x - 32, x1 = r.b.x + 32, y0 = r.b.y - 32;
            if (x1 > 1552 && x0 < 1728 && y0 < 128)
                clip = 1;
        }
        died += ev == EV_DIED;
        clipped += ev != EV_DIED && clip;
    }
    snprintf(what, sizeof(what), "big saw: round hitbox, %d jumps clip a corner of its square and live, %d die", clipped, died);
    check(died > 0 && clipped > 0, what);

    // Decoration never collides; pulses follow the song's beat.
    mkj(1, "{\"t\":\"deco_block\",\"x\":12,\"y\":0},{\"t\":\"deco_spikes\",\"x\":14,\"y\":0},"
           "{\"t\":\"deco_glow\",\"x\":16,\"y\":0},{\"t\":\"deco_star\",\"x\":18,\"y\":0},"
           "{\"t\":\"deco_arrow\",\"x\":20,\"y\":0},{\"t\":\"pulse\",\"x\":22,\"y\":0},"
           "{\"t\":\"pulse_bars\",\"x\":24,\"y\":0},{\"t\":\"end\",\"x\":40,\"y\":0}");
    begin(&r, 0);
    check(run_for(&r, 400, 0) == EV_COMPLETE, "decoration and pulses never collide");
    r.bpm = 120;
    r.song_t = 60;
    float on = run_beat(&r);
    r.song_t = 75;
    float off = run_beat(&r);
    r.song_t = 89;
    check(on == 1.0f && off == 0.25f && run_beat(&r) < 0.01f, "the beat clock peaks on every beat of the song's tempo");

    // Groups and move triggers survive a save.
    static Level a;
    static char text[1 << 16];
    snprintf(buf, sizeof(buf), "%s,{\"t\":\"move_trigger\",\"x\":8,\"y\":0,\"g\":1,\"dx\":-2,\"dy\":3,\"f\":45}", WALL);
    mkj(1, buf);
    int n = level_write(&tl, text, (int)sizeof(text));
    int k = level_find(&tl, 8, 0);
    check(n > 0 && level_parse(&a, text, n) == LEVEL_OK && a.nmoving == 3 && k >= 0 &&
          !memcmp(a.objs, tl.objs, sizeof(LevelObj) * (size_t)tl.nobj) && a.objs[k].dx == -2 && a.objs[k].time == 45,
          "groups and move triggers are written and read back");
}

// Objects on show: saws, pulses, decoration and a group on the move.
static const Level *showcase(void)
{
    grid_clear(60);
    grid_put(9, 4, 'O'); grid_put(13, 5, '@'); grid_put(19, 3, '&'); grid_put(20, 3, '&');
    grid_put(21, 0, 'H'); grid_put(22, 0, 'H'); grid_put(23, 0, 'H');
    for (int x = 8; x < 30; x++) grid_put(x, 8, '~');
    grid_put(10, 7, '+'); grid_put(16, 6, '+'); grid_put(25, 7, '+');
    grid_put(5, 1, ')'); grid_put(6, 1, ')');
    grid_put(26, 0, '%'); grid_put(26, 1, '%'); grid_put(27, 0, '%');
    for (int x = 2; x < 8; x++) grid_put(x, 0, 'w');
    grid_put(58, 0, '|');
    const Level *lv = grid_level(1);
    static Level show;
    show = *lv;
    // Blocks at 16..17, 0..1 in group 1 rise when the trigger at 3 is passed.
    for (int y = 0; y < 2; y++)
        for (int x = 16; x < 18; x++) {
            LevelObj o = { (int16_t)x, (int16_t)y, OBJ_BLOCK, 0, 0, 1, 0, 0, 0, { 0 } };
            level_put(&show, o);
        }
    LevelObj m = { 3, 0, OBJ_MOVE, 0, 0, 1, 0, 3, 40, { 0 } };
    level_put(&show, m);
    level_update_end(&show);
    return &show;
}

// A short level through every mode, for the solver and the screenshots.
static const Level *mode_tour(void)
{
    grid_clear(130);
    grid_put(10, 1, 'B');
    grid_put(18, 0, '^'); grid_put(19, 0, '^');
    grid_put(28, 7, 'v'); grid_put(29, 7, 'v');
    // Blocks around each portal so it cannot be flown past.
    grid_put(40, 1, 'U');
    for (int y = 3; y < 8; y++) grid_put(40, y, '#');
    for (int y = 0; y < 3; y++) grid_put(48, y, '#');
    for (int y = 6; y < 10; y++) grid_put(57, y, '#');
    grid_put(66, 4, 'W');
    for (int y = 0; y < 3; y++) grid_put(66, y, '#');
    for (int y = 6; y < 10; y++) grid_put(66, y, '#');
    for (int y = 0; y < 4; y++) grid_put(74, y, '#');
    for (int y = 6; y < 10; y++) grid_put(84, y, '#');
    grid_put(94, 1, 'C');
    for (int y = 3; y < 10; y++) grid_put(94, y, '#');
    grid_put(97, 0, 'n');
    grid_put(104, 0, '^');
    grid_put(110, 0, 'N');
    grid_put(116, 0, '^');
    grid_put(128, 0, '|');
    return grid_level(1);
}

// --- Practice mode ----------------------------------------------------------

static void test_practice(void)
{
    Run r;
    MK(1, "............................................^.................|");
    begin(&r, 1);
    for (int f = 0; f < 60; f++)
        run_step(&r, 0);
    run_step(&r, IN_SQUARE);
    float cpx = r.cps[0].x;
    check(r.ncp == 1, "Square places a checkpoint in practice");
    int ev = EV_NONE;
    while (ev != EV_DIED)
        ev = run_step(&r, 0);
    while (ev != EV_RESPAWN)
        ev = run_step(&r, 0);
    check(r.b.x == cpx && r.attempt == 2 && r.state == RUN_PLAY, "dying respawns at the checkpoint");
    run_step(&r, IN_TRIANGLE);
    check(r.ncp == 0, "Triangle removes the last checkpoint");
    while (ev != EV_DIED)
        ev = run_step(&r, 0);
    while (ev != EV_RESPAWN)
        ev = run_step(&r, 0);
    check(r.b.x < TILE, "with no checkpoints a death restarts from the beginning");

    // A checkpoint with no way to survive is dropped after one quick death.
    while (r.b.x < 3440)
        run_step(&r, 0);
    run_step(&r, IN_SQUARE);
    ev = EV_NONE;
    while (ev != EV_DIED)
        ev = run_step(&r, 0);
    while (ev != EV_RESPAWN)
        ev = run_step(&r, 0);
    int n_after_first = r.ncp;
    while (ev != EV_DIED)
        ev = run_step(&r, 0);
    check(n_after_first == 1 && r.ncp == 0, "a checkpoint that dies again at once is removed");

    // Automatic checkpoints, and switching practice on and off mid-run.
    MK(1, "..............................................................|");
    run_start(&r, &tl, 1, 1);
    for (int f = 0; f < 300 && r.state == RUN_PLAY; f++)
        run_step(&r, 0);
    check(r.ncp >= 2, "auto checkpoints drop one every 2 seconds");
    run_start(&r, &tl, 0, 0);
    for (int f = 0; f < 40; f++)
        run_step(&r, 0);
    float x = r.b.x;
    run_set_practice(&r, 1);
    check(r.practice && r.ncp == 1 && r.cps[0].x == x, "practice on mid-run keeps going with a checkpoint here");
    run_set_practice(&r, 0);
    check(!r.practice && r.ncp == 0 && r.b.x < TILE && r.attempt == 2, "practice off restarts the level");
}

// --- Solver: proves a level can be finished --------------------------------
// The search itself is tools/solver.c (shared with tools/render.c).

static Level blv_scratch;

// Built-in levels allowed to pass comfort at 5 of 6 grid offsets instead of 6.
// Empty: a level goes here only when the main session agrees (a DEMON at most).
static const char *const COMFORT_FIVE[] = { NULL };

static int comfort_five(const char *id)
{
    for (int i = 0; COMFORT_FIVE[i]; i++)
        if (!strcmp(COMFORT_FIVE[i], id))
            return 1;
    return 0;
}

// How high the player's feet may get: a jump (or an orb) above the highest
// object, a yellow pad's launch above a pad, or a flying corridor's ceiling.
// Higher means the run went over the level, like a flipped cube with no roof
// running along the top of the world.
#define SIM_WORLD_TOP (40.0f * TILE)    // play.c's WORLD_TOP

static float level_ceiling(const Level *lv)
{
    float rise[MAX_GROUPS + 1] = { 0 }, top = 0;
    for (int i = 0; i < lv->nobj; i++)
        if (lv->objs[i].type == OBJ_MOVE && lv->objs[i].dy > 0 && lv->objs[i].group <= MAX_GROUPS)
            rise[lv->objs[i].group] += (float)(lv->objs[i].dy * TILE);
    for (int i = 0; i < lv->nobj; i++) {
        const LevelObj *o = &lv->objs[i];
        if ((o->type >= OBJ_DECO_GLOW && o->type <= OBJ_DECO_SPIKES) || o->type == OBJ_SIGN || o->type == OBJ_COLOR ||
            o->type == OBJ_MOVE || o->type == OBJ_PULSE || o->type == OBJ_PULSE_BARS)
            continue;
        float t = (float)((o->y + 1) * TILE) + (obj_grouped(o) ? rise[o->group] : 0) +
                  (o->type == OBJ_PAD_YELLOW ? 4.2f : 2.5f) * TILE;
        int m = run_portal_mode(o->type);
        if (m > MODE_CUBE) {
            float lo, hi;
            run_corridor(m, o, &lo, &hi);
            t = hi > t ? hi : t;
        }
        top = t > top ? t : top;
    }
    return top;
}

// Plays the solver's trace: 1 if the feet went above the ceiling or a cube
// touched the top of the world. *feet_max is the highest the feet got.
static int over_the_top(const Level *lv, float ceiling, float *feet_max)
{
    Run r;
    int top = 0;
    run_start(&r, lv, 0, 0);
    *feet_max = 0;
    for (int t = 0; t < ntrace && r.state == RUN_PLAY; t++) {
        run_step(&r, trace[t]);
        float hh = run_half_h(&r.b);
        if (r.b.y - hh > *feet_max)
            *feet_max = r.b.y - hh;
        top |= r.b.mode == MODE_CUBE && r.b.y + hh >= SIM_WORLD_TOP - 1;
    }
    return *feet_max > ceiling || top;
}

// A flipped stretch with no roof: the cube falls to the top of the world and
// runs over everything to the end. The check fails it, and passes the same
// stretch with a roof and a gravity portal down under it.
static void test_over_the_top(void)
{
    static Level open, roofed;
    float feet_open = 0, feet_roofed = 0;
    open = *MK(1, "....................................",
                  "...........................g........",
                  "....................................",
                  "....................................",
                  "....................................",
                  ".....G.......^^^^^^^^^^^.........|");
    roofed = *MK(1, ".....##########################.....",
                    "...........................g........",
                    "....................................",
                    "....................................",
                    "....................................",
                    ".....G.......^^^^^^^^^^^.........|");
    int skipped = solve(&open, 2, 0, 50000000L) == 1 && over_the_top(&open, level_ceiling(&open), &feet_open);
    int kept = solve(&roofed, 2, 0, 50000000L) == 1 && !over_the_top(&roofed, level_ceiling(&roofed), &feet_roofed);
    char what[200];
    snprintf(what, sizeof(what), "the over-the-top check fails a flipped stretch with no roof (feet %.1f cells, limit %.1f) "
             "and passes it with a roof (feet %.1f, limit %.1f)", feet_open / TILE, level_ceiling(&open) / TILE,
             feet_roofed / TILE, level_ceiling(&roofed) / TILE);
    check(skipped && kept, what);
}

static void egg_proofs(const Level *lv, const char *label);

static void solve_level(const Level *lv, const char *label, int builtin)
{
    char what[160];
    double t0 = (double)clock() / CLOCKS_PER_SEC;
    int rc = solve(lv, 2, 0, 400000000L);
    int ok = rc == 1 && replay(lv, trace, ntrace);
    snprintf(what, sizeof(what), "%s can be finished (%.1f s long, solved in %.1f s)", label,
             ntrace / 60.0, (double)clock() / CLOCKS_PER_SEC - t0);
    check(ok, what);
    // Comfort: still finishable when presses can only change every 6 frames
    // (100 ms), whichever frame that grid starts on.
    if (!ok)
        return;     // the comfort and coin searches would only use up their budgets
    float ceiling = level_ceiling(lv), feet, feet_max = 0;
    int over = builtin && over_the_top(lv, ceiling, &feet_max);
    int phases = 0;
    for (int p = 0; p < 6; p++) {
        int got = solve(lv, 6, p ? p : 6, 200000000L) == 1;
        phases += got;
        if (got && builtin) {
            over |= over_the_top(lv, ceiling, &feet);
            feet_max = feet > feet_max ? feet : feet_max;
        }
    }
    printf("     comfort: finishable on a 100 ms press grid for %d of 6 grid offsets\n", phases);
    if (builtin) {
        snprintf(what, sizeof(what), "%s stays inside the level: no skipping over the top (feet at most %.1f cells, limit %.1f)",
                 label, feet_max / TILE, ceiling / TILE);
        check(!over, what);
    }
    if (builtin || strstr(label, "samples/")) {
        int five = comfort_five(label);
        snprintf(what, sizeof(what), "%s keeps comfort %s", label, five ? "5/6 (allowed)" : "6/6");
        check(phases == 6 || (five && phases == 5), what);
    }
    // Every secret coin can be picked up on a run that still finishes.
    if (lv->ncoins) {
        int each = 0;
        for (int k = 0; k < lv->ncoins; k++) {
            want_coins(lv, 1 << k);
            each += solve(lv, 2, 0, 400000000L) == 1 && replay(lv, trace, ntrace);
        }
        want_coins(lv, (1 << lv->ncoins) - 1);
        int all = solve(lv, 2, 0, 400000000L) == 1 && replay(lv, trace, ntrace);
        want_coins(lv, 0);
        snprintf(what, sizeof(what), "%s: each of its %d coins can be collected on a finishing run (all in one run: %s)",
                 label, lv->ncoins, all ? "yes" : "no");
        check(each == lv->ncoins, what);
    }
    egg_proofs(lv, label);
    if (ok)
        solve(lv, 2, 0, 400000000L);    // leave the fine trace for the caller
}

// --- 3.2 mode framework -----------------------------------------------------

int trace_digest(const Level *lv, char *out, int max);
static void press(Game *g, int bits);
static void presses(Game *g, int bits, int n);

// Every built-in level the mode framework left unchanged plays exactly as
// before it: the same solver traces and the same body on every frame
// (tools/traces_base.txt, from commit c0e8de4).
static void test_traces(void)
{
    static char base[1 << 16], line[4096];
    static Level lv;
    const char *path = getenv("SIM_TRACES");
    int n = path ? read_text(path, base, sizeof(base)) : -1;
    if (n <= 0) {
        printf("NOTE: no SIM_TRACES file, old levels not compared\n");
        return;
    }
    int same = 0, differ = 0, changed = 0;
    for (int i = 0; i < BUILTIN_COUNT; i++) {
        level_parse(&lv, BUILTIN_JSON[i], (int)strlen(BUILTIN_JSON[i]));
        char want[4096] = "", hash[16];
        snprintf(hash, sizeof(hash), "%08x", (unsigned)level_hash(&lv));
        for (char *p = base; p && *p; p = strchr(p, '\n') ? strchr(p, '\n') + 1 : NULL) {
            size_t k = strlen(BUILTIN_IDS[i]);
            if (!strncmp(p, BUILTIN_IDS[i], k) && p[k] == ' ') {
                size_t len = strcspn(p + k + 1, "\n");
                snprintf(want, sizeof(want), "%.*s", (int)len, p + k + 1);
            }
        }
        if (!*want || strncmp(want, hash, 8)) {
            printf("     %s: changed since the baseline, not compared\n", BUILTIN_IDS[i]);
            changed++;
            continue;
        }
        trace_digest(&lv, line, sizeof(line));
        if (!strcmp(line, want)) {
            same++;
        } else {
            differ++;
            printf("     %s differs:\n       was %s\n       now %s\n", BUILTIN_IDS[i], want, line);
        }
    }
    char what[160];
    snprintf(what, sizeof(what), "%d unchanged built-in levels replay the same solver traces as before the mode "
             "framework (%d differ, %d changed levels skipped)", same, differ, changed);
    check(same > 0 && differ == 0, what);
}

static void test_mode_table(void)
{
    int ok = 1, built = 0;
    for (int m = 0; m < MODE_COUNT; m++) {
        const ModeDef *d = MODES[m];
        if (!d)
            continue;
        built++;
        char pn[40];
        snprintf(pn, sizeof(pn), "portal_%s", d->name);
        int portal = run_mode_portal(m);
        int good = d->label && !strcmp(pn, d->portal_name) && mode_from_name(d->name) == m &&
                   obj_type_from_name(d->portal_name) == portal && run_portal_mode(portal) == m &&
                   !strcmp(obj_name(portal), d->portal_name) && obj_kind(portal) == K_PORTAL && d->step && d->turn &&
                   d->draw && d->half_w > 0 && d->half_h > 0 && (d->inputs & IN_CROSS) && d->noutline >= 3 &&
                   d->noutline <= 8;
        if (!good)
            printf("     mode %d (%s) is not registered right\n", m, d->name);
        ok &= good;
    }
    for (int t = 1; t < OBJ_COUNT; t++) {
        const ObjDef *d = obj_def(t);
        if (d && (obj_type_from_name(d->name) != t || !d->label || d->cat < 0 || d->cat >= PAL_COUNT)) {
            printf("     object %d (%s) is not registered right\n", t, d->name);
            ok = 0;
        }
    }
    char what[96];
    snprintf(what, sizeof(what), "the mode table: %d modes, each with its name, portal, physics and art", built);
    check(ok && built >= 5 && MODES[MODE_CUBE] && MODES[MODE_WAVE], what);
}

// A portal plays a 12 frame morph (drawing only, with a sound), and every
// vehicle carries his cube with its face as the pilot.
static void test_morph(void)
{
    MK(1, "..........................................",
          "..........S.........B..........U...........",
          "..........................................");
    Run r;
    begin(&r, 0);
    int sfx = 0, t0 = -1, frames = 0;
    Look lk = { { 3, 2, 1, 1, 0, 0 }, 0 };
    look_set(&lk);
    while (r.state == RUN_PLAY && r.b.mode == MODE_CUBE) {
        run_step(&r, 0);
        sfx = r.sfx;
    }
    t0 = r.b.morph_t;
    static const char *const NAMES[5] = { "40_morph_t00", "40b_morph_t03", "40c_morph_t06", "40d_morph_t09",
                                          "40e_morph_done" };
    for (int k = 0; k < 5; k++) {
        shot_run(&r, NAMES[k]);
        for (int f = 0; f < 3 && r.state == RUN_PLAY; f++, frames++)
            run_step(&r, (frames / 8) % 2 ? IN_CROSS : 0);
    }
    check(r.b.mode == MODE_SHIP && (sfx & SFX_MORPH) && (sfx & SFX_PORTAL) && t0 == 0 && r.b.morph_t == MORPH_FRAMES,
          "a mode portal starts a 12 frame morph with its sound");
    while (r.state == RUN_PLAY && r.b.mode == MODE_SHIP)
        run_step(&r, r.frame % 20 < 9 ? IN_CROSS : 0);
    for (int f = 0; f < 20 && r.state == RUN_PLAY; f++)
        run_step(&r, 0);
    shot_run(&r, "40f_ball_pilot");
    while (r.state == RUN_PLAY && r.b.mode == MODE_BALL)
        run_step(&r, 0);
    for (int f = 0; f < 20 && r.state == RUN_PLAY; f++)
        run_step(&r, f % 10 == 0 ? IN_CROSS : 0);
    shot_run(&r, "40g_ufo_pilot");
    check(r.b.mode == MODE_UFO && r.state == RUN_PLAY, "the ball and UFO portals morph too");
    Look plain = { { 0, 1, 0, 0, 0, 0 }, 0 };
    look_set(&plain);
}

// The mode a run is in once it is past cell x (no presses).
static int mode_after(const Level *lv, int x, int *t_touch)
{
    Run r;
    run_start(&r, lv, 0, 0);
    int was = r.b.mode;
    *t_touch = -1;
    while (r.state == RUN_PLAY && r.b.x < (float)x * TILE) {
        run_step(&r, 0);
        if (r.b.mode != was && *t_touch < 0)
            *t_touch = r.b.t;
    }
    return r.state == RUN_PLAY ? r.b.mode : -1;
}

// SHIFTER and MYSTERY portals: by the frame clock, the same every attempt,
// in practice after a checkpoint, and for the solver.
static void test_shifter(void)
{
    static Level lv;
    static char json[2048], text[8192];
    char what[200];
    int got[MODE_COUNT] = { 0 }, ok = 1, kinds = 0;
    for (int x = 10; x < 34; x++) {
        int len = snprintf(json, sizeof(json), "{\"song\":3,\"objects\":[{\"t\":\"shifter\",\"x\":%d,\"y\":1,"
                           "\"m\":[\"ship\",\"ufo\",\"ball\"]},{\"t\":\"end\",\"x\":60,\"y\":0}]}", x);
        level_parse(&lv, json, len);
        int t, m = mode_after(&lv, x + 2, &t);
        Run r;
        run_start(&r, &lv, 0, 0);
        int lit = (run_beat_at(&r, t) / 1) % 3, want = lit == 0 ? MODE_SHIP : lit == 1 ? MODE_UFO : MODE_BALL;
        ok &= lv.timed && t > 0 && m == want;
        kinds += !got[m]++;
    }
    snprintf(what, sizeof(what), "a SHIFTER turns the player into the mode lit on the beat as he enters (%d of 3 modes "
             "seen over 24 places)", kinds);
    check(ok && kinds == 3, what);

    int same = 1, picks = 0, seen[MODE_COUNT] = { 0 };
    for (int x = 10; x < 34; x++) {
        int len = snprintf(json, sizeof(json), "{\"objects\":[{\"t\":\"mystery\",\"x\":%d,\"y\":1,"
                           "\"m\":[\"ship\",\"ufo\",\"ball\",\"wave\"]},{\"t\":\"end\",\"x\":60,\"y\":0}]}", x);
        level_parse(&lv, json, len);
        int t1, t2, a = mode_after(&lv, x + 2, &t1), b = mode_after(&lv, x + 2, &t2);
        same &= a == b && a > 0;
        picks += !seen[a]++;
    }
    snprintf(what, sizeof(what), "a MYSTERY picks the same mode every attempt and different ones in different places "
             "(%d of 4 seen)", picks);
    check(same && picks >= 3, what);

    // Saved and loaded with their lists; the list is part of the level hash.
    int len = snprintf(json, sizeof(json), "{\"objects\":[{\"t\":\"shifter\",\"x\":20,\"y\":1,\"c\":2,"
                       "\"m\":[\"ship\",\"nothing\",\"wave\"]},{\"t\":\"end\",\"x\":60,\"y\":0}]}");
    level_parse(&lv, json, len);
    uint32_t h1 = level_hash(&lv);
    int n = level_write(&lv, text, sizeof(text));
    static Level back;
    level_parse(&back, text, n);
    const LevelObj *o = &back.objs[0];
    int round = o->type == OBJ_SHIFTER && o->param == 2 && o->list[0] == MODE_SHIP + 1 && o->list[1] == MODE_WAVE + 1 &&
                o->list[2] == 0 && strstr(text, "\"m\":[\"ship\",\"wave\"]") && level_hash(&back) == h1;
    back.objs[0].list[1] = MODE_UFO + 1;
    check(round && level_hash(&back) != h1, "a portal's mode list saves and loads (unknown modes left out) and is in the hash");

    // Practice: a checkpoint before the portal gives the same mode after a death.
    len = snprintf(json, sizeof(json), "{\"objects\":[{\"t\":\"shifter\",\"x\":25,\"y\":1,"
                   "\"m\":[\"ship\",\"ufo\",\"ball\"]},{\"t\":\"spike\",\"x\":40,\"y\":0},"
                   "{\"t\":\"spike\",\"x\":40,\"y\":1},{\"t\":\"spike\",\"x\":40,\"y\":2},"
                   "{\"t\":\"spike\",\"x\":40,\"y\":3},{\"t\":\"spike\",\"x\":40,\"y\":4},"
                   "{\"t\":\"spike\",\"x\":40,\"y\":5},{\"t\":\"spike\",\"x\":40,\"y\":6},"
                   "{\"t\":\"spike\",\"x\":40,\"y\":7},{\"t\":\"spike\",\"x\":40,\"y\":8},"
                   "{\"t\":\"spike\",\"x\":40,\"y\":9},{\"t\":\"end\",\"x\":60,\"y\":0}]}");
    level_parse(&lv, json, len);
    Run r;
    run_start(&r, &lv, 1, 0);
    int first = -1, again = -1;
    while (r.state == RUN_PLAY && r.b.x < 15 * TILE)
        run_step(&r, 0);
    run_step(&r, IN_SQUARE);
    while (r.state == RUN_PLAY) {
        run_step(&r, 0);
        if (r.b.x > 27 * TILE && first < 0)
            first = r.b.mode;
    }
    while (r.state != RUN_PLAY)
        run_step(&r, 0);
    while (r.state == RUN_PLAY && again < 0) {
        run_step(&r, 0);
        if (r.b.x > 27 * TILE)
            again = r.b.mode;
    }
    snprintf(what, sizeof(what), "after a practice death the SHIFTER gives the same mode again (%d, %d)", first, again);
    check(first > 0 && first == again, what);

    // In play the lit mode shows inside the portal; the mystery shows "?".
    len = snprintf(json, sizeof(json), "{\"name\":\"SHIFT\",\"objects\":[{\"t\":\"shifter\",\"x\":14,\"y\":1,"
                   "\"m\":[\"ship\",\"ufo\",\"ball\",\"wave\"]},{\"t\":\"mystery\",\"x\":20,\"y\":1,"
                   "\"m\":[\"ship\",\"ufo\"]},{\"t\":\"end\",\"x\":60,\"y\":0}]}");
    level_parse(&lv, json, len);
    run_start(&r, &lv, 0, 0);
    for (int f = 0; f < 20; f++)
        run_step(&r, 0);
    shot_run(&r, "41_shifter_mystery");
    for (int f = 0; f < 15; f++)
        run_step(&r, 0);
    shot_run(&r, "41b_shifter_next_beat");

    // The editor: the SHIFTER brush, placed with three modes, and Triangle on
    // it opens its settings; the change goes straight into it.
    static Game g;
    reset_data();
    game_init(&g, 9);
    game_step(&g, 0);
    g.title_sel = TITLE_EDITOR;
    press(&g, IN_CROSS);
    press(&g, IN_CROSS);
    for (int k = 0; k < 200 && editor_brush_type(&g.ed) != OBJ_SHIFTER; k++)
        press(&g, IN_R1);
    presses(&g, IN_RIGHT, 6);
    press(&g, IN_UP);
    press(&g, IN_CROSS);
    for (int f = 0; f < 20; f++)
        game_step(&g, 0);
    shot_game(&g, "41c_editor_shifter");
    press(&g, IN_TRIANGLE);
    int panel = g.ed.screen == ED_OBJSET;
    press(&g, IN_RIGHT);                            // mode 1: ship -> ball
    presses(&g, IN_DOWN, 3);
    press(&g, IN_RIGHT);                            // mode 4: none -> cube
    presses(&g, IN_DOWN, 1);
    press(&g, IN_RIGHT);                            // every 2 beats
    shot_game(&g, "41d_editor_shifter_settings");
    press(&g, IN_CIRCLE);
    int back_ed = g.ed.screen == ED_EDIT;
    press(&g, IN_R2);
    presses(&g, IN_DOWN, 10);
    press(&g, IN_CROSS);
    n = plat_read_file(LEVELS_DIR "/slot01.json", text, (int)sizeof(text));
    int at = -1;
    for (int i = 0; n > 0 && level_parse(&lv, text, n) == LEVEL_OK && i < lv.nobj; i++)
        if (lv.objs[i].type == OBJ_SHIFTER)
            at = i;
    const LevelObj *so = at >= 0 ? &lv.objs[at] : NULL;
    snprintf(what, sizeof(what), "the editor places a SHIFTER and Triangle sets its modes and beats (list %d %d %d %d, "
             "every %d)", so ? so->list[0] : -1, so ? so->list[1] : -1, so ? so->list[2] : -1, so ? so->list[3] : -1,
             so ? so->param : -1);
    check(so && so->type == OBJ_SHIFTER && panel && back_ed && so->list[0] == MODE_BALL + 1 &&
          so->list[1] == MODE_UFO + 1 && so->list[3] == MODE_CUBE + 1 && so->param == 2, what);
}

// Level transformations on the TWIST AND TURN sample: what each trigger
// does, that none of the drawing ones change a single frame of physics, that
// morph blocks open its wall on the beat, and what they cost to draw.
static void test_transform(int nsamples, char **samples)
{
    static Level lv, plain;
    static char text[1 << 20], d1[4096], d2[4096];
    char what[240];
    const char *path = NULL;
    for (int i = 0; i < nsamples; i++)
        if (strstr(samples[i], "transform_tour"))
            path = samples[i];
    int n = path ? read_text(path, text, sizeof(text)) : -1;
    if (n <= 0 || level_parse(&lv, text, n) != LEVEL_OK) {
        check(0, "the TWIST AND TURN sample loads");
        return;
    }
    // The drawing-only triggers taken out leave exactly the same solutions.
    plain = lv;
    for (int i = plain.nobj - 1; i >= 0; i--) {
        int t = plain.objs[i].type;
        if (t >= OBJ_MIRROR && t <= OBJ_INVERT ? 1 : t == OBJ_THEME)
            level_remove(&plain, i);
    }
    level_update_end(&plain);
    trace_digest(&lv, d1, sizeof(d1));
    trace_digest(&plain, d2, sizeof(d2));
    check(!strcmp(d1 + 8, d2 + 8) && plain.nobj == lv.nobj - 14,
          "mirror, flip, zoom, shake, rainbow, invert and theme triggers change no physics: the same solver traces");
    // Morph blocks open the wall: with its trigger gone the level cannot be finished.
    static Level shut;
    shut = lv;
    for (int i = shut.nobj - 1; i >= 0; i--)
        if (shut.objs[i].type == OBJ_MORPH_BLOCKS && shut.objs[i].group == 1)
            level_remove(&shut, i);
    level_update_end(&shut);
    int open = solve(&lv, 2, 0, 400000000L) == 1, closed = solve(&shut, 2, 0, 100000000L);
    check(open && closed == 0, "the wall only opens because its morph blocks turn into air");

    // Along the solved run: each effect's state, screenshots, and draw time.
    solve(&lv, 2, 0, 400000000L);
    static const struct { int x, after; const char *name; } SHOTS[] = {
        { 20, 10, "42_mirror_turning" }, { 20, 40, "42b_mirror" }, { 70, 40, "42c_flip" }, { 120, 45, "42d_zoom_far_out" },
        { 150, 45, "42e_zoom_close_up" }, { 184, 10, "42f_shake" }, { 200, 40, "42g_rainbow" }, { 240, 40, "42h_invert" },
        { 274, 18, "42i_theme_sweep" }, { 274, 60, "42j_theme_night" }, { 306, 50, "42k_theme_day" },
        { 326, 12, "42l_morph_warning" }, { 342, -8, "42m_morph_open" }, { 352, 20, "42n_morph_beat" },
    };
    double ms[8] = { 0 }, worst[8] = { 0 };
    int cnt[8] = { 0 };
    static const char *const KIND[8] = { "plain", "mirror/flip turning", "mirrored or flipped", "zoomed out",
                                         "zoomed in", "rainbow or invert", "theme sweep", "morph blocks" };
    int ok_mirror = 0, ok_flip = 0, ok_zoom = 0, ok_shake = 0, ok_rb = 0, ok_inv = 0, ok_theme = 0, ok_mb = 0;
    Run r;
    run_start(&r, &lv, 0, 0);
    int fired[16] = { 0 };
    for (int t = 0; t < ntrace && r.state == RUN_PLAY; t++) {
        run_step(&r, trace[t]);
        const Transform *f = &r.b.tf;
        int cell = (int)(r.b.x / TILE);
        ok_mirror |= cell == 40 && f->mirror && f->mirror_t == TF_EASE;
        ok_flip |= cell == 90 && f->flip && f->flip_t == TF_EASE && !f->mirror && f->mirror_t == 0;
        ok_zoom |= cell == 140 && transform_zoom(&r.b) < 0.66f;
        ok_shake |= cell == 186 && f->shake_t > 0;
        ok_rb |= cell == 220 && f->rainbow_t == TF_EASE;
        ok_inv |= cell == 250 && f->invert_t == TF_EASE && f->rainbow_t == 0;
        ok_theme |= cell == 290 && f->theme_to == 2 && r.b.col_to.top == THEMES[2].colors.top;
        ok_mb |= cell == 343 && r.b.mb_to[0] == MB_AIR;
        for (int k = 0; k < (int)(sizeof(SHOTS) / sizeof(SHOTS[0])); k++) {
            int at = (int)((SHOTS[k].x + 0.5f) * TILE + (float)SHOTS[k].after * run_speed_px(r.b.speed));
            if (!fired[k] && r.b.x >= (float)at) {
                fired[k] = 1;
                shot_run(&r, SHOTS[k].name);
            }
        }
        float z = transform_zoom(&r.b);
        int kind = f->theme_t < SWEEP_FRAMES && f->theme_from != f->theme_to ? 6 :
                   (f->mirror_t > 0 && f->mirror_t < TF_EASE) || (f->flip_t > 0 && f->flip_t < TF_EASE) ? 1 :
                   f->mirror_t || f->flip_t ? 2 : z < 0.99f ? 3 : z > 1.01f ? 4 : f->rainbow_t || f->invert_t ? 5 :
                   r.b.mb_to[0] || r.b.mb_to[1] ? 7 : 0;
        draw_set_target(fb);
        clock_t c0 = clock();
        run_draw(&r, HUD_PROGRESS | HUD_TEXT);
        double dt = (double)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
        ms[kind] += dt;
        cnt[kind]++;
        if (dt > worst[kind])
            worst[kind] = dt;
    }
    snprintf(what, sizeof(what), "each transformation acts at its trigger (mirror %d flip %d zoom %d shake %d rainbow %d "
             "invert %d theme %d morph %d)", ok_mirror, ok_flip, ok_zoom, ok_shake, ok_rb, ok_inv, ok_theme, ok_mb);
    check(ok_mirror && ok_flip && ok_zoom && ok_shake && ok_rb && ok_inv && ok_theme && ok_mb && r.state == RUN_DONE, what);
    double base = cnt[0] ? ms[0] / cnt[0] : 1, slowest = 0;
    for (int k = 0; k < 8; k++)
        if (cnt[k]) {
            printf("     draw time %-20s avg %.2f ms, max %.2f ms over %d frames\n", KIND[k], ms[k] / cnt[k], worst[k], cnt[k]);
            if (ms[k] / cnt[k] > slowest)
                slowest = ms[k] / cnt[k];
        }
    snprintf(what, sizeof(what), "transformed frames cost at most %.2fx a plain frame to draw", slowest / base);
    check(slowest < base * 2.0, what);

    // Practice: a checkpoint while mirrored comes back mirrored.
    run_start(&r, &lv, 1, 0);
    for (int t = 0; t < ntrace && r.b.x < 45 * TILE; t++)
        run_step(&r, trace[t]);
    run_step(&r, IN_SQUARE | trace[0]);
    while (r.state == RUN_PLAY)
        run_step(&r, 0);
    while (r.state != RUN_PLAY)
        run_step(&r, 0);
    check(r.b.tf.mirror && r.b.tf.mirror_t == TF_EASE, "a practice checkpoint keeps the screen mirrored");

    // The editor: all eight in the TRANSFORM group, marked on the map, and
    // MORPH BLOCKS' settings from its brush.
    static Game g;
    reset_data();
    write_text(LEVELS_DIR "/slot01.json", "{\"name\":\"TWISTY\",\"objects\":[{\"t\":\"mirror\",\"x\":6,\"y\":0,\"c\":1},"
               "{\"t\":\"zoom\",\"x\":8,\"y\":1,\"c\":4},{\"t\":\"theme_morph\",\"x\":10,\"y\":0,\"c\":2},"
               "{\"t\":\"morph_blocks\",\"x\":12,\"y\":0,\"g\":2,\"c\":1,\"dx\":0,\"dy\":0,\"f\":2},"
               "{\"t\":\"block\",\"x\":14,\"y\":2,\"g\":2},{\"t\":\"end\",\"x\":40,\"y\":0}]}");
    game_init(&g, 10);
    game_step(&g, 0);
    g.title_sel = TITLE_EDITOR;
    press(&g, IN_CROSS);
    press(&g, IN_CROSS);
    int seen = 0;
    for (int k = 0; k < 300; k++) {
        int t = editor_brush_type(&g.ed);
        if (t >= OBJ_MIRROR && t <= OBJ_THEME)
            seen |= 1 << (t - OBJ_MIRROR);
        press(&g, IN_R1);
    }
    while (editor_brush_type(&g.ed) != OBJ_MORPH_BLOCKS)
        press(&g, IN_R1);
    g.ed.cx = 20;
    g.ed.cy = 3;
    for (int f = 0; f < 20; f++)
        game_step(&g, 0);
    shot_game(&g, "42o_editor_transform");
    press(&g, IN_TRIANGLE);
    int panel = g.ed.screen == ED_OBJSET && g.ed.obj_edit == -1;
    press(&g, IN_RIGHT);                            // group 2
    presses(&g, IN_DOWN, 2);
    press(&g, IN_RIGHT);                            // every beat
    shot_game(&g, "42p_editor_morph_settings");
    press(&g, IN_CIRCLE);
    press(&g, IN_CROSS);
    press(&g, IN_R2);
    presses(&g, IN_DOWN, 10);
    press(&g, IN_CROSS);
    n = plat_read_file(LEVELS_DIR "/slot01.json", text, (int)sizeof(text));
    static Level ed;
    int put = n > 0 && level_parse(&ed, text, n) == LEVEL_OK ? level_find(&ed, 20, 3) : -1;
    snprintf(what, sizeof(what), "the TRANSFORM group has all 8 triggers (%02x) and a new MORPH BLOCKS gets its brush "
             "settings", seen);
    check(seen == 0xFF && panel && put >= 0 && ed.objs[put].type == OBJ_MORPH_BLOCKS && ed.objs[put].group == 2 &&
          ed.objs[put].time == 1 && !obj_grouped(&ed.objs[put]) && ed.nmoving == 1, what);
}

// A run on a flat level that has just gone through mode m's portal at
// (3, y) and settled on the floor.
static void into_mode(Run *r, Level *lv, int m, int y)
{
    char json[512];
    int len = snprintf(json, sizeof(json), "{\"objects\":[{\"t\":\"%s\",\"x\":3,\"y\":%d},"
                       "{\"t\":\"block\",\"x\":34,\"y\":4},{\"t\":\"block\",\"x\":35,\"y\":4},"
                       "{\"t\":\"block\",\"x\":36,\"y\":4},{\"t\":\"end\",\"x\":300,\"y\":0}]}",
                       MODES[m]->portal_name, y);
    level_parse(lv, json, len);
    run_start(r, lv, 0, 0);
    while (r->state == RUN_PLAY && (r->b.mode != m || r->b.x < 6 * TILE))
        run_step(r, 0);
    for (int f = 0; f < 30 && r->state == RUN_PLAY; f++)
        run_step(r, 0);
}

// Highest the feet get over the next 90 frames with X held for `hold` frames
// (and pressed again on frame `again`, if not -1), in cells.
static float peak_cells(const Run *r0, int hold, int again)
{
    Run r = *r0;
    float top = 0, base = feet(r0);
    for (int f = 0; f < 90 && r.state == RUN_PLAY; f++) {
        run_step(&r, f < hold || (again >= 0 && f >= again && f < again + 2) ? IN_CROSS : 0);
        if (feet(&r) - base > top)
            top = feet(&r) - base;
    }
    return top / TILE;
}

// The five stage 1 modes: physics, art, portals, and a demo the solver finishes.
static void test_new_modes(int nsamples, char **samples)
{
    static Level lv;
    char what[200];
    Run r, cube;
    Look lk = { { 1, 3, 1, 0, 0, 0 }, 0 };
    look_set(&lk);

    // ROBOT: a tap hops, a full hold climbs about 3.5 cells.
    into_mode(&r, &lv, MODE_ROBOT, 1);
    into_mode(&cube, &lv, MODE_CUBE, 1);
    float tap = peak_cells(&r, 1, -1), half = peak_cells(&r, 5, -1), full = peak_cells(&r, 40, -1);
    float cj = peak_cells(&cube, 1, -1);
    snprintf(what, sizeof(what), "ROBOT: holding X jumps higher (tap %.2f, 5 frames %.2f, held %.2f cells; cube %.2f)",
             tap, half, full, cj);
    check(r.b.mode == MODE_ROBOT && tap < cj && tap < half && half < full && full > 3.3f && full < 3.8f, what);
    for (int f = 0; f < 6; f++)
        run_step(&r, IN_CROSS);
    shot_run(&r, "43_robot_boost");

    // SPIDER: a press snaps to the ceiling, the next back to the floor, and
    // under a shelf it snaps to the shelf's underside; holding does not repeat.
    into_mode(&r, &lv, MODE_SPIDER, 1);
    float hh = run_half_h(&r.b);
    run_step(&r, IN_CROSS);
    int up = r.b.grav < 0 && r.b.y + hh > 8 * TILE - 2;
    for (int f = 0; f < 3; f++)
        run_step(&r, IN_CROSS);
    shot_run(&r, "43b_spider_zip");
    int flips = 0, was = r.b.grav;
    for (int f = 0; f < 30; f++) {
        run_step(&r, IN_CROSS);
        flips += r.b.grav != was;
        was = r.b.grav;
    }
    run_step(&r, 0);
    run_step(&r, IN_CROSS);
    int down = r.b.grav > 0 && r.b.y - hh < 2;
    while (r.state == RUN_PLAY && r.b.x < 35 * TILE)
        run_step(&r, 0);
    run_step(&r, IN_CROSS);
    int shelf = r.b.grav < 0 && r.b.y + hh > 4 * TILE - 2 && r.b.y + hh < 4 * TILE + 2;
    snprintf(what, sizeof(what), "SPIDER: a press snaps it to the ceiling (%d) and back (%d), to a shelf's underside (%d), "
             "and held it does not repeat (%d)", up, down, shelf, flips);
    check(up && down && shelf && flips == 0 && r.state == RUN_PLAY, what);

    // SWING: each press turns gravity round and it swings back; holding does nothing more.
    into_mode(&r, &lv, MODE_SWING, 1);
    int g0 = r.b.grav, turns = 0, alive = 1, dir = 0, swings = 0;
    for (int f = 0; f < 240; f++) {
        int in = f % 40 < 12 ? IN_CROSS : 0;
        int before = r.b.grav;
        run_step(&r, in);
        turns += r.b.grav != before;
        int d = r.b.vy > 0.5f ? 1 : r.b.vy < -0.5f ? -1 : 0;
        swings += d && d != dir;
        if (d)
            dir = d;
        alive &= r.state == RUN_PLAY;
        if (f == 50)
            shot_run(&r, "43c_swing");
    }
    snprintf(what, sizeof(what), "SWING: every press (6) turns gravity round (%d turns, %d swings), safe in its corridor",
             turns, swings);
    check(g0 == 1 && turns == 6 && swings >= 5 && alive, what);

    // JETPACK: softer than the ship.
    Run ship;
    static Level lv2;
    into_mode(&r, &lv, MODE_JETPACK, 1);
    into_mode(&ship, &lv2, MODE_SHIP, 1);
    float j0 = feet(&r), s0 = feet(&ship), jmax = 0;
    for (int f = 0; f < 20; f++) {
        run_step(&r, IN_CROSS);
        run_step(&ship, IN_CROSS);
        if (r.b.vy > jmax)
            jmax = r.b.vy;
    }
    shot_run(&r, "43d_jetpack");
    snprintf(what, sizeof(what), "JETPACK: 20 frames of thrust lift it %.0f px, the ship %.0f px (top speed %.1f)",
             feet(&r) - j0, feet(&ship) - s0, jmax);
    check(feet(&r) - j0 > 0 && feet(&r) - j0 < (feet(&ship) - s0) * 0.8f && jmax <= 10.0f, what);

    // BUNNY: a second press in the air jumps again; a third does not; landing gives it back.
    into_mode(&r, &lv, MODE_BUNNY, 1);
    float one = peak_cells(&r, 1, -1), two = peak_cells(&r, 1, 10);
    Run b3 = r;
    int jumps = 0;
    for (int f = 0; f < 90; f++) {
        int before = b3.jumps;
        run_step(&b3, f == 0 || f == 10 || f == 20 ? IN_CROSS : 0);
        jumps += b3.jumps != before;
        if (f == 15)
            shot_run(&b3, "43e_bunny_flip");
    }
    for (int f = 0; f < 60; f++)
        run_step(&b3, 0);
    float again = peak_cells(&b3, 1, 10);
    snprintf(what, sizeof(what), "BUNNY: a double jump (%.2f cells, single %.2f), never a triple (%d jumps), and back "
             "after landing (%.2f)", two, one, jumps, again);
    check(two > one + 1.0f && jumps == 2 && again > two - 0.05f && again < two + 0.05f, what);

    // Every 3.2 mode that is built has a demo, samples/mode_<name>.txt: the
    // solver finishes it (comfort 6/6 is checked with the samples) and
    // spends a good part of it in that mode.
    static char text[1 << 20];
    static Level demo;
    for (int m = MODE_ROBOT; m < MODE_COUNT; m++) {
        if (!MODES[m])
            continue;
        char want[40];
        snprintf(want, sizeof(want), "mode_%s.json", MODES[m]->name);
        int frames = 0, found = 0;
        for (int i = 0; i < nsamples; i++) {
            if (!strstr(samples[i], want))
                continue;
            int n = read_text(samples[i], text, sizeof(text));
            found = n > 0 && level_parse(&demo, text, n) == LEVEL_OK && solve(&demo, 2, 0, 400000000L) == 1;
            Run d;
            run_start(&d, &demo, 0, 0);
            for (int t = 0; found && t < ntrace; t++) {
                run_step(&d, trace[t]);
                frames += d.b.mode == m;
            }
        }
        snprintf(what, sizeof(what), "the %s demo is finished by the solver with %.1f s in the %s", want, frames / 60.0,
                 MODES[m]->label);
        check(found && frames > 300, what);
    }
    Look plain = { { 0, 1, 0, 0, 0, 0 }, 0 };
    look_set(&plain);
}

// --- Mode pack A (modes 6-15) -----------------------------------------------

// A run on a level of the given objects (JSON items) that starts in mode m,
// a little way in with nothing pressed.
static void pa_start(Run *r, Level *lv, int m, const char *objs)
{
    static char json[1 << 14];
    int len = snprintf(json, sizeof(json), "{\"mode\":\"%s\",\"objects\":[%s%s{\"t\":\"end\",\"x\":400,\"y\":0}]}",
                       MODES[m]->name, objs, objs[0] ? "," : "");
    level_parse(lv, json, len);
    run_start(r, lv, 0, 0);
    for (int f = 0; f < 20 && r->state == RUN_PLAY; f++)
        run_step(r, 0);
}

// Highest the feet get, in cells over base, over n frames of the inputs:
// X held on frames [h0, h1) (and pressed again on frame `again` if >= 0).
static float pa_peak(const Run *r0, int h0, int h1, int again, int n)
{
    Run r = *r0;
    float top = -1e9f, base = feet(r0);
    for (int f = 0; f < n && r.state == RUN_PLAY; f++) {
        run_step(&r, (f >= h0 && f < h1) || (again >= 0 && f >= again && f < again + 6) ? IN_CROSS : 0);
        if (feet(&r) - base > top)
            top = feet(&r) - base;
    }
    return top / TILE;
}

static void pa_run_to(Run *r, float x, int in)
{
    while (r->state == RUN_PLAY && r->b.x < x * TILE)
        run_step(r, in);
}

// A screenshot, and where the player is in it (for close-ups).
static void pa_shot(const Run *r, const char *name)
{
    shot_run(r, name);
    int x, y;
    run_screen_pos(r->b.x, r->b.y, &x, &y);
    printf("     %s: player at %d %d\n", name, x, y);
}

// Modes 6-15 and their objects: physics, art, portals, the editor.
static void test_pack_a(void)
{
    static Level lv, lv2;
    char what[240];
    Run r, s;
    Look lk = { { 1, 3, 1, 0, 0, 0 }, 0 };
    look_set(&lk);

    // GLIDER: holding X sinks slowly, letting go drops fast, a press flaps,
    // and an updraft fan carries him up.
    pa_start(&r, &lv, MODE_GLIDER, "");
    for (int f = 0; f < 72; f++)
        run_step(&r, f % 12 < 6 ? IN_CROSS : 0);
    s = r;
    float y0 = r.b.y, flap = pa_peak(&r, 0, 1, -1, 30);
    for (int f = 0; f < 40; f++) {
        run_step(&r, IN_CROSS);
        run_step(&s, 0);
    }
    float glide = y0 - r.b.y, drop = y0 - s.b.y;
    pa_start(&r, &lv, MODE_GLIDER, "{\"t\":\"fan\",\"x\":14,\"y\":0},{\"t\":\"fan\",\"x\":15,\"y\":0},"
             "{\"t\":\"fan\",\"x\":16,\"y\":0},{\"t\":\"spike\",\"x\":19,\"y\":0},{\"t\":\"spike\",\"x\":20,\"y\":0}");
    float lift = 0, base = feet(&r);
    while (r.state == RUN_PLAY && r.b.x < 24 * TILE) {
        run_step(&r, IN_CROSS);
        if (feet(&r) - base > lift)
            lift = feet(&r) - base;
        if (r.b.x > 16.4f * TILE && r.b.x < 16.4f * TILE + 14)
            pa_shot(&r, "44a_glider_fan");
    }
    snprintf(what, sizeof(what), "GLIDER: held it sinks %.0f px in 40 frames (dropping %.0f), a flap lifts %.2f cells, "
             "fans carry it %.2f cells up over the spikes", glide, drop, flap, lift / TILE);
    check(glide > 40 && glide < drop * 0.35f && flap > 0.5f && flap < 1.2f && lift > 2.5f * TILE && r.state == RUN_PLAY,
          what);

    // FROG: three charge steps (a hop, a jump, a leap); held in the air it
    // does nothing until it lands.
    pa_start(&r, &lv, MODE_FROG, "");
    float hop = pa_peak(&r, 0, 6, -1, 90), mid = pa_peak(&r, 0, 18, -1, 90), big = pa_peak(&r, 0, 30, -1, 90);
    float hop2 = pa_peak(&r, 0, 11, -1, 90), longer = pa_peak(&r, 0, 90, -1, 120);
    s = r;
    for (int f = 0; f < 18; f++)
        run_step(&s, IN_CROSS);
    for (int f = 0; f < 10; f++)
        run_step(&s, 0);
    Run t = s;
    float air = 0;
    for (int f = 0; f < 40; f++) {
        run_step(&s, f < 8 ? IN_CROSS : 0);
        run_step(&t, 0);
        if (f < 30 && (s.b.y - t.b.y > air || t.b.y - s.b.y > air))
            air = s.b.y > t.b.y ? s.b.y - t.b.y : t.b.y - s.b.y;
    }
    s = r;
    for (int f = 0; f < 26; f++)
        run_step(&s, IN_CROSS);
    pa_shot(&s, "44b_frog_charge");
    snprintf(what, sizeof(what), "FROG: charging 6/11/18/30 frames leaps %.2f/%.2f/%.2f/%.2f cells, held long %.2f, "
             "a press in the air changes nothing (%.0f px)", hop, hop2, mid, big, longer, air);
    check(hop > 1.1f && hop < 1.5f && hop2 > hop - 0.01f && hop2 < hop + 0.01f && mid > 2.3f && mid < 2.7f && big > 4.0f &&
          big < 4.4f && longer > big - 0.01f && longer < big + 0.01f && air < 0.01f, what);

    // DASHER: a press in the air dashes level and fast, once per jump; a
    // crystal gives it back.
    pa_start(&r, &lv, MODE_DASHER, "");
    pa_start(&s, &lv2, MODE_DASHER, "{\"t\":\"dash_crystal\",\"x\":16,\"y\":2}");
    Run jump = r;
    float x0 = r.b.x;
    int level = 1, dashes = 0;
    for (int f = 0; f < 60 && r.state == RUN_PLAY; f++) {
        int in = f < 2 || (f >= 12 && f < 18) || (f >= 30 && f < 36) ? IN_CROSS : 0;
        float yb = r.b.y;
        int was = r.b.ms[0];
        run_step(&r, in);
        run_step(&jump, f < 2 ? IN_CROSS : 0);
        dashes += r.b.ms[0] > 0 && !was;
        if (was > SUB && r.b.ms[0] > 0)
            level &= r.b.y == yb;
        if (f == 18)
            pa_shot(&r, "44c_dasher_dash");
    }
    float extra = (r.b.x - x0) - (jump.b.x - x0);
    int with = 0;
    pa_run_to(&s, 12.0f, 0);
    for (int f = 0; f < 60 && s.state == RUN_PLAY; f++) {
        int was = s.b.ms[0];
        run_step(&s, f < 2 || (f >= 12 && f < 18) || (f >= 30 && f < 36) ? IN_CROSS : 0);
        with += s.b.ms[0] > 0 && !was;
    }
    snprintf(what, sizeof(what), "DASHER: a dash goes %.2f cells further and stays level (%d), one per jump (%d), two "
             "with a crystal (%d)", extra / TILE, level, dashes, with);
    check(extra > 2.4f * TILE && extra < 3.0f * TILE && level && dashes == 1 && with == 2, what);

    // BLINK: a press jumps 3 cells ahead, through a thin wall and over
    // spikes, stops short of a thick wall, and blinks once in the air.
    pa_start(&r, &lv, MODE_BLINK, "{\"t\":\"block\",\"x\":12,\"y\":0},{\"t\":\"block\",\"x\":12,\"y\":1},"
             "{\"t\":\"spike\",\"x\":20,\"y\":0},{\"t\":\"spike\",\"x\":21,\"y\":0},"
             "{\"t\":\"block\",\"x\":30,\"y\":0},{\"t\":\"block\",\"x\":31,\"y\":0},{\"t\":\"block\",\"x\":32,\"y\":0},"
             "{\"t\":\"block\",\"x\":33,\"y\":0},{\"t\":\"pad_yellow\",\"x\":40,\"y\":0}");
    pa_run_to(&r, 10.6f, 0);
    float b0 = r.b.x;
    run_step(&r, IN_CROSS);
    float jumpd = r.b.x - b0;
    pa_shot(&r, "44d_blink");
    run_step(&r, 0);
    pa_run_to(&r, 19.4f, 0);
    run_step(&r, IN_CROSS);
    pa_run_to(&r, 24.0f, 0);
    int past = r.state == RUN_PLAY;
    pa_run_to(&r, 28.6f, 0);
    run_step(&r, IN_CROSS);
    int shortstop = r.state == RUN_PLAY && r.b.x < 30 * TILE - run_half_w(&r.b) + 20 && r.b.x > 29.4f * TILE;
    pa_start(&r, &lv, MODE_BLINK, "{\"t\":\"pad_yellow\",\"x\":10,\"y\":0}");
    pa_run_to(&r, 10.5f, 0);
    for (int f = 0; f < 6; f++)
        run_step(&r, 0);
    float a0 = r.b.x;
    for (int f = 0; f < 20; f++)
        run_step(&r, f % 12 < 6 ? IN_CROSS : 0);
    float airgo = r.b.x - a0 - 20 * run_speed_px(1);
    snprintf(what, sizeof(what), "BLINK: a press goes %.2f cells (through a wall), over spikes (%d), stops short of a "
             "thick wall (%d), once in the air (%.2f cells extra)", jumpd / TILE, past, shortstop, airgo / TILE);
    check(jumpd > 3.1f * TILE && jumpd < 3.3f * TILE && past && shortstop && airgo > 2.9f * TILE && airgo < 3.1f * TILE,
          what);

    // GRAPPLE: holding X near a ring catches it and swings him down and up;
    // the rope flings him high; with no ring, holding does nothing.
    pa_start(&r, &lv, MODE_GRAPPLE, "{\"t\":\"anchor_ring\",\"x\":16,\"y\":5}");
    s = r;
    int caught = 0;
    float low = 1e9f, high = 0, lowx = 0;
    for (int f = 0; f < 120 && r.state == RUN_PLAY; f++) {
        run_step(&r, r.b.x < 19 * TILE || r.b.ms[0] ? IN_CROSS : 0);
        run_step(&s, 0);
        caught |= r.b.ms[0] != 0;
        if (r.b.ms[0] && r.b.x > 16.5f * TILE && !lowx) {
            low = feet(&r);
            lowx = r.b.x;
        }
        if (feet(&r) > high)
            high = feet(&r);
        if (r.b.ms[0] && r.b.x > 15.0f * TILE && r.b.x < 15.0f * TILE + 14)
            pa_shot(&r, "44e_grapple_swing");
    }
    snprintf(what, sizeof(what), "GRAPPLE: the rope catches the ring (%d), swings under it %.2f cells up (%.1f) and flings "
             "him %.2f cells up; with no press he stays down (%.2f)", caught, low / TILE, lowx / TILE, high / TILE,
             feet(&s) / TILE);
    check(caught && lowx > 16.5f * TILE && lowx < 16.8f * TILE && low > 1.3f * TILE && low < 1.7f * TILE &&
          high > 4.5f * TILE && feet(&s) < 1, what);

    // DRILL: it lands on dirt, dives in, tunnels under spikes, bursts out
    // over a stone and lands on the dirt again. The cube lands on dirt too.
    char objs[8192];
    int len = 0;
    for (int x = 20; x < 46; x++)
        for (int y = 0; y < 2; y++)
            len += snprintf(objs + len, sizeof(objs) - (size_t)len, "{\"t\":\"%s\",\"x\":%d,\"y\":%d},",
                            x == 33 ? "block" : "dirt", x, y);
    for (int x = 26; x < 30; x++)
        len += snprintf(objs + len, sizeof(objs) - (size_t)len, "{\"t\":\"spike\",\"x\":%d,\"y\":2},", x);
    objs[len - 1] = 0;
    pa_start(&r, &lv, MODE_DRILL, objs);
    pa_run_to(&r, 18.5f, 0);
    run_step(&r, IN_CROSS);
    pa_run_to(&r, 24.0f, 0);
    int ontop = r.b.on_ground && feet(&r) > 2 * TILE - 2;
    run_step(&r, IN_CROSS);
    pa_run_to(&r, 27.5f, 0);
    int dug = r.b.ms[0] && feet(&r) < 1;
    pa_shot(&r, "44f_drill_tunnel");
    pa_run_to(&r, 30.5f, 0);
    run_step(&r, IN_CROSS);
    for (int f = 0; f < 8; f++)
        run_step(&r, 0);
    pa_shot(&r, "44f2_drill_burst");
    pa_run_to(&r, 40.0f, 0);
    int again = r.state == RUN_PLAY && r.b.on_ground && feet(&r) > 2 * TILE - 2;
    pa_start(&s, &lv, MODE_CUBE, objs);
    pa_run_to(&s, 18.5f, 0);
    run_step(&s, IN_CROSS);
    pa_run_to(&s, 24.0f, 0);
    int cube = s.b.on_ground && feet(&s) > 2 * TILE - 2;
    snprintf(what, sizeof(what), "DRILL: lands on dirt (%d), dives under the spikes (%d), bursts over the stone onto the "
             "dirt (%d); the cube stands on dirt (%d)", ontop, dug, again, cube);
    check(ontop && dug && again && cube, what);

    // SURFER: floats on the surface; a longer dive throws him higher out.
    len = 0;
    for (int x = 10; x < 70; x++)
        for (int y = 0; y < 3; y++)
            len += snprintf(objs + len, sizeof(objs) - (size_t)len, "{\"t\":\"water\",\"x\":%d,\"y\":%d},", x, y);
    objs[len - 1] = 0;
    pa_start(&r, &lv, MODE_SURFER, objs);
    pa_run_to(&r, 22.0f, 0);
    float ride = feet(&r) - 3 * TILE, still = r.b.vy;
    float dive6 = pa_peak(&r, 0, 6, -1, 90), dive18 = pa_peak(&r, 0, 12, -1, 90), dive36 = pa_peak(&r, 0, 24, -1, 120);
    s = r;
    for (int f = 0; f < 20; f++)
        run_step(&s, IN_CROSS);
    pa_shot(&s, "44g_surfer_dive");
    for (int f = 0; f < 22; f++)
        run_step(&s, 0);
    pa_shot(&s, "44g2_surfer_jump");
    snprintf(what, sizeof(what), "SURFER: rides with its feet %.0f px in the water (vy %.2f); dives of 6/12/24 frames "
             "throw him %.2f/%.2f/%.2f cells up", -ride, still, dive6, dive18, dive36);
    check(ride < -10 && ride > -50 && still > -0.5f && still < 0.5f && dive6 + 0.3f < dive18 && dive18 + 0.3f < dive36 &&
          dive36 > 2.2f, what);

    // BALLOON: rises by itself, sinks while held, and spikes pop it.
    pa_start(&r, &lv, MODE_BALLOON, "{\"t\":\"spike\",\"x\":40,\"y\":9,\"r\":2},{\"t\":\"spike\",\"x\":41,\"y\":9,\"r\":2}");
    float up0 = r.b.y;
    for (int f = 0; f < 40; f++)
        run_step(&r, 0);
    float rose = r.b.y - up0, sk0 = r.b.y;
    for (int f = 0; f < 40; f++)
        run_step(&r, IN_CROSS);
    float sank = sk0 - r.b.y;
    pa_shot(&r, "44h_balloon");
    pa_run_to(&r, 45.0f, 0);
    snprintf(what, sizeof(what), "BALLOON: rises %.0f px in 40 frames on its own, sinks %.0f while held, and pops on "
             "spikes (%d)", rose, sank, r.state == RUN_DEAD);
    check(rose > 80 && sank > 60 && r.state == RUN_DEAD, what);

    // HELI: slower to lift than the jetpack, slow to sink.
    pa_start(&r, &lv, MODE_HELI, "");
    pa_start(&s, &lv2, MODE_JETPACK, "");
    float h0 = feet(&r), j0 = feet(&s), hmax = 0;
    for (int f = 0; f < 20; f++) {
        run_step(&r, IN_CROSS);
        run_step(&s, IN_CROSS);
    }
    float hl = feet(&r) - h0, jl = feet(&s) - j0;
    for (int f = 0; f < 60; f++) {
        run_step(&r, IN_CROSS);
        if (r.b.vy > hmax)
            hmax = r.b.vy;
    }
    pa_shot(&r, "44i_heli");
    float hfall = 0;
    for (int f = 0; f < 90; f++) {
        run_step(&r, 0);
        if (-r.b.vy > hfall)
            hfall = -r.b.vy;
    }
    snprintf(what, sizeof(what), "HELI: 20 frames lift it %.0f px (jetpack %.0f), top speed up %.1f, down %.1f", hl, jl,
             hmax, hfall);
    check(hl > 0 && hl < jl * 0.8f && hmax <= 7.5f && hfall <= 6.5f && hfall > 4, what);

    // POGO: bounces by itself, bounces big when X is held, slams on a press.
    pa_start(&r, &lv, MODE_POGO, "");
    for (int f = 0; f < 40; f++)
        run_step(&r, 0);
    float small = 0, bigb = 0;
    s = r;
    for (int f = 0; f < 60; f++) {
        run_step(&s, 0);
        small = feet(&s) > small ? feet(&s) : small;
    }
    s = r;
    for (int f = 0; f < 90; f++) {
        run_step(&s, IN_CROSS);
        bigb = f > 30 && feet(&s) > bigb ? feet(&s) : bigb;
    }
    small /= TILE;
    bigb /= TILE;
    s = r;
    for (int f = 0; f < 60; f++)
        run_step(&s, IN_CROSS);
    for (int f = 0; f < 200 && !(s.b.vy < 0 && s.b.vy > -3); f++)
        run_step(&s, IN_CROSS);
    run_step(&s, 0);
    run_step(&s, IN_CROSS);
    float slam = s.b.vy;
    run_step(&s, IN_CROSS);
    pa_shot(&s, "44j_pogo_slam");
    snprintf(what, sizeof(what), "POGO: bounces %.2f cells on its own, %.2f held, and a press slams down (vy %.1f)", small,
             bigb, slam);
    check(small > 0.9f && small < 1.3f && bigb > 3.2f && bigb < 3.7f && slam < -25, what);

    // The editor has all five objects and all ten portals.
    static Game g;
    reset_data();
    write_text(LEVELS_DIR "/slot01.json", "{\"name\":\"PACK A\",\"objects\":[{\"t\":\"fan\",\"x\":6,\"y\":0},"
               "{\"t\":\"dirt\",\"x\":8,\"y\":0},{\"t\":\"water\",\"x\":9,\"y\":0},{\"t\":\"anchor_ring\",\"x\":10,\"y\":5},"
               "{\"t\":\"dash_crystal\",\"x\":12,\"y\":2},{\"t\":\"portal_glider\",\"x\":14,\"y\":1},"
               "{\"t\":\"end\",\"x\":40,\"y\":0}]}");
    game_init(&g, 10);
    game_step(&g, 0);
    g.title_sel = TITLE_EDITOR;
    press(&g, IN_CROSS);
    press(&g, IN_CROSS);
    int objs_seen = 0, portals = 0;
    for (int k = 0; k < 400; k++) {
        int t = editor_brush_type(&g.ed);
        if (t >= OBJ_FAN && t <= OBJ_WATER)
            objs_seen |= 1 << (t - OBJ_FAN);
        if (t >= OBJ_PORTAL_GLIDER && t <= OBJ_PORTAL_POGO)
            portals |= 1 << (t - OBJ_PORTAL_GLIDER);
        press(&g, IN_R1);
    }
    while (editor_brush_type(&g.ed) != OBJ_ANCHOR)
        press(&g, IN_R1);
    g.ed.cx = 7;
    g.ed.cy = 3;
    for (int f = 0; f < 20; f++)
        game_step(&g, 0);
    shot_game(&g, "44k_editor_packa");
    snprintf(what, sizeof(what), "the editor has the 5 pack A objects (%02x) and the 10 pack A portals (%03x)", objs_seen,
             portals);
    check(objs_seen == 0x1F && portals == 0x3FF, what);
    Look plain = { { 0, 1, 0, 0, 0, 0 }, 0 };
    look_set(&plain);
}

// --- Mode pack B (modes 16-25) -------------------------------------------------

// A level from a list of JSON objects (with a trailing comma) and an end far
// away; the run starts and goes on, X up, until it has turned into mode m.
static void pb_level(Run *r, Level *lv, const char *objs, int m)
{
    static char json[8192];
    int len = snprintf(json, sizeof(json), "{\"objects\":[%s{\"t\":\"end\",\"x\":300,\"y\":0}]}", objs);
    level_parse(lv, json, len);
    run_start(r, lv, 0, 0);
    while (r->state == RUN_PLAY && r->b.mode != m)
        run_step(r, 0);
}

// Appends objects of type t filling cells x0..x1, y0..y1 to buf.
static void pb_fill(char *buf, size_t n, const char *t, int x0, int y0, int x1, int y1)
{
    for (int x = x0; x <= x1; x++)
        for (int y = y0; y <= y1; y++) {
            size_t len = strlen(buf);
            snprintf(buf + len, n - len, "{\"t\":\"%s\",\"x\":%d,\"y\":%d},", t, x, y);
        }
}

// Steps with input in until the run's centre passes cell column x (or it
// stops playing); 1 if it is still playing.
static int pb_to(Run *r, float x, int in)
{
    while (r->state == RUN_PLAY && r->b.x < x * TILE)
        run_step(r, in);
    return r->state == RUN_PLAY;
}

static void test_packb_physics(void)
{
    static Level lv;
    static char objs[8192];
    char what[220];
    Run r, a;
    Look lk = { { 1, 3, 1, 0, 0, 0 }, 0 };
    look_set(&lk);

    // MAGNET: holding X pulls it up to the ceiling, letting go drops it.
    into_mode(&r, &lv, MODE_MAGNET, 1);
    float hh = run_half_h(&r.b);
    int t_up = -1, t_down = -1;
    for (int f = 0; f < 40; f++) {
        run_step(&r, IN_CROSS);
        if (t_up < 0 && r.b.on_ground && r.b.grav < 0)
            t_up = f;
        if (f == 12)
            shot_run(&r, "44a_magnet_pull");
    }
    int up = r.b.grav < 0 && r.b.on_ground && r.b.y + hh > 8 * TILE - 1;
    shot_run(&r, "44a_magnet_ceiling");
    for (int f = 0; f < 40; f++) {
        run_step(&r, 0);
        if (t_down < 0 && r.b.on_ground && r.b.grav > 0)
            t_down = f;
    }
    int down = r.b.grav > 0 && r.b.on_ground && r.b.y - hh < 1;
    snprintf(what, sizeof(what), "MAGNET: holding X clings to the ceiling (in %d frames), letting go drops to the floor "
             "(%d)", t_up, t_down);
    check(up && down && t_up > 5 && t_up < 32 && t_down > 5 && t_down < 32 && r.state == RUN_PLAY, what);

    // SLIME: it runs up a wall the cube dies on and goes over the top; on a
    // wall with a spike on its face it dies, unless a press jumps it off the
    // wall and over the spike.
    objs[0] = 0;
    strcat(objs, "{\"t\":\"portal_slime\",\"x\":3,\"y\":1},");
    pb_fill(objs, sizeof(objs), "block", 14, 0, 15, 3);
    pb_fill(objs, sizeof(objs), "block", 30, 0, 31, 5);
    strcat(objs, "{\"t\":\"spike\",\"x\":29,\"y\":3,\"r\":3},");
    pb_level(&r, &lv, objs, MODE_SLIME);
    float top = 0, frozen = -1;
    int stuck = 0;
    while (r.state == RUN_PLAY && r.b.x < 18 * TILE) {
        run_step(&r, 0);
        if (r.b.y - run_half_h(&r.b) > top)
            top = r.b.y - run_half_h(&r.b);
        if (r.b.ms[0] && !stuck) {
            stuck = 1;
            frozen = r.b.x;
        }
        if (stuck == 1 && r.b.y > 2 * TILE) {
            stuck = 2;
            shot_run(&r, "44b_slime_climb");
            frozen = r.b.x - frozen;
        }
    }
    int over = r.state == RUN_PLAY && top >= 4 * TILE - 1;
    Run c;
    static Level lc;
    objs[0] = 0;
    pb_fill(objs, sizeof(objs), "block", 14, 0, 15, 3);
    pb_level(&c, &lc, objs, MODE_CUBE);
    int cube_dies = !pb_to(&c, 18, 0);
    snprintf(what, sizeof(what), "SLIME: it runs up a 4 high wall without moving ahead (%.0f px) and over its top (%.2f "
             "cells); the cube dies there (%d)", frozen, top / TILE, cube_dies);
    check(over && stuck == 2 && frozen > -0.5f && frozen < 0.5f && cube_dies, what);
    while (r.state == RUN_PLAY && !r.b.ms[0])
        run_step(&r, 0);
    a = r;
    int no_press = !pb_to(&a, 33, 0) && a.b.y > 2 * TILE;
    int jumped = 0, first = -1;
    for (int wait = 0; wait < 14 && !jumped; wait++) {
        a = r;
        for (int k = 0; k < wait; k++)
            run_step(&a, 0);
        run_step(&a, IN_CROSS);
        Run seen = a;
        jumped = pb_to(&a, 33, 0);
        if (jumped) {
            first = wait;
            for (int k = 0; k < 3; k++)
                run_step(&seen, 0);
            shot_run(&seen, "44b_slime_wall_jump");
        }
    }
    snprintf(what, sizeof(what), "SLIME: a spike on a wall's face kills it (%d) unless a press splat-jumps it up the wall "
             "past the spike (%d, %d frames after it stuck)", no_press, jumped, first);
    check(no_press && jumped, what);

    // CANNON: rolling into a barrel loads it and it waits there; a press
    // fires it along the aim into the next barrel; the fuse fires it too.
    objs[0] = 0;
    strcat(objs, "{\"t\":\"portal_cannon\",\"x\":3,\"y\":1},{\"t\":\"barrel\",\"x\":10,\"y\":1},"
                 "{\"t\":\"barrel\",\"x\":17,\"y\":2},{\"t\":\"barrel\",\"x\":24,\"y\":3,\"c\":33},");
    pb_fill(objs, sizeof(objs), "spike", 12, 0, 40, 0);
    pb_level(&r, &lv, objs, MODE_CANNON);
    while (r.state == RUN_PLAY && !r.b.ms[0])
        run_step(&r, 0);
    int b1 = r.b.ms[0] - 1;
    float x1 = r.b.x;
    for (int f = 0; f < 30; f++)
        run_step(&r, 0);
    shot_run(&r, "44c_cannon_loaded");
    int waits = r.state == RUN_PLAY && r.b.ms[0] == b1 + 1 && r.b.x == x1;
    run_step(&r, IN_CROSS);
    int fired = !r.b.ms[0] && r.b.mf[0] > 0;
    for (int f = 0; f < 8; f++)
        run_step(&r, 0);
    shot_run(&r, "44c_cannon_fly");
    while (r.state == RUN_PLAY && !r.b.ms[0] && r.b.x < 30 * TILE)
        run_step(&r, 0);
    int b2 = r.b.ms[0] - 1;
    int caught = r.state == RUN_PLAY && b2 >= 0 && lv.objs[b2].x == 17 && lv.objs[b1].x == 10;
    int f = 0;
    while (r.state == RUN_PLAY && r.b.ms[0] == b2 + 1 && f < 400)
        run_step(&r, 0), f++;
    snprintf(what, sizeof(what), "CANNON: a barrel loads it and holds it (%d), X fires it (%d) into the next barrel (%d), "
             "and the fuse fires it after %.1f s", waits, fired, caught, f / 60.0);
    check(waits && fired && caught && f > 120 && f < 160, what);

    // TWIN: the other twin starts on the ceiling, a press jumps both apart
    // and they land again; a spike only the ceiling twin meets kills.
    into_mode(&r, &lv, MODE_TWIN, 1);
    hh = run_half_h(&r.b);
    int on_ceiling = r.b.ms[0] && r.b.mf[0] + hh > 8 * TILE - 1;
    float y0 = r.b.y, ty0 = r.b.mf[0];
    run_step(&r, IN_CROSS);
    for (int k = 0; k < 9; k++)
        run_step(&r, 0);
    shot_run(&r, "44d_twin_jump");
    int apart = r.b.y > y0 + TILE && r.b.mf[0] < ty0 - TILE;
    for (int k = 0; k < 40; k++)
        run_step(&r, 0);
    int landed = r.b.on_ground && r.b.ms[0] && r.state == RUN_PLAY;
    pb_level(&a, &lc, "{\"t\":\"portal_twin\",\"x\":3,\"y\":1},{\"t\":\"spike\",\"x\":20,\"y\":7,\"r\":2},", MODE_TWIN);
    int ceiling_kills = !pb_to(&a, 24, 0) && a.b.x < 21 * TILE && a.b.y > 5 * TILE;
    int twin_ok = solve(&lc, 2, 0, 20000000L) == 1;
    snprintf(what, sizeof(what), "TWIN: one on the ceiling (%d), a press jumps both apart (%d) and both land (%d); the "
             "ceiling twin's spike kills (%d) and a jump clears it (%d)", on_ceiling, apart, landed, ceiling_kills, twin_ok);
    check(on_ceiling && apart && landed && ceiling_kills && twin_ok, what);

    // GHOST: a ghost wall stops it solid and lets it through see-through;
    // the meter runs out, and fills again on the ground.
    objs[0] = 0;
    strcat(objs, "{\"t\":\"portal_ghost\",\"x\":3,\"y\":1},");
    pb_fill(objs, sizeof(objs), "ghost_wall", 16, 0, 17, 4);
    pb_level(&r, &lv, objs, MODE_GHOST);
    a = r;
    int wall_stops = !pb_to(&a, 20, 0);
    a = r;
    pb_to(&a, 13, 0);
    int through = 1;
    while (a.state == RUN_PLAY && a.b.x < 16.6f * TILE)
        run_step(&a, IN_CROSS);
    shot_run(&a, "44e_ghost_through");
    while (a.state == RUN_PLAY && a.b.x < 19 * TILE)
        run_step(&a, IN_CROSS);
    through = pb_to(&a, 24, 0);
    objs[0] = 0;
    pb_fill(objs, sizeof(objs), "ghost_wall", 16, 0, 17, 4);
    pb_level(&c, &lc, objs, MODE_CUBE);
    int cube_stops = !pb_to(&c, 20, IN_CROSS);
    into_mode(&r, &lv, MODE_GHOST, 1);
    int see = 0;
    for (int k = 0; k < 200; k++) {
        run_step(&r, IN_CROSS);
        see += r.b.ms[1];
    }
    int spent = !r.b.ms[1] && r.b.ms[2];
    for (int k = 0; k < 90; k++)
        run_step(&r, 0);
    snprintf(what, sizeof(what), "GHOST: a ghost wall stops it (%d) and the cube (%d) but not see-through (%d); %.1f s "
             "see-through on a full meter (%d), full again on the ground (%d)", wall_stops, cube_stops, through,
             see / 60.0, spent, r.b.ms[0]);
    check(wall_stops && cube_stops && through && see == 90 && spent && r.b.ms[0] == 90 * 4, what);

    // COLOR SWAP: pink at first, so a pink gate stops it; a press makes it
    // blue to pass, and blue it stands on a blue bridge over spikes that
    // pink falls through.
    objs[0] = 0;
    strcat(objs, "{\"t\":\"portal_color_swap\",\"x\":3,\"y\":1},{\"t\":\"pad_yellow\",\"x\":22,\"y\":0},");
    pb_fill(objs, sizeof(objs), "pink_gate", 16, 0, 16, 3);
    pb_fill(objs, sizeof(objs), "blue_block", 25, 2, 34, 2);
    pb_fill(objs, sizeof(objs), "spike", 24, 0, 34, 0);
    pb_level(&r, &lv, objs, MODE_COLOR_SWAP);
    a = r;
    int pink_stops = !pb_to(&a, 18, 0);
    a = r;
    pb_to(&a, 12, 0);
    run_step(&a, IN_CROSS);
    Run blue = a;
    int blue_passes = pb_to(&blue, 37, 0);
    pb_to(&a, 21, 0);
    run_step(&a, IN_CROSS);
    int pink_falls = !pb_to(&a, 37, 0);
    a = r;
    pb_to(&a, 12, 0);
    run_step(&a, IN_CROSS);
    pb_to(&a, 28, 0);
    shot_run(&a, "44f_color_swap");
    snprintf(what, sizeof(what), "COLOR SWAP: a pink gate stops pink (%d), blue gets through and stands on a blue bridge "
             "(%d), pink falls through it (%d)", pink_stops, blue_passes, pink_falls);
    check(pink_stops && blue_passes && pink_falls, what);

    // BLASTER: Square fires a bolt that breaks a crate wall; a target blows up
    // the crates of its group; in practice Square fires instead of placing a
    // checkpoint.
    objs[0] = 0;
    strcat(objs, "{\"t\":\"portal_blaster\",\"x\":3,\"y\":1},{\"t\":\"target\",\"x\":30,\"y\":0,\"g\":1},");
    pb_fill(objs, sizeof(objs), "crate", 20, 0, 20, 2);
    for (int x = 36; x <= 37; x++)
        for (int y = 0; y <= 4; y++) {
            size_t len = strlen(objs);
            snprintf(objs + len, sizeof(objs) - len, "{\"t\":\"crate\",\"x\":%d,\"y\":%d,\"g\":1},", x, y);
        }
    pb_level(&r, &lv, objs, MODE_BLASTER);
    a = r;
    int crate_stops = !pb_to(&a, 22, 0);
    a = r;
    pb_to(&a, 14, 0);
    run_step(&a, IN_SQUARE);
    run_step(&a, IN_SQUARE);
    run_step(&a, 0);
    shot_run(&a, "44g_blaster_shot");
    int bolt = a.b.ms[0] > 0;
    int shot_through = pb_to(&a, 25, 0) && a.b.pb_broke[0] != 0;
    pb_to(&a, 26, 0);
    run_step(&a, IN_SQUARE);
    while (a.state == RUN_PLAY && a.b.ms[0])
        run_step(&a, 0);
    shot_run(&a, "44g_blaster_target");
    int group_gone = a.b.pb_groups == 1 && pb_to(&a, 40, 0);
    run_start(&a, &lv, 1, 0);
    while (a.b.mode != MODE_BLASTER)
        run_step(&a, 0);
    int ncp = a.ncp;
    run_step(&a, IN_SQUARE);
    run_step(&a, 0);
    int practice_fires = a.ncp == ncp && a.b.ms[0] > 0;
    snprintf(what, sizeof(what), "BLASTER: a crate wall stops it (%d), a Square bolt (%d) breaks it (%d), a target blows "
             "up its group (%d), and in practice Square fires instead of a checkpoint (%d)", crate_stops, bolt,
             shot_through, group_gone, practice_fires);
    check(crate_stops && bolt && shot_through && group_gone && practice_fires, what);

    // TIME BENDER: holding X halves its speed (a flap still flaps) for up to
    // two seconds, and the meter fills again with X up.
    into_mode(&r, &lv, MODE_TIME_BENDER, 1);
    a = r;
    float xa = a.b.x;
    for (int k = 0; k < 60; k++)
        run_step(&a, k < 2 ? IN_CROSS : 0);
    float full = a.b.x - xa;
    a = r;
    xa = a.b.x;
    int slow = 0;
    for (int k = 0; k < 60; k++) {
        run_step(&a, IN_CROSS);
        slow += a.b.ms[1];
    }
    float bent = a.b.x - xa;
    shot_run(&a, "44h_time_bender_slow");
    for (int k = 0; k < 100; k++) {
        run_step(&a, IN_CROSS);
        slow += a.b.ms[1];
    }
    int spent_tb = !a.b.ms[1];
    for (int k = 0; k < 130; k++)
        run_step(&a, k % 30 < 3 ? IN_CROSS : 0);
    snprintf(what, sizeof(what), "TIME BENDER: holding X it goes %.0f px in a second, %.0f px without (%.2f); %.1f s of "
             "slow time on a full meter (%d), full again with X up (%d)", bent, full, bent / full, slow / 60.0, spent_tb,
             a.b.ms[0]);
    check(bent / full > 0.45f && bent / full < 0.55f && slow == 120 && spent_tb && a.b.ms[0] == 120 * 4 &&
          a.state == RUN_PLAY, what);

    // SKATER: a press ollies it onto a rail over spikes; held, X keeps it
    // grinding (a third faster) instead of ollieing again; a rail lets a cube
    // jump up through it from below.
    objs[0] = 0;
    strcat(objs, "{\"t\":\"portal_skater\",\"x\":3,\"y\":1},");
    pb_fill(objs, sizeof(objs), "rail", 12, 1, 26, 1);
    pb_fill(objs, sizeof(objs), "spike", 14, 0, 26, 0);
    pb_level(&r, &lv, objs, MODE_SKATER);
    int done = 0, grind = 0, ollies = 0;
    float gx = 0;
    for (float px = 9.0f; px < 12.0f && !done; px += 0.25f) {
        a = r;
        pb_to(&a, px, 0);
        grind = 0;
        int jumps = a.jumps;
        while (a.state == RUN_PLAY && a.b.x < 30 * TILE) {
            float x = a.b.x;
            int was = a.b.ms[0];
            run_step(&a, IN_CROSS);
            if (was && a.b.ms[0]) {
                grind++;
                gx = a.b.x - x;
                if (grind == 20)
                    shot_run(&a, "44i_skater_grind");
            }
        }
        done = a.state == RUN_PLAY;
        ollies = a.jumps - jumps;
    }
    pb_level(&c, &lc, "{\"t\":\"rail\",\"x\":14,\"y\":2},{\"t\":\"rail\",\"x\":15,\"y\":2},", MODE_CUBE);
    pb_to(&c, 12.3f, 0);
    int under = 0;
    for (int k = 0; k < 60 && c.state == RUN_PLAY; k++) {
        run_step(&c, k == 0 ? IN_CROSS : 0);
        if (c.b.x > 13.6f * TILE && c.b.x < 16.4f * TILE && c.b.y + run_half_h(&c.b) > 2.6f * TILE)
            under = 1;
    }
    snprintf(what, sizeof(what), "SKATER: one press and X held: %d ollie, then it grinds the rail over spikes (%d frames) at "
             "%.1f px a frame (%.2fx); a cube jumps up through a rail (%d)", ollies, grind, gx, gx / run_speed_px(1), under);
    check(done && ollies == 1 && grind > 30 && gx / run_speed_px(1) > 1.3f && gx / run_speed_px(1) < 1.4f && under &&
          c.state == RUN_PLAY && c.b.on_ground, what);

    // SHAPESHIFTER: a press makes it a ship with a morph, held it climbs; the
    // next press makes it a cube that drops much faster than the ship sinks.
    into_mode(&r, &lv, MODE_SHAPESHIFTER, 1);
    int cube0 = r.b.ms[0] == 0;
    run_step(&r, IN_CROSS);
    int morph = (r.sfx & SFX_MORPH) && r.b.ms[0] == 1;
    float ys = r.b.y;
    for (int k = 0; k < 30; k++)
        run_step(&r, IN_CROSS);
    int climbs = r.b.y > ys + 1.5f * TILE;
    run_step(&r, 0);
    a = r;
    run_step(&a, IN_CROSS);
    shot_run(&a, "44j_shapeshifter_morph");
    int back = a.b.ms[0] == 0;
    float ya = a.b.y, yb = r.b.y;
    for (int k = 0; k < 12; k++) {
        run_step(&a, 0);
        run_step(&r, 0);
    }
    snprintf(what, sizeof(what), "SHAPESHIFTER: a cube (%d), a press makes the ship with a morph (%d) that climbs held (%d); "
             "the next press is a cube again (%d) that drops %.0f px while the ship sinks %.0f", cube0, morph, climbs,
             back, ya - a.b.y, yb - r.b.y);
    check(cube0 && morph && climbs && back && ya - a.b.y > (yb - r.b.y) * 1.8f, what);
    for (int k = 0; k < 40; k++)
        run_step(&r, k < 2 || (k > 20 && k < 24) ? IN_CROSS : 0);
    shot_run(&r, "44j_shapeshifter_ship");

    Look plain = { { 0, 1, 0, 0, 0, 0 }, 0 };
    look_set(&plain);
}

// Ghost files keep Square (the blaster's button) on a line of their own:
// it reads back, a run played from it fires again, and an older file with no
// such line still loads.
static void test_packb_ghost(void)
{
    static Ghost gh, back;
    static Level lv;
    char what[160];
    Run r;
    pb_level(&r, &lv, "{\"t\":\"portal_blaster\",\"x\":3,\"y\":1},{\"t\":\"crate\",\"x\":20,\"y\":0},"
                      "{\"t\":\"crate\",\"x\":20,\"y\":1},", MODE_BLASTER);
    memset(&gh, 0, sizeof(gh));
    run_start(&r, &lv, 0, 0);
    for (int t = 0; t < 200; t++) {
        int in = t >= 70 && t < 74 ? IN_SQUARE : 0;
        ghost_push(&gh, in & IN_CROSS);
        if (in & IN_SQUARE)
            ghost_mark_square(&gh);
        run_step(&r, in);
    }
    gh.hash = level_hash(&lv);
    gh.pct = 40;
    reset_data();
    ghost_save(&gh, "file:sq.json");
    int loaded = ghost_load(&back, "file:sq.json", gh.hash) && back.frames == 200 && ghost_square(&back, 71) &&
                 !ghost_square(&back, 69) && !ghost_square(&back, 74);
    run_start(&r, &lv, 0, 0);
    for (int t = 0; t < back.frames; t++)
        run_step(&r, (ghost_bit(&back, t) ? IN_CROSS : 0) | (ghost_square(&back, t) ? IN_SQUARE : 0));
    int replays = r.state == RUN_PLAY && r.b.pb_broke[0] && r.b.x > 21 * TILE;
    write_text(GHOSTS_DIR "/old.ghost", "fritzdash-ghost 1\n000000aa 50 10\n4 3 3 \n");
    int old = ghost_load(&back, "old", 0xaa) && back.frames == 10 && ghost_bit(&back, 5) && !ghost_bit(&back, 8);
    for (int t = 0; t < 10; t++)
        old &= !ghost_square(&back, t);
    snprintf(what, sizeof(what), "ghost files keep Square presses (%d), a blaster ghost replays its shot (%d), and old "
             "ghost files still load (%d)", loaded, replays, old);
    check(loaded && replays && old, what);
}

// The editor: every pack B object and portal is in the palette, objects drawn
// in a level, and a barrel's aim, swing and power set with Triangle.
static void test_packb_editor(void)
{
    static Game g;
    static Level lv;
    static char text[1 << 16];
    char what[200];
    reset_data();
    write_text(LEVELS_DIR "/slot01.json", "{\"name\":\"PACK B\",\"objects\":[{\"t\":\"barrel\",\"x\":5,\"y\":2},"
               "{\"t\":\"ghost_wall\",\"x\":8,\"y\":0},{\"t\":\"ghost_wall\",\"x\":8,\"y\":1},{\"t\":\"pink_block\",\"x\":10,\"y\":0},"
               "{\"t\":\"blue_block\",\"x\":11,\"y\":0},{\"t\":\"pink_gate\",\"x\":13,\"y\":0},{\"t\":\"blue_gate\",\"x\":14,\"y\":0},"
               "{\"t\":\"crate\",\"x\":16,\"y\":0},{\"t\":\"target\",\"x\":17,\"y\":1,\"g\":1},{\"t\":\"rail\",\"x\":19,\"y\":1},"
               "{\"t\":\"rail\",\"x\":20,\"y\":1},{\"t\":\"portal_magnet\",\"x\":3,\"y\":1},{\"t\":\"end\",\"x\":40,\"y\":0}]}");
    game_init(&g, 12);
    game_step(&g, 0);
    g.title_sel = TITLE_EDITOR;
    press(&g, IN_CROSS);
    press(&g, IN_CROSS);
    int seen = 0, portals = 0;
    for (int k = 0; k < 600; k++) {
        int t = editor_brush_type(&g.ed);
        if (t >= OBJ_BARREL && t <= OBJ_RAIL)
            seen |= 1 << (t - OBJ_BARREL);
        if (t >= OBJ_PORTAL_MAGNET && t <= OBJ_PORTAL_SHAPESHIFTER)
            portals |= 1 << (t - OBJ_PORTAL_MAGNET);
        press(&g, IN_R1);
    }
    for (int f = 0; f < 20; f++)
        game_step(&g, 0);
    shot_game(&g, "44k_editor_packb");

    reset_data();
    game_init(&g, 13);
    game_step(&g, 0);
    g.title_sel = TITLE_EDITOR;
    press(&g, IN_CROSS);
    press(&g, IN_CROSS);
    for (int k = 0; k < 600 && editor_brush_type(&g.ed) != OBJ_BARREL; k++)
        press(&g, IN_R1);
    presses(&g, IN_RIGHT, 6);
    press(&g, IN_UP);
    press(&g, IN_CROSS);
    press(&g, IN_TRIANGLE);
    int panel = g.ed.screen == ED_OBJSET;
    press(&g, IN_RIGHT);                            // aim 45 -> 60
    press(&g, IN_DOWN);
    press(&g, IN_RIGHT);                            // swings a little
    press(&g, IN_DOWN);
    press(&g, IN_RIGHT);                            // medium -> far
    for (int f = 0; f < 20; f++)
        game_step(&g, 0);
    shot_game(&g, "44l_editor_barrel_settings");
    press(&g, IN_CIRCLE);
    press(&g, IN_R2);
    presses(&g, IN_DOWN, 10);
    press(&g, IN_CROSS);
    int n = plat_read_file(LEVELS_DIR "/slot01.json", text, (int)sizeof(text)), at = -1;
    for (int i = 0; n > 0 && level_parse(&lv, text, n) == LEVEL_OK && i < lv.nobj; i++)
        if (lv.objs[i].type == OBJ_BARREL)
            at = i;
    char aim[40] = "", swing[40] = "", power[40] = "";
    if (at >= 0) {
        obj_def(OBJ_BARREL)->set_text(&lv.objs[at], 0, aim, sizeof(aim));
        obj_def(OBJ_BARREL)->set_text(&lv.objs[at], 1, swing, sizeof(swing));
        obj_def(OBJ_BARREL)->set_text(&lv.objs[at], 2, power, sizeof(power));
    }
    snprintf(what, sizeof(what), "the editor has all 9 pack B objects (%03x) and 10 portals (%03x); Triangle sets a "
             "barrel (%s, %s, %s)", seen, portals, aim, swing, power);
    check(seen == 0x1FF && portals == 0x3FF && panel && at >= 0 && !strcmp(aim, "60 DEGREES") &&
          !strcmp(swing, "SWINGS A LITTLE") && !strcmp(power, "FAR"), what);
}

static void test_packb(void)
{
    test_packb_physics();
    test_packb_ghost();
    test_packb_editor();
}

// --- 3.2 integration: both mode packs together -------------------------------

// Every new mode's portal (robot .. shapeshifter) and every pack A and pack B
// object is in the editor palette, once. One screenshot per page of the
// palette strip (five names) that holds any of them, over a level with all of
// them laid out in palette order, so each page shows its objects too.
static void test_int_palette(void)
{
    static Game g;
    static char json[1 << 14];
    static int types[200], at[200];
    char what[200];
    reset_data();
    game_init(&g, 14);
    game_step(&g, 0);
    g.title_sel = TITLE_EDITOR;
    press(&g, IN_CROSS);
    press(&g, IN_CROSS);
    int n = 0, first = editor_brush_type(&g.ed);
    do {
        types[n++] = editor_brush_type(&g.ed);
        press(&g, IN_R1);
    } while (editor_brush_type(&g.ed) != first && n < 200);

    int len = snprintf(json, sizeof(json), "{\"name\":\"ALL MODES\",\"objects\":[");
    int nnew = 0, portals = 0, twice = 0;
    unsigned long long objs = 0;
    for (int i = 0; i < n; i++) {
        int t = types[i], m = -1;
        at[i] = -1;
        for (int k = MODE_ROBOT; k < MODE_COUNT; k++)
            if (MODES[k] && t == run_mode_portal(k))
                m = k;
        if (m < 0 && !(t >= OBJ_PACK_A && t < OBJ_EGGS))
            continue;
        if (m >= 0) {
            twice |= portals >> (m - MODE_ROBOT) & 1;
            portals |= 1 << (m - MODE_ROBOT);
        } else {
            twice |= (int)(objs >> (t - OBJ_PACK_A) & 1);
            objs |= 1ull << (t - OBJ_PACK_A);
        }
        at[i] = 4 + 3 * nnew++;
        len += snprintf(json + len, sizeof(json) - (size_t)len, "{\"t\":\"%s\",\"x\":%d,\"y\":%d},", obj_name(t), at[i],
                        t == OBJ_ANCHOR ? 4 : 1);
    }
    snprintf(json + len, sizeof(json) - (size_t)len, "{\"t\":\"end\",\"x\":%d,\"y\":0}]}", 8 + 3 * nnew);

    unsigned long long want_objs = (1ull << (OBJ_WATER - OBJ_PACK_A + 1)) - 1;
    want_objs |= ((1ull << (OBJ_RAIL - OBJ_BARREL + 1)) - 1) << (OBJ_BARREL - OBJ_PACK_A);
    snprintf(what, sizeof(what), "the editor palette has all 25 new mode portals (%07x) and the 14 pack A and B objects "
             "(%llx), each once", portals, objs);
    check(nnew == 39 && portals == (1 << 25) - 1 && objs == want_objs && !twice, what);

    reset_data();
    write_text(LEVELS_DIR "/slot01.json", json);
    game_init(&g, 15);
    game_step(&g, 0);
    g.title_sel = TITLE_EDITOR;
    press(&g, IN_CROSS);
    press(&g, IN_CROSS);
    for (int f = 0; f < 600; f++)                   // the BUILDER badge pop-up goes away
        game_step(&g, 0);
    int page = 0;
    for (int i = 0; i < n; i++) {
        if (at[i] < 0)
            continue;
        int c = i + 2 < n ? i + 2 : n - 1;
        for (int k = 0; k < 400 && editor_brush_type(&g.ed) != types[c]; k++)
            press(&g, IN_R1);
        g.ed.cx = at[c] >= 0 ? at[c] : at[i];
        g.ed.cy = 6;                                // the brush shows above the row, not on it
        for (int f = 0; f < 20; f++)
            game_step(&g, 0);
        char name[40];
        snprintf(name, sizeof(name), "45_palette_%02d", ++page);
        shot_game(&g, name);
        i = c + 2;
    }
    printf("     %d palette pages for the new modes and objects\n", page);
}

// --- Endless mode (the v1.x game) -------------------------------------------

static int plan_safe(const Endless *g0, int d)
{
    Endless g = *g0;
    int jumped = 0;
    for (int t = 0; t < 160; t++) {
        int press = !jumped && t >= d;
        int was_ground = g.on_ground;
        endless_step(&g, press);
        if (g.state != EN_PLAY)
            return 0;
        if (press && was_ground && !g.on_ground)
            jumped = 1;
        if (jumped && g.on_ground) {
            for (int k = 0; k < 4; k++) {
                endless_step(&g, 0);
                if (g.state != EN_PLAY)
                    return 0;
            }
            return 1;
        }
    }
    return 1;
}

static int autopilot(const Endless *g)
{
    if (g->state != EN_PLAY || !g->on_ground)
        return 0;
    if (plan_safe(g, 5))
        return 0;
    if (plan_safe(g, 0))
        return 1;
    for (int d = 1; d < 45; d++)
        if (plan_safe(g, d))
            return 0;
    return 1;
}

static void endless_go(Endless *g, int best, uint32_t seed)
{
    endless_init(g, best, seed);
    endless_step(g, 0);
}

static void test_endless(void)
{
    Endless g;
    endless_init(&g, 0, 99);
    for (int i = 0; i < 5; i++)
        endless_step(&g, IN_CROSS);
    check(g.on_ground && g.state == EN_PLAY, "endless: the X that picked Endless does not jump");

    endless_go(&g, 0, 99);
    int f = 0;
    while (g.state == EN_PLAY && f < 5000) {
        endless_step(&g, 0);
        f++;
    }
    check(g.state == EN_DYING && g.score == 0, "endless: never pressing X dies at the first obstacle with score 0");
    check(g.save_pending == 0 && g.best == 0, "endless: score 0 is not saved as a best");
    for (int i = 0; i < 200; i++)
        endless_step(&g, IN_CROSS);
    check(g.state == EN_OVER, "endless: holding X through death stays on the game over screen");
    endless_step(&g, 0);
    endless_step(&g, IN_CROSS);
    check(g.state == EN_PLAY && g.score == 0, "endless: X on game over starts a fresh run");

    endless_go(&g, 0, 99);
    f = 0;
    while (g.state == EN_PLAY && f < 5000) {
        endless_step(&g, 0);
        f++;
    }
    for (int i = 0; i < 200; i++)
        endless_step(&g, 0);
    endless_step(&g, IN_START);
    check(g.state == EN_PLAY && g.score == 0, "endless: Options on game over retries");

    endless_go(&g, 0, 4242);
    for (int i = 0; i < 120; i++)
        endless_step(&g, 0);
    endless_step(&g, IN_START);
    check(g.state == EN_PAUSE && g.pause_sel == 0, "endless: Options opens the pause menu on Resume");
    Endless before = g;
    for (int i = 0; i < 100; i++)
        endless_step(&g, 0);
    check(g.state == EN_PAUSE && g.nobs == before.nobs && g.nobs > 0 && g.obs[0].x == before.obs[0].x &&
          g.py == before.py && g.score == before.score, "endless: the world is frozen while paused");
    endless_step(&g, IN_START);
    check(g.state == EN_PLAY, "endless: Options again resumes");
    endless_step(&g, 0);
    endless_step(&g, IN_START);
    endless_step(&g, 0);
    endless_step(&g, IN_UP);
    endless_step(&g, 0);
    check(g.pause_sel == 2, "endless: D-pad up from Resume wraps to Title");
    endless_step(&g, IN_CROSS);
    check(g.exit, "endless: Title asks to go back to the title menu");

    endless_go(&g, 0, 99);
    while (g.state == EN_PLAY)
        endless_step(&g, 0);
    for (int i = 0; i < 100; i++)
        endless_step(&g, 0);
    endless_step(&g, IN_CIRCLE);
    check(g.exit, "endless: Circle on game over goes back to the title menu");

    endless_go(&g, 0, 2024);
    for (f = 0; f < 216000 && g.state == EN_PLAY; f++)
        endless_step(&g, autopilot(&g));
    printf("endless: %d frames, state=%d, score=%d, speed=%.1f\n", f, g.state, g.score, g.speed);
    check(g.state == EN_PLAY, "endless: autopilot survives one hour, so every pattern is clearable at every speed");
}

// --- The whole app ----------------------------------------------------------

static void presses(Game *g, int bits, int n);

static void press(Game *g, int bits)
{
    game_step(g, 0);
    game_step(g, bits);
    game_step(g, 0);
}

// Boot parses the built-in levels once; a rescan (every visit to LEVELS) only
// reads his files. Runs before any other game_init so its boot is the cold one.
static void test_scan_speed(void)
{
    static Game g;
    long bytes = 0;
    for (int i = 0; i < BUILTIN_COUNT; i++)
        bytes += (long)strlen(BUILTIN_JSON[i]);
    reset_data();
    clock_t c0 = clock();
    game_init(&g, 7);
    double boot = (double)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
    game_step(&g, 0);
    c0 = clock();
    for (int k = 0; k < 20; k++) {
        press(&g, IN_CROSS);
        press(&g, IN_CIRCLE);
    }
    double rescan = (double)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC / 20;
    printf("built-in levels: %d, %ld bytes of JSON; boot with the parse %.2f ms, a rescan %.3f ms (this CPU)\n",
           BUILTIN_COUNT, bytes, boot, rescan);
    check(g.nlevels == BUILTIN_COUNT && g.state == ST_TITLE && (rescan < boot / 4 || rescan < 0.5),
          "the built-in levels are parsed once at boot; going to LEVELS only rescans his files");
}

// From the title: LEVELS (or 2 PLAYER) shows the world cards; X goes into the
// world the cursor is on.
static void open_world(Game *g)
{
    press(g, IN_CROSS);
    press(g, IN_CROSS);
}

static void test_app(void)
{
    static Game g;
    char buf[4096], what[160];
    reset_data();
    write_text(LEVELS_DIR "/b_second.json",
               "{\"format\":\"fritzdash-level\",\"name\":\"SECOND\",\"author\":\"FRITZ\",\"objects\":[{\"t\":\"end\",\"x\":30,\"y\":0}]}");
    write_text(LEVELS_DIR "/a_first.JSON", "{\"name\":\"FIRST\",\"objects\":[{\"t\":\"spike\",\"x\":14,\"y\":0}]}");
    write_text(LEVELS_DIR "/c_broken.json", "{\"name\":\"BROKEN\",\"objects\":[");
    write_text(LEVELS_DIR "/._a_first.json", "\x00\x05\x16\x07 resource fork");
    write_text(LEVELS_DIR "/notes.txt", "not a level");

    game_init(&g, 1);
    for (int i = 0; i < 30; i++)
        game_step(&g, IN_CROSS);
    check(g.state == ST_TITLE, "X held since boot does not pick a menu entry");
    check(g.nlevels == BUILTIN_COUNT + 2 && g.bad_files == 1, "level files load; broken, hidden and non-JSON files are skipped");
    check(!strcmp(g.levels[BUILTIN_COUNT].name, "FIRST") && !strcmp(g.levels[BUILTIN_COUNT + 1].name, "SECOND"),
          "level files are listed by file name after the built-in levels");
    game_set_diag("PADS:1 H:00000100 USER:1E5A8C3F RC:00000000 BTN:00000060 CON:1 RD:00000ABC");
    shot_game(&g, "01_title");
    game_step(&g, IN_L1 | IN_R1);
    check(g.show_diag, "L1+R1 shows the diagnostic line");
    shot_game(&g, "01b_title_diag");

    press(&g, IN_CROSS);
    check(g.state == ST_LEVELS && g.picking && g.lworld_sel == 0, "LEVELS opens the world cards on WORLD 1");
    shot_game(&g, "02_levels_worlds");
    press(&g, IN_LEFT);
    check(g.picking && g.lworld_sel == LWORLD_MINE, "left from the first world wraps to MY LEVELS");
    press(&g, IN_CROSS);
    check(!g.picking && g.level_sel == BUILTIN_COUNT, "X opens MY LEVELS on its first level");
    shot_game(&g, "02b_levels_file");
    press(&g, IN_RIGHT);
    press(&g, IN_RIGHT);
    check(g.level_sel == BUILTIN_COUNT, "left and right go round that world's levels only");
    press(&g, IN_CIRCLE);
    check(g.state == ST_LEVELS && g.picking && g.lworld_sel == LWORLD_MINE, "Circle goes back to the world cards");
    press(&g, IN_RIGHT);
    press(&g, IN_CROSS);
    int last0 = 0;
    for (int i = 0; i < BUILTIN_COUNT; i++)
        last0 = BUILTIN_IDS[i][0] == '0' ? i : last0;
    check(!g.picking && g.level_sel == 0, "X opens WORLD 1 on its first level");
    shot_game(&g, "02_levels_builtin");
    press(&g, IN_LEFT);
    check(g.level_sel == last0, "left from the first level wraps to the world's last");
    press(&g, IN_RIGHT);
    press(&g, IN_RIGHT);
    check(g.level_sel == 1, "right moves to the next level");
    press(&g, IN_CIRCLE);
    press(&g, IN_CIRCLE);
    check(g.state == ST_TITLE, "Circle twice goes back to the title");
    press(&g, IN_CROSS);
    check(g.picking && g.lworld_sel == 0 && g.level_sel == 1, "the world cards remember the world");
    press(&g, IN_CROSS);
    check(!g.picking && g.level_sel == 1, "and the world remembers the level");
    press(&g, IN_CIRCLE);
    press(&g, IN_CIRCLE);

    // Play the user level FIRST: die on its spike and check the saved progress.
    open_world(&g);
    g.level_sel = BUILTIN_COUNT;
    press(&g, IN_CROSS);
    check(g.state == ST_PLAY && !g.run.practice, "X starts the level");
    int f;
    for (f = 0; f < 600 && g.run.state == RUN_PLAY; f++)
        game_step(&g, 0);
    read_text(PROGRESS_FILE, buf, sizeof(buf));
    printf("progress.txt after one death: %s", buf);
    snprintf(what, sizeof(what), "a death saves attempts and best %%");
    check(g.levels[BUILTIN_COUNT].attempts == 1 && g.levels[BUILTIN_COUNT].best > 0 &&
          strstr(buf, "file:a_first.JSON") != NULL, what);
    for (f = 0; f < 60; f++)
        game_step(&g, 0);
    check(g.state == ST_PLAY && g.run.state == RUN_PLAY && g.run.attempt == 2, "the level restarts by itself after a death");

    game_step(&g, IN_START);
    check(g.state == ST_PAUSE, "Options pauses");
    Run before = g.run;
    for (f = 0; f < 50; f++)
        game_step(&g, 0);
    check(g.run.b.x == before.b.x && g.run.frame == before.frame, "the level is frozen while paused");
    shot_game(&g, "07_pause");
    press(&g, IN_DOWN);
    press(&g, IN_DOWN);
    press(&g, IN_CROSS);
    check(g.state == ST_PLAY && g.run.practice && g.run.ncp == 1, "PRACTICE in the pause menu turns practice on here");
    game_step(&g, IN_START);
    game_step(&g, 0);
    press(&g, IN_DOWN);
    press(&g, IN_CROSS);
    check(g.state == ST_PLAY && g.run.b.x < TILE && g.run.ncp == 0, "RESTART goes back to the start");
    game_step(&g, IN_START);
    game_step(&g, 0);
    press(&g, IN_UP);
    press(&g, IN_CROSS);
    check(g.state == ST_LEVELS, "EXIT goes back to the level select");

    // Finish the SECOND level (empty, so no input needed) for the complete screen.
    g.level_sel = BUILTIN_COUNT + 1;
    press(&g, IN_SQUARE);
    check(g.state == ST_PLAY && g.run.practice, "Square starts the level in practice");
    for (f = 0; f < 400 && g.state == ST_PLAY; f++) {
        game_step(&g, 0);
        if (g.run.state == RUN_DONE && g.run.state_frames == 25)
            shot_game(&g, "09b_level_complete_banner");
    }
    check(g.state == ST_COMPLETE && g.levels[BUILTIN_COUNT + 1].practice_best == 100 &&
          g.levels[BUILTIN_COUNT + 1].best == 0, "finishing in practice saves 100% practice, not normal");
    for (f = 0; f < 40; f++)
        game_step(&g, 0);
    shot_game(&g, "09_complete");
    press(&g, IN_CIRCLE);
    check(g.state == ST_LEVELS, "Circle on the complete screen goes back to levels");
    press(&g, IN_CROSS);
    for (f = 0; f < 400 && g.state == ST_PLAY; f++)
        game_step(&g, 0);
    for (f = 0; f < 40; f++)
        game_step(&g, 0);
    press(&g, IN_CROSS);
    check(g.state == ST_PLAY && g.run.attempt == 1, "X on the complete screen plays again");

    // Progress survives a restart of the game.
    game_init(&g, 2);
    check(g.levels[BUILTIN_COUNT].attempts == 1 && g.levels[BUILTIN_COUNT + 1].best == 100 &&
          g.levels[BUILTIN_COUNT + 1].practice_best == 100, "progress is read back on the next start");

    // Settings.
    for (int i = 0; i < TITLE_SETTINGS; i++)
        press(&g, IN_DOWN);
    press(&g, IN_CROSS);
    check(g.state == ST_SETTINGS, "SETTINGS opens");
    press(&g, IN_CROSS);
    press(&g, IN_DOWN);
    press(&g, IN_CROSS);
    press(&g, IN_DOWN);
    press(&g, IN_LEFT);
    press(&g, IN_DOWN);
    press(&g, IN_RIGHT);
    press(&g, IN_RIGHT);
    shot_game(&g, "10_settings");
    read_text(SETTINGS_FILE, buf, sizeof(buf));
    check(!g.show_progress && g.auto_checkpoints && g.music_vol == 6 && g.sfx_vol == 10 &&
          strstr(buf, "progress_bar 0") && strstr(buf, "auto_checkpoints 1") && strstr(buf, "music 6"),
          "settings toggle, volumes change, and all are saved");
    press(&g, IN_CIRCLE);
    game_init(&g, 3);
    check(!g.show_progress && g.auto_checkpoints && g.music_vol == 6, "settings are read back on the next start");
    g.show_progress = 1;

    // Endless: plays the old game, saves its best, Circle returns.
    g.title_sel = TITLE_ENDLESS;
    press(&g, IN_CROSS);
    check(g.state == ST_ENDLESS, "ENDLESS starts the endless runner");
    for (f = 0; f < 3000 && g.endless.score < 12; f++)
        game_step(&g, autopilot(&g.endless));
    shot_game(&g, "12_endless");
    for (f = 0; f < 3000 && g.endless.state == EN_PLAY; f++)
        game_step(&g, 0);
    for (f = 0; f < 120; f++)
        game_step(&g, 0);
    read_text(BEST_FILE, buf, sizeof(buf));
    snprintf(what, sizeof(what), "a new endless best is saved (best.txt = %d)", atoi(buf));
    check(atoi(buf) == g.endless_best && g.endless_best >= 12, what);
    // Practice plays the calm song, or the level's own one if Settings say so.
    press(&g, IN_CIRCLE);
    g.title_sel = TITLE_LEVELS;
    open_world(&g);
    press(&g, IN_SQUARE);
    int calm = g.song == SONG_PRACTICE && g.run.bpm == synth_bpm(SONG_PRACTICE);
    game_step(&g, IN_START);
    game_step(&g, 0);
    presses(&g, IN_DOWN, 2);
    press(&g, IN_CROSS);
    check(calm && !g.run.practice && g.song != SONG_PRACTICE,
          "practice plays the calm song; turning practice off goes back to the level song");
    g.practice_song = 0;
    game_step(&g, IN_START);
    game_step(&g, 0);
    presses(&g, IN_DOWN, 2);
    press(&g, IN_CROSS);
    check(g.run.practice && g.song != SONG_PRACTICE, "with PRACTICE MUSIC: LEVEL SONG practice keeps the level song");
    g.practice_song = 1;
    game_step(&g, IN_START);
    game_step(&g, 0);
    presses(&g, IN_UP, 1);
    press(&g, IN_CROSS);
    press(&g, IN_CIRCLE);
    shot_game(&g, "10b_settings_after");
    shot_game(&g, "12b_endless_over");
    press(&g, IN_CIRCLE);
    check(g.state == ST_TITLE, "Circle on the endless game over returns to the title");
}


// --- Level files round trip --------------------------------------------------

static void test_level_write(void)
{
    static Level a, b;
    static char text[1 << 20];
    int ok = 1;
    for (int i = 0; i < BUILTIN_COUNT; i++) {
        level_parse(&a, BUILTIN_JSON[i], (int)strlen(BUILTIN_JSON[i]));
        snprintf(a.name, sizeof(a.name), "QUOTE \"%d\" \\ BACK", i);
        int n = level_write(&a, text, (int)sizeof(text));
        ok &= n > 0 && level_parse(&b, text, n) == LEVEL_OK && b.nobj == a.nobj && !strcmp(a.name, b.name) &&
              b.speed == a.speed && b.stars == a.stars && b.song == a.song && b.end_x == a.end_x &&
              !memcmp(&a.colors, &b.colors, sizeof(a.colors)) &&
              !memcmp(a.objs, b.objs, sizeof(LevelObj) * (size_t)a.nobj);
    }
    check(ok, "every built-in level writes out and reads back the same, names with quotes included");
    check(level_write(&a, text, 200) == -1, "writing into too small a buffer fails cleanly");
}

// --- Editor -------------------------------------------------------------------

static void presses(Game *g, int bits, int n)
{
    for (int i = 0; i < n; i++)
        press(g, bits);
}

static void test_editor(void)
{
    static Game g;
    static Level lv;
    static char text[1 << 20];
    char what[160];
    reset_data();
    game_init(&g, 7);
    game_step(&g, 0);
    g.title_sel = TITLE_EDITOR;
    press(&g, IN_CROSS);
    check(g.state == ST_EDITOR && g.ed.screen == ED_SLOTS, "EDITOR opens the slot picker");
    press(&g, IN_CROSS);
    check(g.ed.screen == ED_EDIT && g.ed.slot == 0 && g.ed.cx == 8, "X on an empty slot starts a new level");

    press(&g, IN_CROSS);                     // block at (8, 0)
    presses(&g, IN_R1, 2);                   // spike
    press(&g, IN_RIGHT);
    press(&g, IN_CROSS);                     // spike at (9, 0)
    press(&g, IN_TRIANGLE);                  // turned a quarter
    presses(&g, IN_L1, 2);                   // block
    presses(&g, IN_RIGHT, 3);
    game_step(&g, 0);                        // paint (12..14, 0) holding X
    game_step(&g, IN_CROSS);
    game_step(&g, IN_CROSS | IN_RIGHT);
    game_step(&g, IN_CROSS);
    game_step(&g, IN_CROSS | IN_RIGHT);
    game_step(&g, 0);
    check(g.ed.cx == 14 && g.ed.dirty, "the cursor moves and painting with X held places as it goes");
    presses(&g, IN_UP, 2);
    shot_game(&g, "13_editor");
    presses(&g, IN_DOWN, 2);
    press(&g, IN_CIRCLE);                    // undo the paint stroke
    presses(&g, IN_LEFT, 6);
    game_step(&g, IN_L2);                    // select (8..9, 0)
    game_step(&g, IN_L2 | IN_RIGHT);
    game_step(&g, 0);
    check(g.ed.has_sel && g.ed.sel_x0 == 8 && g.ed.sel_x1 == 9, "holding L2 and moving selects a rectangle");
    press(&g, IN_R2);
    check(g.ed.screen == ED_MENU, "R2 opens the editor menu");
    presses(&g, IN_DOWN, 2);
    press(&g, IN_CROSS);                     // copy
    check(g.ed.nclip == 2 && g.ed.screen == ED_EDIT, "COPY SELECTION copies its objects");
    presses(&g, IN_RIGHT, 11);
    press(&g, IN_R2);
    presses(&g, IN_DOWN, 3);
    press(&g, IN_CROSS);                     // paste at (20, 0)

    press(&g, IN_R2);
    presses(&g, IN_DOWN, 5);
    press(&g, IN_CROSS);                     // name
    check(g.ed.screen == ED_NAME, "NAME opens the keyboard");
    presses(&g, IN_SQUARE, 12);
    presses(&g, IN_RIGHT, 5); press(&g, IN_CROSS);                       // F
    press(&g, IN_DOWN); presses(&g, IN_RIGHT, 2); press(&g, IN_CROSS);   // R
    press(&g, IN_UP); press(&g, IN_RIGHT); press(&g, IN_CROSS);          // I
    press(&g, IN_DOWN); press(&g, IN_RIGHT); press(&g, IN_CROSS);        // T
    press(&g, IN_DOWN); presses(&g, IN_LEFT, 4); press(&g, IN_CROSS);    // Z
    shot_game(&g, "13c_editor_name");
    press(&g, IN_START);
    check(g.ed.screen == ED_MENU && !strcmp(g.ed.kb.text, "FRITZ"), "the keyboard types a name");
    press(&g, IN_DOWN);
    press(&g, IN_RIGHT);                     // colours: palette 1
    shot_game(&g, "13b_editor_menu");
    presses(&g, IN_DOWN, 4);
    press(&g, IN_CROSS);                     // save
    int n = plat_read_file(LEVELS_DIR "/slot01.json", text, (int)sizeof(text));
    int ok = n > 0 && level_parse(&lv, text, n) == LEVEL_OK;
    snprintf(what, sizeof(what), "SAVE writes slot01.json (%d objects, name %s)", ok ? lv.nobj : -1, ok ? lv.name : "?");
    check(ok && lv.nobj == 4 && !strcmp(lv.name, "FRITZ") && !strcmp(lv.author, "FRITZ") &&
          !memcmp(&lv.colors, &LEVEL_PALETTES[1], sizeof(LevelColors)), what);
    int spike = level_find(&lv, 9, 0), pasted = level_find(&lv, 21, 0);
    check(spike >= 0 && lv.objs[spike].type == OBJ_SPIKE && lv.objs[spike].rot == 1 && pasted >= 0 &&
          lv.objs[pasted].rot == 1 && level_find(&lv, 12, 0) < 0, "rotation, undo and paste all landed in the file");

    press(&g, IN_CIRCLE);
    press(&g, IN_START);                     // playtest from the cursor at (20, 0)
    check(g.state == ST_PLAY && g.playtest && g.run.practice && g.run.b.x > 20 * TILE && g.run.b.x < 21 * TILE,
          "Options playtests from the cursor in practice mode");
    for (int f = 0; f < 30; f++)
        game_step(&g, 0);
    shot_game(&g, "13d_editor_playtest");
    game_step(&g, IN_START);
    game_step(&g, 0);
    presses(&g, IN_UP, 1);
    press(&g, IN_CROSS);
    check(g.state == ST_EDITOR && g.ed.screen == ED_EDIT, "BACK TO EDITOR returns to editing");
    press(&g, IN_R2);
    press(&g, IN_UP);
    press(&g, IN_CROSS);                     // save and exit
    check(g.ed.screen == ED_SLOTS && g.ed.slot_used[0] && !strcmp(g.ed.slot_names[0], "FRITZ"),
          "SAVE AND EXIT goes back to the slots");
    shot_game(&g, "13e_editor_slots");
    press(&g, IN_CIRCLE);
    check(g.state == ST_TITLE, "Circle on the slots goes back to the title");

    g.title_sel = TITLE_LEVELS;
    press(&g, IN_CROSS);
    int found = -1;
    for (int i = 0; i < g.nlevels; i++)
        if (!strcmp(g.levels[i].file, "slot01.json"))
            found = i;
    check(found >= 0 && !strcmp(g.levels[found].name, "FRITZ") && !strcmp(g.levels[found].author, "FRITZ"),
          "the saved level shows up in LEVELS by FRITZ");
    press(&g, IN_CIRCLE);

    g.title_sel = TITLE_EDITOR;
    press(&g, IN_CROSS);
    press(&g, IN_SQUARE);
    check(g.ed.slot_used[0], "one Square does not delete a level");
    press(&g, IN_SQUARE);
    check(!g.ed.slot_used[0] && plat_read_file(LEVELS_DIR "/slot01.json", text, 16) < 0, "a second Square deletes it");
    press(&g, IN_CIRCLE);
}

// Signs: Triangle changes the words, and they are saved.
static void test_editor_signs(void)
{
    static Game g;
    static Level lv;
    static char text[1 << 20];
    reset_data();
    game_init(&g, 9);
    game_step(&g, 0);
    g.title_sel = TITLE_EDITOR;
    press(&g, IN_CROSS);
    press(&g, IN_CROSS);
    for (int k = 0; k < 200 && editor_brush_type(&g.ed) != OBJ_SIGN; k++)
        press(&g, IN_L1);
    presses(&g, IN_UP, 3);
    press(&g, IN_CROSS);
    press(&g, IN_TRIANGLE);
    press(&g, IN_TRIANGLE);
    shot_game(&g, "13h_editor_sign");
    press(&g, IN_R2);
    presses(&g, IN_DOWN, 10);
    press(&g, IN_CROSS);
    int n = plat_read_file(LEVELS_DIR "/slot01.json", text, (int)sizeof(text));
    int k = n > 0 && level_parse(&lv, text, n) == LEVEL_OK ? level_find(&lv, 8, 3) : -1;
    check(k >= 0 && lv.objs[k].type == OBJ_SIGN && lv.objs[k].param == 2 && !strcmp(SIGN_TEXTS[2], "JUMP ONTO THE BLOCKS"),
          "a sign placed in the editor keeps the words Triangle picked");
    press(&g, IN_CIRCLE);
}

// The groups panel: blocks go in a group and a move trigger moves it.
static void test_editor_groups(void)
{
    static Game g;
    static Level lv;
    static char text[1 << 20];
    reset_data();
    game_init(&g, 8);
    game_step(&g, 0);
    g.title_sel = TITLE_EDITOR;
    press(&g, IN_CROSS);
    press(&g, IN_CROSS);
    press(&g, IN_R2);
    presses(&g, IN_DOWN, 9);
    press(&g, IN_CROSS);
    check(g.ed.screen == ED_GROUPS, "GROUPS AND MOVES opens its panel");
    presses(&g, IN_RIGHT, 2);                       // new objects in group 2
    press(&g, IN_DOWN); press(&g, IN_RIGHT);        // the trigger moves group 2
    press(&g, IN_DOWN); press(&g, IN_RIGHT);        // right 1
    press(&g, IN_DOWN); press(&g, IN_RIGHT);        // up 3
    shot_game(&g, "13f_editor_groups");
    press(&g, IN_CIRCLE);
    press(&g, IN_CIRCLE);
    presses(&g, IN_RIGHT, 4);
    press(&g, IN_CROSS);                            // block at (12, 0) in group 2
    press(&g, IN_UP);
    press(&g, IN_CROSS);                            // and (12, 1)
    press(&g, IN_DOWN);
    presses(&g, IN_LEFT, 6);
    for (int k = 0; k < 200 && editor_brush_type(&g.ed) != OBJ_MOVE; k++)
        press(&g, IN_R1);                           // MOVE TRIGGER in the palette
    press(&g, IN_CROSS);                            // trigger at (6, 0)
    presses(&g, IN_RIGHT, 2);
    shot_game(&g, "13g_editor_move_trigger");
    press(&g, IN_R2);
    presses(&g, IN_DOWN, 10);
    press(&g, IN_CROSS);                            // save
    int n = plat_read_file(LEVELS_DIR "/slot01.json", text, (int)sizeof(text));
    int ok = n > 0 && level_parse(&lv, text, n) == LEVEL_OK;
    int b = ok ? level_find(&lv, 12, 1) : -1, m = ok ? level_find(&lv, 6, 0) : -1;
    check(b >= 0 && lv.objs[b].type == OBJ_BLOCK && lv.objs[b].group == 2 && lv.nmoving == 2 && m >= 0 &&
          lv.objs[m].type == OBJ_MOVE && lv.objs[m].group == 2 && lv.objs[m].dx == 1 && lv.objs[m].dy == 3 &&
          lv.objs[m].time == 60, "the editor saves grouped blocks and a move trigger with its settings");
    press(&g, IN_CIRCLE);
}

// Editor upgrades: level map, info line, ready-made pieces, clipboard between
// slots, selection to a group, changing a move trigger, music from here, groups of the palette.
static void test_editor_upgrades(void)
{
    static Game g;
    static Level lv;
    static char text[1 << 20];
    char what[160];
    // Every ready-made piece can be played through on its own, comfortably.
    int good = 0;
    for (int k = 0; k < editor_templates(); k++) {
        level_parse(&lv, "{\"objects\":[{\"t\":\"end\",\"x\":60,\"y\":0}]}", 44);
        editor_template_put(&lv, k, 12, 0);
        int ok = solve(&lv, 2, 0, 50000000L) == 1, comfort = 0;
        for (int p = 0; ok && p < 6; p++)
            comfort += solve(&lv, 6, p ? p : 6, 20000000L) == 1;
        good += ok && comfort >= 5;
        printf("     ready-made piece %d: finishable %d, comfort %d/6\n", k, ok, comfort);
    }
    snprintf(what, sizeof(what), "all %d ready-made pieces can be played through (comfort 5/6 or more)", editor_templates());
    check(good == editor_templates() && good >= 6, what);
    level_parse(&lv, "{\"objects\":[{\"t\":\"portal_speed_2\",\"x\":50,\"y\":0},{\"t\":\"end\",\"x\":200,\"y\":0}]}", 87);
    int ft = editor_frames_to(&lv, 100);
    snprintf(what, sizeof(what), "music from here starts where the player would be (%d frames to column 100 past a 2x portal)", ft);
    check(ft > 510 && ft < 530, what);

    reset_data();
    write_text(LEVELS_DIR "/slot01.json", "{\"name\":\"LONG ONE\",\"objects\":[{\"t\":\"block\",\"x\":12,\"y\":0},"
               "{\"t\":\"move_trigger\",\"x\":14,\"y\":0,\"g\":1,\"dx\":0,\"dy\":2,\"f\":60},{\"t\":\"block\",\"x\":18,\"y\":0,\"g\":1},"
               "{\"t\":\"spike\",\"x\":150,\"y\":0},{\"t\":\"portal_ship\",\"x\":200,\"y\":1},{\"t\":\"block\",\"x\":230,\"y\":5},"
               "{\"t\":\"portal_cube\",\"x\":260,\"y\":1},{\"t\":\"coin\",\"x\":280,\"y\":2},{\"t\":\"end\",\"x\":320,\"y\":0}]}");
    game_init(&g, 121);
    game_step(&g, 0);
    g.title_sel = TITLE_EDITOR;
    press(&g, IN_CROSS);
    press(&g, IN_CROSS);
    press(&g, IN_L3);
    int map = g.ed.screen == ED_MAP && g.ed.map_x == 4;
    press(&g, IN_DOWN);
    presses(&g, IN_RIGHT, 3);
    for (int f = 0; f < 10; f++)
        game_step(&g, 0);
    shot_game(&g, "36_editor_map");
    int want = g.ed.map_x + 4;
    press(&g, IN_CROSS);
    snprintf(what, sizeof(what), "L3 opens the level map; X jumps the cursor to the box (column %d)", g.ed.cx);
    check(map && want > 200 && g.ed.screen == ED_EDIT && g.ed.cx == want, what);

    // Triangle on the move trigger changes it.
    g.ed.cx = 14;
    g.ed.cy = 0;
    for (int f = 0; f < 30; f++)
        game_step(&g, 0);
    shot_game(&g, "36b_editor_info");
    press(&g, IN_TRIANGLE);
    int trig = g.ed.screen == ED_GROUPS && g.ed.edit_trigger && g.ed.move_dy == 2 && g.ed.group_sel == 1;
    presses(&g, IN_DOWN, 2);
    presses(&g, IN_LEFT, 4);                        // dy 2 -> -2
    shot_game(&g, "36e_editor_trigger_edit");
    press(&g, IN_CIRCLE);
    trig &= g.ed.screen == ED_EDIT && !g.ed.edit_trigger;

    // Select both blocks near the start and put them in group 3.
    g.ed.cx = 12;
    game_step(&g, 0);
    game_step(&g, IN_L2);
    for (int k = 0; k < 6; k++) {
        game_step(&g, IN_L2 | IN_RIGHT);
        game_step(&g, IN_L2);
    }
    game_step(&g, 0);
    press(&g, IN_R2);
    presses(&g, IN_DOWN, 15);
    presses(&g, IN_RIGHT, 3);
    shot_game(&g, "36d_editor_menu");
    press(&g, IN_CROSS);
    int grouped = g.ed.screen == ED_EDIT && g.ed.has_sel && g.ed.sel_x0 == 12 && g.ed.sel_x1 == 18;

    // Copy the selection, then a ready-made piece at (40, 0).
    press(&g, IN_R2);
    presses(&g, IN_DOWN, 2);
    press(&g, IN_CROSS);                            // copy
    g.ed.cx = 40;
    press(&g, IN_R2);
    presses(&g, IN_DOWN, 13);
    press(&g, IN_CROSS);
    int tmpl = g.ed.screen == ED_TEMPLATES;
    press(&g, IN_RIGHT);                            // SPIKE PIT
    shot_game(&g, "36c_editor_templates");
    press(&g, IN_CROSS);
    press(&g, IN_R2);
    presses(&g, IN_DOWN, 10);
    press(&g, IN_CROSS);                            // save
    read_text(LEVELS_DIR "/slot01.json", text, (int)sizeof(text));
    level_parse(&lv, text, (int)strlen(text));
    int m = level_find(&lv, 14, 0), b1 = level_find(&lv, 12, 0), b2 = level_find(&lv, 18, 0), pit = level_find(&lv, 43, 0);
    check(trig && m >= 0 && lv.objs[m].dy == -2 && lv.objs[m].group == 1, "Triangle on a move trigger changes what it does");
    check(grouped && b1 >= 0 && b2 >= 0 && lv.objs[b1].group == 3 && lv.objs[b2].group == 3 && lv.objs[m].group == 1,
          "SELECTION TO GROUP puts the selected objects (not the trigger) in a group");
    check(tmpl && pit >= 0 && lv.objs[pit].type == OBJ_SPIKE && level_find(&lv, 40, 0) >= 0,
          "READY-MADE PIECES puts a spike pit at the cursor");

    // Music from here.
    press(&g, IN_DOWN);
    press(&g, IN_DOWN);
    press(&g, IN_DOWN);
    press(&g, IN_DOWN);
    press(&g, IN_CROSS);
    int music = g.ed.music_on && g.ed.screen == ED_EDIT;
    press(&g, IN_R2);
    press(&g, IN_UP);
    press(&g, IN_CROSS);                            // save and exit (stops the music)
    check(music && !g.ed.music_on && g.ed.screen == ED_SLOTS, "HEAR THE MUSIC FROM HERE plays; leaving stops it");

    // The clipboard is still there in another slot, even after the title.
    press(&g, IN_CIRCLE);
    g.title_sel = TITLE_EDITOR;
    press(&g, IN_CROSS);
    press(&g, IN_RIGHT);
    press(&g, IN_CROSS);                            // slot 2, a new level
    g.ed.cx = 10;
    press(&g, IN_R2);
    presses(&g, IN_DOWN, 3);
    press(&g, IN_CROSS);                            // paste
    press(&g, IN_R2);
    presses(&g, IN_DOWN, 10);
    press(&g, IN_CROSS);
    read_text(LEVELS_DIR "/slot02.json", text, (int)sizeof(text));
    level_parse(&lv, text, (int)strlen(text));
    check(g.ed.nclip >= 3 && level_find(&lv, 10, 0) >= 0 && level_find(&lv, 16, 0) >= 0,
          "the clipboard survives leaving the editor and pastes into another slot");
    press(&g, IN_CIRCLE);

    // L2 with L1 or R1 jumps between groups of objects.
    int b0 = g.ed.brush;
    game_step(&g, 0);
    game_step(&g, IN_L2);
    game_step(&g, IN_L2 | IN_R1);
    int next = g.ed.brush;
    game_step(&g, IN_L2);
    game_step(&g, IN_L2 | IN_R1);
    game_step(&g, 0);
    snprintf(what, sizeof(what), "L2 + R1 jumps to the next group of objects (%d, %d, %d) without selecting", b0, next, g.ed.brush);
    check(b0 == 0 && next == 2 && g.ed.brush == 6 && !g.ed.selecting, what);
    shot_game(&g, "36f_editor_palette_group");
}

// Remix: Triangle on the level select copies a level into an empty slot.
static void test_remix(void)
{
    static Game g;
    static Level lv, orig;
    static char text[1 << 20];
    char what[160];
    reset_data();
    write_text(LEVELS_DIR "/slot01.json", "{\"name\":\"MINE\",\"objects\":[{\"t\":\"end\",\"x\":30,\"y\":0}]}");
    write_text(LEVELS_DIR "/slot02.json", "{\"name\":\"MINE TOO\",\"objects\":[{\"t\":\"end\",\"x\":30,\"y\":0}]}");
    game_init(&g, 131);
    game_step(&g, 0);
    open_world(&g);
    g.level_sel = bi("01_sunny_hop");
    press(&g, IN_TRIANGLE);
    shot_game(&g, "37_remix_ask");
    int asked = g.remix_ask;
    press(&g, IN_CIRCLE);
    int cancelled = !g.remix_ask && g.state == ST_LEVELS;
    press(&g, IN_TRIANGLE);
    press(&g, IN_CROSS);
    for (int f = 0; f < 20; f++)
        game_step(&g, 0);
    shot_game(&g, "37b_remix_editor");
    int n = read_text(LEVELS_DIR "/slot03.json", text, (int)sizeof(text));
    level_parse(&orig, BUILTIN_JSON[bi("01_sunny_hop")], (int)strlen(BUILTIN_JSON[bi("01_sunny_hop")]));
    int ok = n > 0 && level_parse(&lv, text, n) == LEVEL_OK;
    snprintf(what, sizeof(what), "Triangle, X remixes SUNNY HOP into the first empty slot (slot %d, \"%s\") and opens it",
             g.ed.slot + 1, ok ? lv.name : "?");
    check(asked && cancelled && g.state == ST_EDITOR && g.ed.screen == ED_EDIT && g.ed.slot == 2 && ok &&
          !strcmp(lv.name, "SUNNY HOP REMIX") && !strcmp(lv.author, "FRITZ") && lv.stars == 0 && lv.nobj == orig.nobj &&
          level_hash(&lv) == level_hash(&orig), what);
    // Every slot taken: it says so and stays.
    for (int i = 0; i < EDIT_SLOTS; i++) {
        char path[160];
        snprintf(path, sizeof(path), LEVELS_DIR "/slot%02d.json", i + 1);
        write_text(path, "{\"name\":\"FULL\",\"objects\":[{\"t\":\"end\",\"x\":30,\"y\":0}]}");
    }
    game_init(&g, 132);
    game_step(&g, 0);
    open_world(&g);
    press(&g, IN_TRIANGLE);
    press(&g, IN_CROSS);
    check(g.state == ST_LEVELS && g.msg_frames > 0 && strstr(g.msg, "FULL") != NULL, "with every slot taken, remix says so");
}

// FRITZ WORLD: his levels in order; beating one opens the next.
static void test_world(void)
{
    static Game g;
    static Level lv;
    char buf[4096];
    static const char ONE[] = "{\"name\":\"ONE\",\"objects\":[{\"t\":\"spike\",\"x\":14,\"y\":0},{\"t\":\"end\",\"x\":30,\"y\":0}]}";
    static const char FOUR[] = "{\"name\":\"FOUR\",\"objects\":[{\"t\":\"spike\",\"x\":16,\"y\":0},{\"t\":\"end\",\"x\":30,\"y\":0}]}";
    reset_data();
    write_text(LEVELS_DIR "/slot01.json", ONE);
    write_text(LEVELS_DIR "/slot02.json", "{\"name\":\"TWO\",\"objects\":[{\"t\":\"end\",\"x\":30,\"y\":0}]}");
    write_text(LEVELS_DIR "/slot04.json", FOUR);
    game_init(&g, 141);
    game_step(&g, 0);
    g.title_sel = TITLE_WORLD;
    press(&g, IN_CROSS);
    check(g.state == ST_WORLD && g.world.n == 0 && !g.world.build, "FRITZ WORLD opens, empty at first");
    shot_game(&g, "38a_world_empty");
    press(&g, IN_TRIANGLE);
    press(&g, IN_CROSS);
    press(&g, IN_RIGHT);
    press(&g, IN_CROSS);
    press(&g, IN_RIGHT);
    press(&g, IN_CROSS);
    int added = g.world.n == 3 && !strcmp(g.world.files[0], "slot01.json") && !strcmp(g.world.files[1], "slot02.json") &&
                !strcmp(g.world.files[2], "slot04.json");
    press(&g, IN_L1);                               // FOUR (just added, under the cursor) moves before TWO
    press(&g, IN_UP);
    int changed = strcmp(g.world.files[1], "slot04.json") != 0;
    press(&g, IN_DOWN);
    shot_game(&g, "38b_world_build");
    check(added && changed && g.world.sel == 1 && !strcmp(g.world.files[1], "slot04.json") &&
          !strcmp(g.world.files[2], "slot02.json"), "building: X adds his levels, L1 moves one, up and down change it");
    press(&g, IN_TRIANGLE);
    read_text(WORLD_FILE, buf, sizeof(buf));
    check(!g.world.build && strstr(buf, "reached 0\nlevel slot01.json\nlevel slot04.json\nlevel slot02.json\n") != NULL,
          "Triangle saves the path in world.txt");

    press(&g, IN_RIGHT);
    check(g.world.sel == 0 && g.world.msg_frames > 0, "only the first level is open");
    level_parse(&lv, ONE, (int)strlen(ONE));
    solve(&lv, 2, 0, 50000000L);
    game_step(&g, 0);
    game_step(&g, IN_CROSS);
    int playing = g.state == ST_PLAY && g.world_run && !strcmp(g.levels[g.level_sel].file, "slot01.json");
    for (int t = 0; t < ntrace; t++)
        game_step(&g, trace[t]);
    for (int f = 0; f < 180 && g.state == ST_PLAY; f++)
        game_step(&g, 0);
    for (int f = 0; f < 40; f++)
        game_step(&g, 0);
    shot_game(&g, "38c_world_unlocked");
    check(playing && g.state == ST_COMPLETE && g.world_result == 1 && g.world.reached == 1,
          "beating the first level opens the next");
    press(&g, IN_CIRCLE);
    shot_game(&g, "38_world");
    check(g.state == ST_WORLD && g.world.sel == 1, "Circle goes back to FRITZ WORLD, on the newly open level");

    // A practice finish opens nothing; Options twice starts over; it is all saved.
    level_parse(&lv, FOUR, (int)strlen(FOUR));
    solve(&lv, 2, 0, 50000000L);
    game_step(&g, 0);
    game_step(&g, IN_SQUARE);
    for (int t = 0; t < ntrace + 180 && g.state == ST_PLAY; t++)
        game_step(&g, t < ntrace ? trace[t] : 0);
    int practiced = g.state == ST_COMPLETE && g.run.practice && g.world.reached == 1;
    press(&g, IN_CIRCLE);
    game_init(&g, 142);
    g.title_sel = TITLE_WORLD;
    press(&g, IN_CROSS);
    int kept = g.world.reached == 1 && g.world.sel == 1;
    press(&g, IN_START);
    press(&g, IN_START);
    check(practiced && kept && g.world.reached == 0, "practice opens nothing; progress is saved; Options twice starts over");
    press(&g, IN_CIRCLE);
}

// His title screen: his words under the logo, his newest level, his icon on the cube.
static void test_title(void)
{
    static Game g;
    char buf[4096], what[160];
    reset_data();
    write_text(ICONS_FILE, "icon1 0000000000000000003003000002000000000000011110000000000000000000\n");
    game_init(&g, 151);
    game_step(&g, 0);
    press(&g, IN_TRIANGLE);
    int typing = g.state == ST_TAGLINE;
    presses(&g, IN_RIGHT, 7);
    press(&g, IN_CROSS);                            // H
    press(&g, IN_RIGHT);
    press(&g, IN_CROSS);                            // I
    shot_game(&g, "39b_title_tagline");
    press(&g, IN_START);
    read_text(TITLE_FILE, buf, sizeof(buf));
    check(typing && g.state == ST_TITLE && !strcmp(g.tagline, "HI") && strstr(buf, "tagline HI\n") != NULL,
          "Triangle on the title writes his words under the logo (saved in title.txt)");
    press(&g, IN_RIGHT);
    int across = g.title_sel == TITLE_EDITOR;
    presses(&g, IN_DOWN, 4);
    press(&g, IN_RIGHT);
    check(across && g.title_sel == TITLE_BADGES, "Left and Right jump between the PLAY and MAKE columns");

    g.title_sel = TITLE_EDITOR;
    press(&g, IN_CROSS);
    presses(&g, IN_RIGHT, 2);
    press(&g, IN_CROSS);                            // slot 3
    press(&g, IN_R2);
    press(&g, IN_UP);
    press(&g, IN_CROSS);                            // save and exit
    press(&g, IN_CIRCLE);
    read_text(TITLE_FILE, buf, sizeof(buf));
    check(g.state == ST_TITLE && !strcmp(g.newest, "slot03.json") && strstr(buf, "newest slot03.json") != NULL,
          "the title remembers his newest level");
    game_init(&g, 152);
    game_step(&g, 0);
    g.frame = 48 * 30 - 1;
    game_step(&g, 0);
    shot_game(&g, "39_title");
    snprintf(what, sizeof(what), "the title shows his words after a restart and the bouncing cube wears his icon (eye %06x)",
             fb[790 * SCREEN_W + 246] & 0xFFFFFF);
    check(!strcmp(g.tagline, "HI") && g.look.v[LOOK_FACE] == 0 && (fb[790 * SCREEN_W + 246] & 0xFFFFFF) == 0, what);
}

// --- Secret coins --------------------------------------------------------------

static int find_level(const Game *g, const char *key)
{
    for (int i = 0; i < g->nlevels; i++)
        if (!strcmp(g->levels[i].key, key))
            return i;
    return -1;
}

static void test_coins(void)
{
    static Game g;
    char buf[4096], line[200];
    Run r;
    MK(1, "..........$.........$...........|");
    begin(&r, 0);
    int ev = run_for(&r, 400, 0);
    check(ev == EV_COMPLETE && tl.ncoins == 2 && r.b.coins == 3, "running through coins picks them up, numbered left to right");
    MK(1, "..........$.......................|", "..................................", "..................................");
    begin(&r, 0);
    run_for(&r, 400, 0);
    check(r.b.coins == 0, "a coin up high is missed without a jump");

    // A finish in normal mode saves the coins; practice does not.
    reset_data();
    write_text(LEVELS_DIR "/coin.json", "{\"name\":\"COIN RUN\",\"objects\":[{\"t\":\"coin\",\"x\":12,\"y\":0},"
                                        "{\"t\":\"coin\",\"x\":16,\"y\":3},{\"t\":\"end\",\"x\":30,\"y\":0}]}");
    write_text(LEVELS_DIR "/coin2.json", "{\"name\":\"COIN PRACTICE\",\"objects\":[{\"t\":\"coin\",\"x\":12,\"y\":0},"
                                         "{\"t\":\"end\",\"x\":30,\"y\":0}]}");
    game_init(&g, 21);
    game_step(&g, 0);
    open_world(&g);
    g.level_sel = find_level(&g, "file:coin.json");
    press(&g, IN_CROSS);
    for (int f = 0; f < 400 && g.state == ST_PLAY; f++)
        game_step(&g, 0);
    for (int f = 0; f < 40; f++)
        game_step(&g, 0);
    read_text(COINS_FILE, buf, sizeof(buf));
    check(g.state == ST_COMPLETE && g.new_coins == 1 && strstr(buf, "1 file:coin.json") != NULL && game_coins(&g) == 0,
          "finishing saves the coins picked up in coins.txt; player-made coins do not count for unlocks");
    shot_game(&g, "09d_complete_coins");
    press(&g, IN_CIRCLE);
    g.level_sel = find_level(&g, "file:coin2.json");
    press(&g, IN_SQUARE);
    for (int f = 0; f < 400 && g.state == ST_PLAY; f++)
        game_step(&g, 0);
    check(g.state == ST_COMPLETE && g.levels[g.level_sel].coins == 0 && g.new_coins == 0,
          "coins picked up in practice are not saved");

    // Built-in coins count for unlocks and show on the level select.
    snprintf(line, sizeof(line), "1 file:coin.json\n5 builtin:01_sunny_hop\n");
    write_text(COINS_FILE, line);
    game_init(&g, 22);
    game_step(&g, 0);
    check(game_coins(&g) == 2 && g.levels[find_level(&g, "file:coin.json")].coins == 1, "coins.txt is read back");
    press(&g, IN_CROSS);
    shot_game(&g, "02g_levels_worlds_coins");
    g.lworld_sel = 0;
    press(&g, IN_CROSS);
    g.level_sel = find_level(&g, "builtin:01_sunny_hop");
    shot_game(&g, "02d_levels_coins");
    press(&g, IN_CIRCLE);
    press(&g, IN_CIRCLE);
    g.title_sel = TITLE_CUSTOMIZE;
    press(&g, IN_CROSS);
    presses(&g, IN_LEFT, LOOK_COUNT[LOOK_COL1] - 10);  // colour 1: GOLD (10), counting back from the end
    check(!strcmp(LOOK_OPTIONS[LOOK_COL1][g.browse.v[LOOK_COL1]].name, "GOLD") && g.look.v[LOOK_COL1] != g.browse.v[LOOK_COL1],
          "GOLD needs 3 coins: not worn with 2");
    shot_game(&g, "14c_customize_coin_lock");
    press(&g, IN_CIRCLE);
    snprintf(line, sizeof(line), "7 builtin:01_sunny_hop\n");
    write_text(COINS_FILE, line);
    game_init(&g, 23);
    game_step(&g, 0);
    g.title_sel = TITLE_CUSTOMIZE;
    press(&g, IN_CROSS);
    presses(&g, IN_LEFT, LOOK_COUNT[LOOK_COL1] - 10);
    check(!strcmp(LOOK_OPTIONS[LOOK_COL1][g.look.v[LOOK_COL1]].name, "GOLD"), "with 3 coins GOLD is worn");
    press(&g, IN_CIRCLE);

    // The coin counter while playing, just after picking one up.
    static Level lv;
    level_parse(&lv, BUILTIN_JSON[bi("01_sunny_hop")], (int)strlen(BUILTIN_JSON[bi("01_sunny_hop")]));
    want_coins(&lv, 1);
    int ok = solve(&lv, 2, 0, 400000000L) == 1;
    want_coins(&lv, 0);
    run_start(&r, &lv, 0, 0);
    r.coins_had = 4;
    int shot = 0;
    for (int t = 0; ok && t < ntrace && !shot; t++) {
        run_step(&r, trace[t]);
        if (r.b.coins & 1) {
            for (int k = 0; k < 6; k++)
                run_step(&r, trace[t + 1 + k]);
            shot_run(&r, "20_coin_pickup");
            shot = 1;
        }
    }
    check(shot, "a solved run picks up coin 1 of level 1 (screenshot)");
}

// --- Stars, Customize, Stats, recording mode -----------------------------------

static void test_customize(void)
{
    static Game g;
    static Level lv;
    char line[200], buf[4096];
    reset_data();
    level_parse(&lv, BUILTIN_JSON[bi("01_sunny_hop")], (int)strlen(BUILTIN_JSON[bi("01_sunny_hop")]));
    snprintf(line, sizeof(line), "3 100 0 builtin:01_sunny_hop\n");
    write_text(PROGRESS_FILE, line);
    game_init(&g, 9);
    game_step(&g, 0);
    int stars = game_stars(&g);
    check(stars == lv.stars && stars > 0, "finishing a built-in level earns its stars");
    shot_game(&g, "01c_title_stars");

    g.title_sel = TITLE_CUSTOMIZE;
    press(&g, IN_CROSS);
    check(g.state == ST_CUSTOMIZE, "CUSTOMIZE opens");
    press(&g, IN_RIGHT);
    check(g.look.v[LOOK_COL1] == 1, "a free colour is worn at once");
    presses(&g, IN_DOWN, 2);
    int equipped = 1;
    for (int k = 1; k < LOOK_COUNT[LOOK_FACE]; k++) {
        press(&g, IN_RIGHT);
        const LookOption *o = &LOOK_OPTIONS[LOOK_FACE][k];
        int free = o->stars <= stars && o->coins <= game_coins(&g) && (!o->badge || (g.badges >> (o->badge - 1) & 1)) &&
                   (!FACE_IS_ICON(k) || !icon_empty(k - FACE_ICON0));
        if (free != (g.look.v[LOOK_FACE] == k))
            equipped = 0;
        if (k == 2)
            shot_game(&g, "14_customize");
    }
    check(equipped, "faces are worn only when there are enough stars");
    presses(&g, IN_DOWN, 3);
    press(&g, IN_RIGHT);
    for (int f = 0; f < 6; f++)
        game_step(&g, 0);
    shot_game(&g, "14b_customize_death");
    press(&g, IN_CIRCLE);
    read_text(CUSTOM_FILE, buf, sizeof(buf));
    Look worn = g.look;
    game_init(&g, 10);
    check(strstr(buf, "col1 1") && !memcmp(&g.look, &worn, sizeof(Look)), "the look is saved and read back");

    // Earn the stars of level 1 for real, with the solver's button presses.
    reset_data();
    game_init(&g, 11);
    level_parse(&blv_scratch, BUILTIN_JSON[bi("01_sunny_hop")], (int)strlen(BUILTIN_JSON[bi("01_sunny_hop")]));
    int solved = solve(&blv_scratch, 2, 0, 400000000L) == 1;
    game_step(&g, 0);
    open_world(&g);
    g.level_sel = find_level(&g, "builtin:01_sunny_hop");
    game_step(&g, 0);
    game_step(&g, IN_CROSS);
    int f;
    for (f = 0; f < 40; f++) {
        game_step(&g, trace[f]);
        if (f == 39)
            shot_game(&g, "15_intro_card");
    }
    for (; f < ntrace; f++)
        game_step(&g, trace[f]);
    for (f = 0; f < 140 && g.state == ST_PLAY; f++)
        game_step(&g, 0);
    for (f = 0; f < 32; f++)
        game_step(&g, 0);
    check(solved && g.state == ST_COMPLETE && g.new_stars == lv.stars && game_stars(&g) == lv.stars,
          "beating level 1 shows +stars and adds them to the total");
    check((g.badges >> B_FIRST_WIN & 1) && (g.badges >> B_NO_PRACTICE & 1) && g.nbadge_toasts >= 1,
          "the first win earns FIRST WIN and NO PRACTICE, with a toast");
    for (f = 0; f < 6; f++)
        game_step(&g, 0);
    shot_game(&g, "21_badge_toast");
    shot_game(&g, "09c_stars_earned");
    press(&g, IN_CIRCLE);
    shot_game(&g, "02c_levels_earned");
    g.clean_hud = 1;
    press(&g, IN_RIGHT);
    press(&g, IN_CROSS);
    for (f = 0; f < 200; f++)
        game_step(&g, 0);
    shot_game(&g, "16_recording_mode");
    game_step(&g, IN_START);
    game_step(&g, 0);
    presses(&g, IN_UP, 1);
    press(&g, IN_CROSS);
    press(&g, IN_CIRCLE);
    press(&g, IN_CIRCLE);
    read_text(STATS_FILE, buf, sizeof(buf));
    check(g.stats[STAT_ATTEMPTS] >= 2 && g.stats[STAT_WINS] == 1 && g.stats[STAT_JUMPS] > 10 &&
          strstr(buf, "wins 1") != NULL, "stats count attempts, jumps and wins and are saved");
    g.title_sel = TITLE_STATS;
    press(&g, IN_CROSS);
    check(g.state == ST_STATS, "STATS opens");
    shot_game(&g, "17_stats");
    press(&g, IN_CIRCLE);
}

// --- Icon maker ------------------------------------------------------------------

static void test_icons(void)
{
    static Game g;
    char buf[4096], what[160];
    reset_data();
    game_init(&g, 101);
    game_step(&g, 0);
    g.title_sel = TITLE_ICONS;
    press(&g, IN_CROSS);
    check(g.state == ST_ICONS && g.icons.icon == 0 && icon_empty(0) && g.icons.pen == PX_WHITE,
          "ICON MAKER opens on an empty MY ICON 1 with white paint");
    const uint8_t *p = icon_pixels(0);
    press(&g, IN_CROSS);                            // white nose at (3, 3)
    press(&g, IN_R1);                               // black
    press(&g, IN_UP);
    press(&g, IN_LEFT);
    press(&g, IN_CROSS);                            // eye at (2, 2)
    presses(&g, IN_RIGHT, 3);
    press(&g, IN_CROSS);                            // eye at (5, 2)
    presses(&g, IN_L1, 2);                          // colour 2
    presses(&g, IN_DOWN, 3);
    presses(&g, IN_LEFT, 3);
    game_step(&g, 0);                               // mouth (2..5, 5) painted holding X
    game_step(&g, IN_CROSS);
    for (int k = 0; k < 3; k++) {
        game_step(&g, IN_CROSS | IN_RIGHT);
        game_step(&g, IN_CROSS);
    }
    game_step(&g, 0);
    int smile = p[3 * 8 + 3] == PX_WHITE && p[2 * 8 + 2] == PX_BLACK && p[2 * 8 + 5] == PX_BLACK;
    for (int x = 2; x <= 5; x++)
        smile &= p[5 * 8 + x] == PX_COL2;
    press(&g, IN_SQUARE);
    int rubbed = p[5 * 8 + 5] == PX_CUBE;
    press(&g, IN_TRIANGLE);
    check(smile && rubbed && p[5 * 8 + 5] == PX_COL2, "X paints (held, along a line), Square rubs out, Triangle undoes");
    shot_game(&g, "34_icon_maker");
    press(&g, IN_START);
    read_text(CUSTOM_FILE, buf, sizeof(buf));
    check(g.look.v[LOOK_FACE] == FACE_ICON0 && strstr(buf, "face 10") != NULL, "Options wears the icon as his face");
    press(&g, IN_R2);
    press(&g, IN_START);
    check(g.icons.icon == 1 && g.look.v[LOOK_FACE] == FACE_ICON0, "R2 goes to MY ICON 2; an empty icon cannot be worn");
    press(&g, IN_TOUCH);
    press(&g, IN_L2);
    press(&g, IN_TOUCH);
    press(&g, IN_CIRCLE);
    read_text(ICONS_FILE, buf, sizeof(buf));
    check(g.state == ST_TITLE && !icon_empty(0) && strstr(buf, "icon1 00000000000000000030030000020000") != NULL,
          "Circle saves the icons and goes back (one Touchpad press does not clear)");

    game_init(&g, 102);
    game_step(&g, 0);
    check(icon_pixels(0)[2 * 8 + 2] == PX_BLACK && g.look.v[LOOK_FACE] == FACE_ICON0, "icons and the worn icon load again");
    draw_set_target(fb);
    look_set(&g.look);
    draw_gradient(0, 0);
    run_draw_cube(500, 500, 0);
    snprintf(what, sizeof(what), "the cube wears the icon: eye %06x, mouth %06x",
             fb[488 * SCREEN_W + 488] & 0xFFFFFF, fb[512 * SCREEN_W + 496] & 0xFFFFFF);
    check((fb[488 * SCREEN_W + 488] & 0xFFFFFF) == 0 &&
          (fb[512 * SCREEN_W + 496] & 0xFFFFFF) == LOOK_COLORS[g.look.v[LOOK_COL2]], what);

    g.title_sel = TITLE_CUSTOMIZE;
    press(&g, IN_CROSS);
    presses(&g, IN_DOWN, 2);
    press(&g, IN_RIGHT);
    check(g.look.v[LOOK_FACE] == FACE_ICON0 && g.browse.v[LOOK_FACE] == FACE_ICON0 + 1 && g.msg_frames > 0,
          "Customize: MY ICON 2 waits until it is drawn");
    shot_game(&g, "34b_customize_my_icon");
    press(&g, IN_LEFT);
    press(&g, IN_CIRCLE);
    press(&g, IN_CIRCLE);

    // Playing wearing it, and the ship and ball carry it too.
    g.title_sel = TITLE_LEVELS;
    open_world(&g);
    g.level_sel = bi("01_sunny_hop");
    press(&g, IN_CROSS);
    for (int f = 0; f < 200; f++)
        game_step(&g, 0);
    shot_game(&g, "34c_icon_in_play");
}

// --- Stock icons -------------------------------------------------------------------

static int stock_index(const char *name)
{
    for (int k = 0; k < STOCK_COUNT; k++)
        if (!strcmp(STOCK[k].opt.name, name))
            return k;
    return -1;
}

// Moves the STOCK ICONS cursor onto icon k with the controller: R1 to its set, then the d-pad.
static void stock_go(Game *g, int k)
{
    int p = 0;
    for (int j = 0; j < k; j++)
        p += STOCK[j].set == STOCK[k].set;
    do
        press(g, IN_R1);
    while (g->stock.set != STOCK[k].set);
    presses(g, IN_DOWN, p / 4);
    presses(g, IN_RIGHT, p % 4);
}

static int px_is(int x, int y, color_t c)
{
    return (fb[y * SCREEN_W + x] & 0xFFFFFF) == c;
}

static void test_stock(void)
{
    static Game g;
    char buf[4096], what[200];
    int secrets = 0, bad = 0, sets[32] = { 0 };
    for (int k = 0; k < STOCK_COUNT; k++) {
        const StockIcon *ic = &STOCK[k];
        int lit = 0;
        for (int i = 0; i < STOCK_PX * STOCK_PX; i++)
            lit += ic->px[i] != '0';
        if (strlen(ic->px) != STOCK_PX * STOCK_PX || lit < 24 || ic->set < 0 || ic->set >= STOCK_SET_COUNT)
            bad++;
        secrets += ic->secret;
        sets[ic->set]++;
    }
    for (int s = 0; s < STOCK_SET_COUNT; s++)
        bad += sets[s] < 8 || sets[s] > 16;
    for (int p = 0; p < PET_COUNT; p++)
        bad += strlen(PETS[p].px[0]) != 256 || strlen(PETS[p].px[1]) != 256 || !strcmp(PETS[p].px[0], PETS[p].px[1]);
    check(STOCK_COUNT == 96 && secrets == 16 && PET_COUNT == 8 && !bad && STOCK_COUNT <= STOCK_MAX &&
          LOOK_COUNT[LOOK_FACE] <= FACE_STOCK0, "96 stock icons (16 secret) in sets of 8 to 16, and 8 two-frame pets");

    // An old save (no stock key) loads unchanged.
    reset_data();
    stock_forget();
    write_text(CUSTOM_FILE, "col1 3\ncol2 4\nface 10\nship 1\ntrail 2\ndeath 1\n");
    write_text(ICONS_FILE, "icon1 0000000000300300000000000000000002000020002222000000000000000000\n");
    game_init(&g, 140);
    game_step(&g, 0);
    static const int OLD[LOOK_PARTS] = { 3, 4, 10, 1, 2, 1 };
    check(!memcmp(g.look.v, OLD, sizeof(OLD)) && g.look.stock == 0 && look_face(&g.look) == 10 && !icon_empty(0) &&
          icon_pixels(0)[10] == PX_BLACK, "an old custom.txt and icons.txt load unchanged, with no stock icon");

    g.title_sel = TITLE_CUSTOMIZE;
    press(&g, IN_CROSS);
    shot_game(&g, "60a_customize_triangle");
    press(&g, IN_TRIANGLE);
    check(g.state == ST_STOCK && g.stock.set == 0 && g.stock.sel == 0, "Triangle in Customize opens STOCK ICONS");
    press(&g, IN_CROSS);
    read_text(CUSTOM_FILE, buf, sizeof(buf));
    check(g.look.stock == 1 && look_face(&g.look) == FACE_STOCK0 && strstr(buf, "stock 1\n") && strstr(buf, "face 10\n") &&
          strstr(buf, "col1 3\n") && strstr(buf, "death 1\n"), "X wears a free icon; custom.txt keeps its keys and adds stock");
    press(&g, IN_LEFT);
    check(g.stock.set == STOCK_SET_COUNT - 1 && g.stock.sel == 3, "Left off the grid goes to the set before");
    press(&g, IN_RIGHT);
    check(g.stock.set == 0 && g.stock.sel == 0, "and Right off it comes back");

    int pika = stock_index("PIKACHU");
    stock_go(&g, pika);
    press(&g, IN_CROSS);
    check(pika >= 0 && stockui_icon(&g.stock) == pika && g.look.stock == pika + 1, "R1 and the d-pad reach PIKACHU; it is free");
    for (int f = 0; f < 20; f++)
        game_step(&g, 0);
    shot_game(&g, "60_stock_icons");

    static const char *const LOCKED[3] = { "LION", "ZAPPUP", "GOLD BOT" }, *const WHY[3] = { "STARS", "COINS", "BADGE" };
    for (int i = 0; i < 3; i++) {
        int k = stock_index(LOCKED[i]);
        stock_go(&g, k);
        press(&g, IN_CROSS);
        snprintf(what, sizeof(what), "%s waits for %s: \"%s\"", LOCKED[i], WHY[i], g.stock.msg);
        check(k >= 0 && g.look.stock == pika + 1 && g.stock.msg_frames > 0 && strstr(g.stock.msg, WHY[i]), what);
        if (i == 0)
            shot_game(&g, "60c_stock_locked");
    }
    g.badges |= 1u << B_STARS10;
    press(&g, IN_CROSS);
    check(g.look.stock == stock_index("GOLD BOT") + 1, "earning the badge unlocks GOLD BOT");

    int king = stock_index("KING");
    stock_go(&g, king);
    press(&g, IN_CROSS);
    check(king >= 0 && STOCK[king].secret && g.look.stock != king + 1 && strstr(g.stock.msg, "SECRET"),
          "a secret icon is a locked ? until an easter egg finds it");
    shot_game(&g, "60b_stock_secret");
    stock_find(king);
    press(&g, IN_CROSS);
    check(stock_found(king) && g.look.stock == king + 1, "once found, the secret icon can be worn");
    stock_forget();
    for (int f = 0; f < BADGE_TOAST_FRAMES + 20; f++)
        game_step(&g, 0);
    stock_go(&g, stock_index("TV BOT"));
    shot_game(&g, "60j_stock_robots");
    stock_go(&g, pika);
    press(&g, IN_CROSS);

    press(&g, IN_CIRCLE);
    check(g.state == ST_CUSTOMIZE && g.custom_row == LOOK_FACE && g.browse.stock == pika + 1,
          "Circle goes back to Customize on the FACE row, showing the icon");
    shot_game(&g, "60d_customize_stock");
    press(&g, IN_RIGHT);
    read_text(CUSTOM_FILE, buf, sizeof(buf));
    check(g.look.stock == 0 && g.look.v[LOOK_FACE] == 10 && strstr(buf, "stock 0\n") != NULL,
          "Right on FACE takes the stock icon off and goes back to his face");
    press(&g, IN_TRIANGLE);
    check(g.state == ST_STOCK && g.stock.set == 0 && g.stock.sel == 0, "with no stock icon worn the browser opens on the first set");
    stock_go(&g, pika);
    press(&g, IN_CROSS);
    press(&g, IN_CIRCLE);
    press(&g, IN_CIRCLE);
    game_init(&g, 141);
    game_step(&g, 0);
    check(g.look.stock == pika + 1 && g.look.v[LOOK_FACE] == 10, "the stock icon is saved and read back");
    shot_game(&g, "60f_title_stock");
    write_text(CUSTOM_FILE, "col1 2\nstock 999\n");
    game_init(&g, 142);
    int big = g.look.stock;
    write_text(CUSTOM_FILE, "stock -3\nface 2\n");
    game_init(&g, 143);
    check(big == 0 && g.look.stock == 0 && g.look.v[LOOK_FACE] == 2, "a stock number out of range loads as no stock icon");

    // The cube, any face at any size, and his colours in an icon.
    Look l = g.look;
    int bolt = stock_index("BOLT BOT"), mine = stock_index("MY BOT");
    color_t c1 = LOOK_COLORS[l.v[LOOK_COL1]], c2 = LOOK_COLORS[l.v[LOOK_COL2]], cyan = STOCK[bolt].pal[4];
    draw_set_target(fb);
    draw_gradient(0, 0);
    l.stock = bolt + 1;
    look_set(&l);
    run_draw_cube(500, 500, 0);
    run_draw_face(1000, 500, 2.0f, 0, FACE_STOCK0 + bolt);
    l.stock = mine + 1;
    look_set(&l);
    run_draw_cube(1400, 500, 0);
    snprintf(what, sizeof(what), "the cube wears the stock icon: eye %06x, clear %06x, face x2 %06x, his colour %06x",
             fb[494 * SCREEN_W + 490] & 0xFFFFFF, fb[530 * SCREEN_W + 470] & 0xFFFFFF, fb[488 * SCREEN_W + 980] & 0xFFFFFF,
             fb[486 * SCREEN_W + 1378] & 0xFFFFFF);
    check(cyan == RGB(70, 215, 255) && px_is(490, 494, cyan) && px_is(470, 530, c1) && px_is(980, 488, cyan) &&
          px_is(1378, 486, c2), what);
    for (int k = 0; k < STOCK_COUNT; k++) {
        l.stock = k + 1;
        look_set(&l);
        run_draw_pilot((float)(80 + (k % 16) * 110), (float)(620 + (k / 16) * 72), 0.8f, 0);
    }
    l.stock = pika + 1;
    look_set(&l);
    look_draw_ship(300, 300, 2.0f, 0);
    run_draw_pilot(700, 300, 0.5f, 0.3f);
    save_ppm("60e_stock_cube_ship");

    // Contact sheet through the game's own drawing: every icon and both frames of every pet, 4x.
    draw_gradient(RGB(20, 14, 40), RGB(20, 14, 40));
    for (int k = 0; k < STOCK_COUNT; k++) {
        int x = 40 + (k % 16) * 116, y = 30 + (k / 16) * 110;
        draw_rect(x - 6, y - 6, 76, 76, c1);
        stock_draw(k, x, y, 4, c1, c2);
    }
    for (int p = 0; p < PET_COUNT; p++)
        for (int f = 0; f < 2; f++)
            pet_draw(p, f, 40 + (p * 2 + f) * 116, 720, 4);
    save_ppm("60i_stock_sheet");

    // Wearing it in play, in a race, and the icon maker taking it off.
    look_set(&g.look);
    write_text(CUSTOM_FILE, "col1 0\ncol2 1\nface 0\n");
    game_init(&g, 144);
    game_step(&g, 0);
    g.look.stock = pika + 1;
    g.title_sel = TITLE_LEVELS;
    open_world(&g);
    g.level_sel = bi("01_sunny_hop");
    press(&g, IN_CROSS);
    for (int f = 0; f < 200; f++)
        game_step(&g, 0);
    shot_game(&g, "60g_stock_in_play");
    static const char RACE[] = "{\"name\":\"RACE\",\"objects\":[{\"t\":\"end\",\"x\":80,\"y\":0}]}";
    write_text(LEVELS_DIR "/race.json", RACE);
    game_init(&g, 145);
    game_step(&g, 0);
    g.look.stock = pika + 1;
    g.title_sel = TITLE_TWO_PLAYER;
    press(&g, IN_CROSS);
    press(&g, IN_CROSS);
    g.level_sel = find_level(&g, "file:race.json");
    game_step(&g, IN_P2_HERE);
    game_step(&g, IN_P2_HERE | IN_CROSS);
    for (int f = 0; f < RACE_COUNT_TEST + 30; f++)
        game_step(&g, IN_P2_HERE);
    check(g.state == ST_RACE && g.run.look.stock == pika + 1, "player 1 races wearing the stock icon");
    shot_game(&g, "60h_stock_race");
    game_init(&g, 146);
    game_step(&g, 0);
    g.look.stock = pika + 1;
    g.title_sel = TITLE_ICONS;
    press(&g, IN_CROSS);
    press(&g, IN_CROSS);
    press(&g, IN_START);
    check(g.look.stock == 0 && g.look.v[LOOK_FACE] == FACE_ICON0, "wearing a MY ICON takes the stock icon off");
}

// Example icons: read-only, and X copies one into his first empty icon.
static void test_icon_examples(void)
{
    static Game g;
    static char before[4096], after[4096], all[4096];
    char what[160];
    static const char MINE[] = "icon1 0000000000000000003003000002000000000000011110000000000000000000\n"
                               "icon2 3333333300000000000000000000000000000000000000000000000000000000\n";
    reset_data();
    write_text(ICONS_FILE, MINE);
    game_init(&g, 111);
    game_step(&g, 0);
    g.title_sel = TITLE_ICONS;
    press(&g, IN_CROSS);
    presses(&g, IN_R2, NUM_ICONS);
    check(g.icons.icon == NUM_ICONS, "R2 past MY ICON 6 goes on to the example icons");
    shot_game(&g, "34d_icon_example");
    uint8_t mirror[ICON_N * ICON_N];
    memcpy(mirror, icon_example(0), sizeof(mirror));
    press(&g, IN_SQUARE);
    press(&g, IN_TOUCH);
    press(&g, IN_TOUCH);
    press(&g, IN_TRIANGLE);
    press(&g, IN_START);
    check(!memcmp(icon_example(0), mirror, sizeof(mirror)) && icon_empty(2) && g.look.v[LOOK_FACE] == 0 &&
          g.icons.icon == NUM_ICONS && g.icons.msg_frames > 0, "an example cannot be painted, rubbed out, cleared or worn");
    game_step(&g, 0);
    for (int f = 0; f < 12; f++)
        game_step(&g, IN_CROSS);                    // X held: copies once, then paints nothing
    game_step(&g, 0);
    read_text(ICONS_FILE, after, sizeof(after));
    snprintf(what, sizeof(what), "X copies MIRROR into the first empty icon (MY ICON %d) and opens it; his two stay",
             g.icons.icon + 1);
    check(g.icons.icon == 2 && !memcmp(icon_pixels(2), mirror, sizeof(mirror)) &&
          strstr(after, "icon3 0000000011033011121331211113311101133110121331211103301100000000") &&
          !strncmp(after, MINE, strlen(MINE)), what);
    shot_game(&g, "34e_icon_example_copied");
    press(&g, IN_CROSS);
    check(icon_pixels(2)[3 * 8 + 3] == PX_WHITE, "after the copy X paints on it again");
    press(&g, IN_CIRCLE);

    // All six drawn: the copy says so and icons.txt does not change.
    int n = 0;
    for (int k = 0; k < NUM_ICONS; k++)
        n += snprintf(all + n, sizeof(all) - (size_t)n, "icon%d %063d1\n", k + 1, 0);
    write_text(ICONS_FILE, all);
    game_init(&g, 112);
    game_step(&g, 0);
    g.title_sel = TITLE_ICONS;
    press(&g, IN_CROSS);
    presses(&g, IN_L2, 1);                          // the last example
    read_text(ICONS_FILE, before, sizeof(before));
    press(&g, IN_CROSS);
    read_text(ICONS_FILE, after, sizeof(after));
    check(!strcmp(before, all) && !strcmp(after, all) && g.icons.icon == NUM_ICONS + NUM_ICON_EXAMPLES - 1 &&
          strstr(g.icons.msg, "ALL 6") != NULL, "with all six icons drawn, X on an example changes nothing and says so");
    shot_game(&g, "34f_icon_example_full");
    press(&g, IN_CIRCLE);
}

// --- Verify and suggested difficulty ----------------------------------------------

// Inside a new world the level list plays that world's theme.
static void test_world_themes(void)
{
    static Game g;
    game_init(&g, 5);
    g.state = ST_LEVELS;
    g.picking = 1;
    g.lworld_sel = 1;
    game_step(&g, 0);
    int cards = g.song;
    g.picking = 0;
    game_step(&g, 0);
    int sunrise = g.song;
    g.lworld_sel = 5;
    game_step(&g, 0);
    int cosmic = g.song;
    g.lworld_sel = 0;
    game_step(&g, 0);
    int first = g.song;
    g.lworld_sel = LWORLD_MINE;
    game_step(&g, 0);
    check(cards == SONG_MENU && sunrise == SONG_THEME1 && cosmic == SONG_THEME1 + 4 && first == SONG_MENU &&
          g.song == SONG_MENU, "each new world's level list plays its theme; the cards, WORLD 1 and MY LEVELS the menu song");
}

static void test_rate_verify(void)
{
    static Game g;
    static Level lv;
    static Rate r;
    static char text[1 << 16];
    char what[200];
    // The game's own rating, a slice a frame, on the built-in levels.
    static const char *const IDS[5] = { "01_sunny_hop", "02_candy_climb", "03_gravity_grotto", "04_rocket_run",
                                        "05_star_storm" };
    int score[5], rating[5], frames_max = 0, slice_max = 0;
    for (int i = 0; i < 5; i++) {
        level_parse(&lv, BUILTIN_JSON[bi(IDS[i])], (int)strlen(BUILTIN_JSON[bi(IDS[i])]));
        rate_start(&r, &lv);
        int frames = 0;
        long before = 0;
        while (rate_step(&r, RATE_STEPS) == RATE_BUSY && frames < 100000) {
            frames++;
            if (r.steps - before > slice_max)
                slice_max = (int)(r.steps - before);
            before = r.steps;
        }
        score[i] = r.score10;
        rating[i] = r.rating;
        if (frames > frames_max)
            frames_max = frames;
        printf("     %s: %s (score %.1f, comfort %d/6, relaxed %d/10, %d frames of rating)\n", IDS[i],
               RATING_NAMES[r.rating], r.score10 / 10.0, r.comfort, r.relaxed, frames);
    }
    snprintf(what, sizeof(what), "suggested difficulty: SUNNY HOP %s, STAR STORM %s, harder levels score higher "
             "(at most %d steps a frame, %.1f s for the slowest)", RATING_NAMES[rating[0]], RATING_NAMES[rating[4]],
             slice_max, frames_max / 60.0);
    check((rating[0] == RATING_EASY || rating[0] == RATING_NORMAL) &&
          (rating[4] == RATING_HARDER || rating[4] == RATING_INSANE) && score[0] < score[2] && score[2] < score[4] &&
          slice_max <= RATE_STEPS + 12 && frames_max < 60 * 60, what);
    MK(1, ".........#..", ".........#..", ".........#..........|");
    rate_start(&r, &tl);
    while (rate_step(&r, RATE_STEPS) == RATE_BUSY)
        ;
    check(r.rating == RATING_UNBEATEN, "a wall too tall to jump is UNBEATEN");
    check(rate_from_score10(rate_score10(6, 10, 1, 0, 30, 3600, 30, 0, 0)) == RATING_EASY &&
          rate_from_score10(rate_score10(1, 0, 3, 1, 90 * 60, 3600, 400 * 60, 1, 1)) == RATING_DEMON,
          "the rating formula matches the home server's");

    // In the editor: saving starts the rating, which lands in the menu and the file.
    reset_data();
    game_init(&g, 111);
    game_step(&g, 0);
    g.title_sel = TITLE_EDITOR;
    press(&g, IN_CROSS);
    press(&g, IN_CROSS);
    presses(&g, IN_RIGHT, 6);
    press(&g, IN_CROSS);                            // block at (14, 0)
    presses(&g, IN_R1, 2);
    presses(&g, IN_RIGHT, 8);
    press(&g, IN_CROSS);                            // spike at (22, 0)
    press(&g, IN_R2);
    presses(&g, IN_DOWN, 10);
    press(&g, IN_CROSS);                            // save
    int busy = g.ed.rate.state == RATE_BUSY;
    shot_game(&g, "35a_editor_rating");
    int f = 0;
    while (g.ed.rate.state == RATE_BUSY && f++ < 5000)
        game_step(&g, 0);
    read_text(LEVELS_DIR "/slot01.json", text, (int)sizeof(text));
    level_parse(&lv, text, (int)strlen(text));
    snprintf(what, sizeof(what), "saving rates the level in the background (%s after %d frames) and saves it in the file",
             RATING_NAMES[g.ed.rate.rating], f);
    check(busy && g.ed.rate.state == RATE_DONE && level_rating(&lv) == g.ed.rate.rating && !level_verified(&lv), what);
    shot_game(&g, "35_editor_menu_rating");

    // VERIFY plays it from the start, not in practice; beating it marks it.
    solve(&lv, 2, 0, 100000000L);
    press(&g, IN_DOWN);
    game_step(&g, 0);
    game_step(&g, IN_CROSS);
    int verify = g.state == ST_PLAY && g.verifying && !g.run.practice && g.run.b.x < TILE;
    for (int t = 0; t < ntrace; t++) {
        game_step(&g, trace[t]);
        if (t == 30)
            shot_game(&g, "35d_verify_run");
    }
    for (f = 0; f < 140 && g.state == ST_PLAY; f++)
        game_step(&g, 0);
    shot_game(&g, "35b_verified");
    read_text(LEVELS_DIR "/slot01.json", text, (int)sizeof(text));
    level_parse(&lv, text, (int)strlen(text));
    check(verify && g.state == ST_COMPLETE && g.just_verified && level_verified(&lv) && level_rating(&lv) >= 0,
          "VERIFY plays from the start without practice; beating it saves the level as verified");
    for (f = 0; f < 40; f++)
        game_step(&g, 0);
    press(&g, IN_CIRCLE);
    press(&g, IN_R2);
    press(&g, IN_UP);
    press(&g, IN_CROSS);                            // save and exit
    press(&g, IN_CIRCLE);
    g.title_sel = TITLE_LEVELS;
    open_world(&g);
    g.level_sel = find_level(&g, "file:slot01.json");
    check(g.levels[g.level_sel].verified && g.levels[g.level_sel].rating >= 0, "the level select knows it is verified and rated");
    shot_game(&g, "35c_levels_verified");
    press(&g, IN_CIRCLE);
    press(&g, IN_CIRCLE);

    // Any change undoes both; a practice run does not verify.
    g.title_sel = TITLE_EDITOR;
    press(&g, IN_CROSS);
    press(&g, IN_CROSS);
    presses(&g, IN_RIGHT, 2);
    press(&g, IN_CROSS);                            // a block at (10, 0)
    press(&g, IN_R2);
    presses(&g, IN_DOWN, 10);
    press(&g, IN_CROSS);                            // save
    read_text(LEVELS_DIR "/slot01.json", text, (int)sizeof(text));
    level_parse(&lv, text, (int)strlen(text));
    int undone = !level_verified(&lv) && level_rating(&lv) < 0 && g.ed.rate.state == RATE_BUSY;
    solve(&lv, 2, 0, 100000000L);
    press(&g, IN_DOWN);
    game_step(&g, 0);
    game_step(&g, IN_CROSS);
    game_step(&g, IN_START);
    game_step(&g, 0);
    presses(&g, IN_DOWN, 2);
    press(&g, IN_CROSS);                            // practice on
    for (int t = 0; t < ntrace + 140 && g.state == ST_PLAY; t++)
        game_step(&g, t < ntrace ? trace[t] : 0);
    read_text(LEVELS_DIR "/slot01.json", text, (int)sizeof(text));
    level_parse(&lv, text, (int)strlen(text));
    check(undone && g.state == ST_COMPLETE && g.run.practice && !g.just_verified && !level_verified(&lv),
          "changing the level undoes verified and the rating; a practice finish does not verify");
}

// --- Ghosts ----------------------------------------------------------------------

static void test_ghost(void)
{
    static Game g;
    static Level lv;
    static Ghost a, b;
    static float px[4000], py[4000];
    char buf[256];
    // Run-length files round trip, starting held or released.
    memset(&a, 0, sizeof(a));
    for (int i = 0; i < 3000; i++)
        ghost_push(&a, (i * 7 / 13) % 3 == 0 || i < 4);
    a.hash = 0xC0FFEE;
    a.pct = 42;
    reset_data();
    ghost_save(&a, "file:rt test.json");
    check(ghost_load(&b, "file:rt test.json", 0xC0FFEE) && b.frames == a.frames && b.pct == 42 &&
          !memcmp(a.bits, b.bits, (size_t)(a.frames + 7) / 8), "a ghost file reads back the same presses");
    check(!ghost_load(&b, "file:rt test.json", 0xBAD), "a ghost for a changed level is not used");

    static const char LEVEL[] = "{\"name\":\"GHOST TEST\",\"objects\":[{\"t\":\"spike\",\"x\":14,\"y\":0},"
                                "{\"t\":\"spike\",\"x\":24,\"y\":0},{\"t\":\"spike\",\"x\":34,\"y\":0},"
                                "{\"t\":\"end\",\"x\":50,\"y\":0}]}";
    write_text(LEVELS_DIR "/ghost.json", LEVEL);
    level_parse(&lv, LEVEL, (int)strlen(LEVEL));
    int solved = solve(&lv, 2, 0, 100000000L) == 1;
    game_init(&g, 41);
    game_step(&g, 0);
    open_world(&g);
    g.level_sel = find_level(&g, "file:ghost.json");
    press(&g, IN_CROSS);
    check(g.state == ST_PLAY && g.ghost_t == -1, "no ghost the first time a level is played");
    int f;
    for (f = 0; f < 400 && g.run.state == RUN_PLAY; f++)
        game_step(&g, 0);
    check(g.ghost.pct > 0 && g.ghost.pct < 50 && plat_read_file(GHOSTS_DIR "/file_ghost.json.ghost", buf, 64) > 0,
          "dying saves the run as the ghost to beat");
    while (game_step(&g, 0), g.run.state != RUN_PLAY)
        ;
    check(g.ghost_t == 0, "the next attempt sets the ghost off beside the player");
    int n = 0;
    for (f = 0; solved && f < ntrace && g.state == ST_PLAY && g.run.state == RUN_PLAY; f++) {
        game_step(&g, trace[f]);
        if (n < 4000) {
            px[n] = g.run.b.x;
            py[n++] = g.run.b.y;
        }
    }
    check(g.run.state == RUN_DONE && g.ghost.pct == 100 && g.ghost.frames == n, "finishing saves a 100% ghost");
    for (f = 0; f < 240 && (g.state == ST_PLAY || g.state_frames < 40); f++)
        game_step(&g, 0);
    press(&g, IN_CIRCLE);
    game_step(&g, IN_CROSS);
    int same = g.ghost_t == 0, shot = 0;
    for (f = 0; f < n; f++) {
        game_step(&g, trace[f]);
        if (g.ghost_run.b.x != px[f] || g.ghost_run.b.y != py[f])
            same = 0;
    }
    snprintf(buf, sizeof(buf), "the ghost replays the best run exactly, frame by frame (%d frames)", n);
    check(same && g.ghost_run.state == RUN_DONE, buf);
    for (f = 0; f < 240 && (g.state == ST_PLAY || g.state_frames < 40); f++)
        game_step(&g, 0);
    press(&g, IN_CROSS);
    for (f = 0; f < 200 && g.run.state == RUN_PLAY; f++) {
        game_step(&g, 0);
        if (!shot && g.run.state == RUN_PLAY && g.ghost_run.b.y > g.run.b.y + 25) {
            shot_game(&g, "23_ghost");
            shot = 1;
        }
    }
    check(shot && g.ghost.pct == 100, "the ghost jumps the spike the player runs into (screenshot)");
    check(g.ghost.pct == 100 && g.levels[g.level_sel].best == 100, "a worse attempt does not replace the ghost");
    g.show_ghost = 0;
    game_step(&g, IN_START);
    game_step(&g, 0);
    presses(&g, IN_DOWN, 1);
    press(&g, IN_CROSS);
    check(g.ghost_t == -1, "with GHOST off in Settings no ghost is shown");
    g.show_ghost = 1;
    g.run.practice = 0;
    game_step(&g, IN_START);
    game_step(&g, 0);
    presses(&g, IN_DOWN, 2);
    press(&g, IN_CROSS);
    check(g.run.practice && g.ghost_t == -1, "practice mode has no ghost");
}

// --- Show me how ------------------------------------------------------------------

static void test_hint(void)
{
    static Level lv;
    static Hint h;
    static Game g;
    Run r, hr;
    char what[160];
    // From the middle of CANDY CLIMB the search finds 5 safe seconds within a few frames.
    level_parse(&lv, BUILTIN_JSON[bi("02_candy_climb")], (int)strlen(BUILTIN_JSON[bi("02_candy_climb")]));
    int ok = solve(&lv, 2, 0, 400000000L) == 1;
    run_start(&r, &lv, 1, 0);
    for (int t = 0; ok && t < 1500; t++)
        run_step(&r, trace[t]);
    hint_start(&h, &r);
    int calls = 0, st = HINT_SEARCH;
    long most = 0, before = 0;
    while (st == HINT_SEARCH && calls < 1000) {
        st = hint_step(&h, HINT_STEPS);
        calls++;
        if (h.steps - before > most)
            most = h.steps - before;
        before = h.steps;
    }
    hint_run(&h, &r, &hr);
    int alive = h.state == HINT_READY;
    for (int t = 0; alive && t < h.len; t++)
        alive = run_step(&hr, h.trace[t]) != EV_DIED;
    snprintf(what, sizeof(what), "show me how: a safe path for %d frames found in %d frames of searching (%ld steps, at most %ld a frame)",
             h.len, calls, h.steps, most);
    check(alive && h.len >= HINT_MAX - 6 && most <= HINT_STEPS + 6 && calls < 60, what);

    // Facing a wall too tall to jump there is no way through.
    MK(1, ".........#..", ".........#..", ".........#..........|");
    begin(&r, 1);
    for (int t = 0; t < 40; t++)
        run_step(&r, 0);
    hint_start(&h, &r);
    calls = 1;
    while (hint_step(&h, HINT_STEPS) == HINT_SEARCH && calls < 2000)
        calls++;
    snprintf(what, sizeof(what), "show me how: no hint where nothing gets through (gave up after %d frames)", calls);
    check(h.state == HINT_FAIL && calls <= 100, what);

    // In the game: Touchpad goes back to the checkpoint and the hint plays.
    reset_data();
    write_text(LEVELS_DIR "/hint.json", "{\"name\":\"HINT TEST\",\"objects\":[{\"t\":\"spike\",\"x\":16,\"y\":0},"
                                        "{\"t\":\"spike\",\"x\":17,\"y\":0},{\"t\":\"block\",\"x\":26,\"y\":0},"
                                        "{\"t\":\"spike\",\"x\":30,\"y\":0},{\"t\":\"end\",\"x\":60,\"y\":0}]}");
    game_init(&g, 51);
    game_step(&g, 0);
    open_world(&g);
    g.level_sel = find_level(&g, "file:hint.json");
    press(&g, IN_SQUARE);
    for (int f = 0; f < 30; f++)
        game_step(&g, 0);
    game_step(&g, IN_SQUARE);
    float cpx = g.run.cps[0].x;
    for (int f = 0; f < 20; f++)
        game_step(&g, 0);
    game_step(&g, IN_TOUCH);
    int searched = g.hint.state == HINT_SEARCH && g.run.b.x == cpx;
    shot_game(&g, "24b_hint_thinking");
    int f = 0;
    while (g.hint.state == HINT_SEARCH && f++ < 100)
        game_step(&g, 0);
    check(searched && g.hint.state == HINT_READY && g.hint_t == 0, "Touchpad in practice goes back to the checkpoint and finds a hint");
    {
        // Right on top of the player the hint ghost still shows: a ring around it and dots ahead.
        game_step(&g, 0);
        shot_game(&g, "24c_hint_on_player");
        int hx, hy, ring = 0, ahead = g.hint_npath >= 8;
        run_screen_pos(g.hint_run.b.x, g.hint_run.b.y, &hx, &hy);
        for (int x = hx - 90; x < hx - 40; x++) {
            uint32_t c = fb[hy * SCREEN_W + x] & 0xFFFFFF;
            ring |= c == HINT_RING_A || c == HINT_RING_B;
        }
        for (int i = 1; i < g.hint_npath; i++)
            ahead &= g.hint_path[i][0] > g.hint_path[i - 1][0];
        snprintf(what, sizeof(what), "the hint ghost on top of the player has a bright ring and %d dots ahead (dx %.0f)",
                 g.hint_npath, g.run.b.x - g.hint_run.b.x);
        check(ring && ahead && g.run.b.x - g.hint_run.b.x < 20 && g.hint_run.b.x - g.run.b.x < 20, what);
    }
    for (f = 0; f < 150 && !(g.hint_t > 0 && (g.hint.trace[g.hint_t - 1] & IN_CROSS) && g.hint_run.b.y > 100); f++)
        game_step(&g, 0);
    shot_game(&g, "24_hint");
    while (g.run.state == RUN_PLAY && f++ < 300)
        game_step(&g, 0);
    while (g.run.state != RUN_PLAY && f++ < 400)
        game_step(&g, 0);
    check(g.hint_t >= 0 && g.hint_t < 5 && g.run.b.x == cpx, "after a death at that checkpoint the hint plays again");
    game_step(&g, IN_TRIANGLE);
    game_step(&g, 0);
    check(g.hint.state == HINT_IDLE && g.hint_t == -1, "changing the checkpoints stops the hint");
    g.run.practice = 0;
    game_step(&g, IN_TOUCH);
    check(g.hint.state == HINT_IDLE, "Touchpad does nothing outside practice");
}

// --- 2 player race -----------------------------------------------------------------

static void test_race(void)
{
    static Game g;
    static Level lv;
    char what[160];
    static const char LEVEL[] = "{\"name\":\"RACE TRACK\",\"objects\":[{\"t\":\"spike\",\"x\":16,\"y\":0},"
                                "{\"t\":\"block\",\"x\":30,\"y\":0},{\"t\":\"spike\",\"x\":44,\"y\":0},"
                                "{\"t\":\"spike\",\"x\":45,\"y\":0},{\"t\":\"spike\",\"x\":70,\"y\":0},"
                                "{\"t\":\"end\",\"x\":100,\"y\":0}]}";
    reset_data();
    write_text(LEVELS_DIR "/race.json", LEVEL);
    level_parse(&lv, LEVEL, (int)strlen(LEVEL));
    int solved = solve(&lv, 2, 0, 100000000L) == 1;
    game_init(&g, 61);
    game_step(&g, 0);
    g.title_sel = TITLE_TWO_PLAYER;
    press(&g, IN_CROSS);
    check(g.state == ST_LEVELS && g.two && g.picking, "2 PLAYER opens the world cards for a race");
    shot_game(&g, "25_race_worlds");
    press(&g, IN_CROSS);
    g.level_sel = find_level(&g, "file:race.json");
    press(&g, IN_CROSS);
    check(g.state == ST_LEVELS && g.msg_frames > 0, "a race needs a second controller");
    shot_game(&g, "25a_race_select");
    const int P2 = IN_P2_HERE;
    game_step(&g, P2);
    game_step(&g, P2 | IN_CROSS);
    check(g.state == ST_RACE && g.racing, "X starts the race when controller 2 is on");
    float x1 = g.run.b.x, x2 = g.run2.b.x;
    for (int f = 0; f < RACE_COUNT_TEST - 1; f++) {
        game_step(&g, P2 | IN_P1_CROSS | IN_P2_CROSS);
        if (f == 70)
            shot_game(&g, "25c_race_countdown");
    }
    check(g.run.b.x == x1 && g.run2.b.x == x2, "nobody moves during 3, 2, 1");
    game_step(&g, P2);
    int jumps = g.run.jumps;
    for (int f = 0; f < 10; f++)
        game_step(&g, P2 | IN_CROSS);
    check(g.run.jumps == jumps, "in a race only controller 1's X moves player 1");
    int f, shot = 0, worst = 0, teleported = 0;
    float prev2 = g.run2.b.x;
    for (f = 10; solved && f < ntrace + 200 && g.state == ST_RACE; f++) {
        int in = P2 | (f < ntrace && trace[f] ? IN_P1_CROSS : 0);
        game_step(&g, in);
        if (g.run2.b.x > prev2 + 3 * TILE)
            teleported++;
        prev2 = g.run2.b.x;
        int gap = (int)(g.run.b.x - g.run2.b.x);
        if (g.run2.state == RUN_PLAY && g.winner < 0 && gap > worst)
            worst = gap;
        if (!shot && g.run2.state == RUN_PLAY && g.run.b.x > 40 * TILE && gap > 2 * TILE && gap < 5 * TILE) {
            shot_game(&g, "25_race");
            shot = 1;
        }
    }
    snprintf(what, sizeof(what), "player 2 falls behind, is brought back on screen %d times (worst gap %d px)", teleported, worst);
    check(g.run2.attempt > 1 && teleported > 0 && worst <= PLAYER_X + PLAYER_S / 2 + 21, what);
    for (f = 0; f < 120 && g.state == ST_RACE; f++)
        game_step(&g, P2);
    check(g.state == ST_RACE_DONE && g.winner == 1 && (g.badges >> B_RACER & 1), "player 1 reaches the end first and wins");
    for (f = 0; f < 40; f++)
        game_step(&g, P2);
    shot_game(&g, "25b_race_done");
    shot = shot && 1;
    game_step(&g, P2);
    game_step(&g, P2 | IN_CROSS);
    check(g.state == ST_RACE && g.race_t == 0, "X races again");
    for (f = 0; f < RACE_COUNT_TEST; f++)
        game_step(&g, P2);
    for (f = 0; solved && f < ntrace + 200 && g.state == ST_RACE; f++)
        game_step(&g, P2 | (f < ntrace && trace[f] ? IN_P1_CROSS | IN_P2_CROSS : 0));
    check(g.state == ST_RACE_DONE && g.winner == 0, "the same run by both players is a tie");
    for (f = 0; f < 40; f++)
        game_step(&g, P2);
    game_step(&g, P2 | IN_CIRCLE);
    check(g.state == ST_LEVELS && g.two, "Circle goes back to the race level select");
    game_step(&g, P2);
    game_step(&g, P2 | IN_CROSS);
    game_step(&g, P2 | IN_START);
    check(g.state == ST_PAUSE && g.racing, "Options pauses a race");
    game_step(&g, P2);
    game_step(&g, P2 | IN_CIRCLE);
    check(g.state == ST_RACE, "Circle resumes it");
    check(shot, "screenshot of both players racing");

    // Race checkpoints: only on the ground, or well inside a flying corridor.
    static Level rr;
    int k = bi("04_rocket_run");
    level_parse(&rr, BUILTIN_JSON[k], (int)strlen(BUILTIN_JSON[k]));
    int ground = 0, flying = 0, bad = 0;
    if (solve(&rr, 2, 0, 400000000L) == 1) {
        Run r;
        run_start(&r, &rr, 1, 1);
        r.race = 1;
        for (int t = 0; t < ntrace && r.state == RUN_PLAY; t++) {
            int had = r.ncp;
            run_step(&r, trace[t]);
            if (r.ncp == had || r.ncp == 0)
                continue;
            const Body *b = &r.cps[r.ncp - 1];
            float hh = run_half_h(b);
            if (b->mode == MODE_CUBE || b->mode == MODE_BALL)
                ground++, bad += !b->on_ground;
            else
                flying++, bad += b->y - hh < b->lo + TILE / 2 || b->y + hh > b->hi - TILE / 2;
        }
    }
    snprintf(what, sizeof(what), "race checkpoints: %d standing, %d flying inside the corridor, %d mid-air", ground,
             flying, bad);
    check(ground > 20 && flying > 5 && bad == 0, what);
}

// --- Death markers ---------------------------------------------------------------

static void test_deaths(void)
{
    static Game g;
    char buf[8192];
    reset_data();
    write_text(LEVELS_DIR "/deaths.json", "{\"name\":\"DEATH TEST\",\"objects\":[{\"t\":\"spike\",\"x\":14,\"y\":0},"
                                          "{\"t\":\"spike\",\"x\":30,\"y\":0},{\"t\":\"end\",\"x\":60,\"y\":0}]}");
    game_init(&g, 71);
    game_step(&g, 0);
    open_world(&g);
    g.level_sel = find_level(&g, "file:deaths.json");
    press(&g, IN_CROSS);
    int f;
    for (int d = 0; d < 3; d++) {
        for (f = 0; f < 400 && g.run.state == RUN_PLAY; f++)
            game_step(&g, 0);
        for (f = 0; f < 80 && g.run.state != RUN_PLAY; f++)
            game_step(&g, 0);
    }
    read_text(GHOSTS_DIR "/file_deaths.json.deaths", buf, sizeof(buf));
    check(g.ndeaths == 3 && g.deaths[0][0] > 12 * TILE && g.deaths[0][0] < 15 * TILE && strchr(buf, '\n') != NULL,
          "each death is remembered and saved for the level");
    game_step(&g, IN_START);
    game_step(&g, 0);
    presses(&g, IN_DOWN, 3);
    press(&g, IN_CROSS);                                 // exit
    game_init(&g, 72);
    game_step(&g, 0);
    open_world(&g);
    g.level_sel = find_level(&g, "file:deaths.json");
    press(&g, IN_SQUARE);
    check(g.ndeaths == 3 && g.run.practice, "the death list is read back for the next try");
    for (f = 0; f < 50; f++)
        game_step(&g, 0);
    game_step(&g, IN_SQUARE);
    for (f = 0; f < 300 && g.run.state == RUN_PLAY; f++)
        game_step(&g, 0);
    for (f = 0; f < 60 && g.run.state != RUN_PLAY; f++)
        game_step(&g, 0);
    for (f = 0; f < 15; f++)
        game_step(&g, 0);
    shot_game(&g, "26_death_marks");
    for (int d = 0; d < 210; d++) {
        for (f = 0; f < 400 && g.run.state == RUN_PLAY; f++)
            game_step(&g, 0);
        for (f = 0; f < 80 && g.run.state != RUN_PLAY; f++)
            game_step(&g, 0);
    }
    check(g.ndeaths == MAX_DEATHS && deaths_load(g.deaths, "file:deaths.json") == MAX_DEATHS, "only the last 200 deaths are kept");
}

// --- Badges ----------------------------------------------------------------------

static void test_badges(void)
{
    static Game g;
    char buf[4096];
    reset_data();
    write_text(STATS_FILE, "attempts 99\nseconds 3599\n");
    game_init(&g, 31);
    game_step(&g, 0);
    check(g.badges == 0 && g.nbadge_toasts == 0, "no badges on a fresh start");
    g.stats[STAT_ATTEMPTS] = 100;
    for (int f = 0; f < 31; f++)
        game_step(&g, 0);
    check((g.badges >> B_ATTEMPTS100 & 1) && !(g.badges >> B_HOUR & 1), "100 attempts earns NEVER GIVE UP");
    int toasted = g.nbadge_toasts;
    for (int f = 0; f < BADGE_TOAST_FRAMES + 2; f++)
        game_step(&g, 0);
    check(toasted == 1 && g.nbadge_toasts == 0, "the toast shows for a while, then goes");
    read_text(BADGES_FILE, buf, sizeof(buf));
    check(strstr(buf, "attempts100 1") != NULL, "badges are saved in badges.txt");

    // A badge unlocks a Customize option.
    g.title_sel = TITLE_CUSTOMIZE;
    press(&g, IN_CROSS);
    presses(&g, IN_DOWN, 2);
    presses(&g, IN_LEFT, LOOK_COUNT[LOOK_FACE] - FACE_ICON0 + 1);    // face: WINK, the last before MY ICON 1
    check(g.browse.v[LOOK_FACE] == FACE_ICON0 - 1 && g.look.v[LOOK_FACE] != g.browse.v[LOOK_FACE] &&
          !strcmp(g.msg, "EARN THE FIRST WIN BADGE"), "WINK needs the FIRST WIN badge");
    shot_game(&g, "14d_customize_badge_lock");
    presses(&g, IN_DOWN, 3);
    presses(&g, IN_LEFT, LOOK_COUNT[LOOK_DEATH] - 5);  // death: CONFETTI (5), unlocked by NEVER GIVE UP
    check(!strcmp(LOOK_OPTIONS[LOOK_DEATH][g.look.v[LOOK_DEATH]].name, "CONFETTI") && (g.badges >> B_FASHION & 1),
          "NEVER GIVE UP unlocks CONFETTI, and wearing it earns FASHION STAR");
    for (int f = 0; f < 10; f++)
        game_step(&g, 0);
    shot_game(&g, "14e_customize_confetti");
    press(&g, IN_CIRCLE);

    // Recording mode holds new badges back until the level is left.
    g.clean_hud = 1;
    for (int f = 0; f < 2 * BADGE_TOAST_FRAMES; f++)
        game_step(&g, 0);
    g.title_sel = TITLE_LEVELS;
    open_world(&g);
    press(&g, IN_CROSS);
    g.stats[STAT_SECONDS] = 3600;
    for (int f = 0; f < 40; f++)
        game_step(&g, 0);
    check(g.state == ST_PLAY && (g.badges >> B_HOUR & 1) && g.nbadge_toasts == 1 && g.badge_toast_frames == 0,
          "recording mode keeps the badge toast out of the level");
    g.clean_hud = 0;

    // The badge screen.
    game_init(&g, 32);
    game_step(&g, 0);
    g.title_sel = TITLE_BADGES;
    press(&g, IN_CROSS);
    check(g.state == ST_BADGES, "BADGES opens the badge screen");
    presses(&g, IN_RIGHT, 6);
    press(&g, IN_DOWN);
    check(g.badge_sel == 16, "the D-pad moves around the badges (10 to a row)");
    press(&g, IN_UP);
    press(&g, IN_LEFT);
    shot_game(&g, "22_badges");
    press(&g, IN_CIRCLE);
    check(g.state == ST_TITLE, "Circle leaves the badge screen");
    shot_game(&g, "01d_title_badges");
}

// --- 3.1 rewards: new Customize options and badges ------------------------------

// Every Customize option of 3.0, in order, with its cost: old saves keep their
// looks and nothing he had gets dearer.
static const struct { int part; const char *name; int stars, coins, badge; } LOOKS_30[] = {
    { LOOK_COL1, "GREEN", 0, 0, 0 }, { LOOK_COL1, "CYAN", 0, 0, 0 }, { LOOK_COL1, "PINK", 0, 0, 0 },
    { LOOK_COL1, "YELLOW", 0, 0, 0 }, { LOOK_COL1, "ORANGE", 1, 0, 0 }, { LOOK_COL1, "RED", 2, 0, 0 },
    { LOOK_COL1, "BLUE", 3, 0, 0 }, { LOOK_COL1, "PURPLE", 5, 0, 0 }, { LOOK_COL1, "WHITE", 8, 0, 0 },
    { LOOK_COL1, "BLACK", 12, 0, 0 }, { LOOK_COL1, "GOLD", 0, 3, 0 }, { LOOK_COL1, "MINT", 0, 0, B_BUILDER + 1 },
    { LOOK_FACE, "CLASSIC", 0, 0, 0 }, { LOOK_FACE, "SMILE", 0, 0, 0 }, { LOOK_FACE, "COOL", 1, 0, 0 },
    { LOOK_FACE, "ANGRY", 3, 0, 0 }, { LOOK_FACE, "ROBOT", 5, 0, 0 }, { LOOK_FACE, "CAT", 8, 0, 0 },
    { LOOK_FACE, "CYCLOPS", 11, 0, 0 }, { LOOK_FACE, "STAR", 15, 0, 0 }, { LOOK_FACE, "ALIEN", 0, 6, 0 },
    { LOOK_FACE, "WINK", 0, 0, B_FIRST_WIN + 1 }, { LOOK_FACE, "MY ICON 1", 0, 0, 0 }, { LOOK_FACE, "MY ICON 2", 0, 0, 0 },
    { LOOK_FACE, "MY ICON 3", 0, 0, 0 }, { LOOK_FACE, "MY ICON 4", 0, 0, 0 }, { LOOK_FACE, "MY ICON 5", 0, 0, 0 },
    { LOOK_FACE, "MY ICON 6", 0, 0, 0 },
    { LOOK_SHIP, "ROCKET", 0, 0, 0 }, { LOOK_SHIP, "JET", 3, 0, 0 }, { LOOK_SHIP, "SAUCER", 6, 0, 0 },
    { LOOK_SHIP, "SHARK", 10, 0, 0 },
    { LOOK_TRAIL, "SQUARES", 0, 0, 0 }, { LOOK_TRAIL, "NONE", 0, 0, 0 }, { LOOK_TRAIL, "LINE", 2, 0, 0 },
    { LOOK_TRAIL, "SPARKS", 5, 0, 0 }, { LOOK_TRAIL, "RAINBOW", 9, 0, 0 }, { LOOK_TRAIL, "STARS", 0, 9, 0 },
    { LOOK_TRAIL, "BUBBLES", 0, 0, B_ORBS + 1 },
    { LOOK_DEATH, "BURST", 0, 0, 0 }, { LOOK_DEATH, "RING", 2, 0, 0 }, { LOOK_DEATH, "SHATTER", 4, 0, 0 },
    { LOOK_DEATH, "FIREWORKS", 7, 0, 0 }, { LOOK_DEATH, "COIN SHOWER", 0, 12, 0 },
    { LOOK_DEATH, "CONFETTI", 0, 0, B_ATTEMPTS100 + 1 },
};

// Every built-in level beaten with all its coins: progress.txt and coins.txt.
static void beat_builtins(int skip)
{
    static char prog[MAX_LEVELS * 100], coins[MAX_LEVELS * 100];
    static Level lv;
    int np = 0, nc = 0;
    for (int i = 0; i < BUILTIN_COUNT; i++) {
        if (i == skip)
            continue;
        level_parse(&lv, BUILTIN_JSON[i], (int)strlen(BUILTIN_JSON[i]));
        np += snprintf(prog + np, sizeof(prog) - (size_t)np, "1 100 0 builtin:%s\n", BUILTIN_IDS[i]);
        nc += snprintf(coins + nc, sizeof(coins) - (size_t)nc, "%d builtin:%s\n", (1 << lv.ncoins) - 1, BUILTIN_IDS[i]);
    }
    write_text(PROGRESS_FILE, prog);
    write_text(COINS_FILE, coins);
}

static void test_rewards(void)
{
    static Game g;
    char what[200];
    int same = 1, at[LOOK_PARTS] = { 0 };
    for (int i = 0; i < (int)(sizeof(LOOKS_30) / sizeof(LOOKS_30[0])); i++) {
        int part = LOOKS_30[i].part;
        const LookOption *o = &LOOK_OPTIONS[part][at[part]++];
        same &= !strcmp(o->name, LOOKS_30[i].name) && o->stars == LOOKS_30[i].stars && o->coins == LOOKS_30[i].coins &&
                o->badge == LOOKS_30[i].badge;
    }
    at[LOOK_COL2] = at[LOOK_COL1];
    check(same, "every 3.0 Customize option keeps its place and its cost");
    int by_stars = 0, by_coins = 0, lo = 1000, hi = 0;
    for (int part = LOOK_COL2; part < LOOK_PARTS; part++)
        for (int k = at[part]; k < LOOK_COUNT[part]; k++) {
            const LookOption *o = &LOOK_OPTIONS[part][k];
            printf("     new: %s %s, %d stars, %d coins\n", part == LOOK_COL2 ? "COLOUR" : part == LOOK_FACE ? "FACE" :
                   part == LOOK_SHIP ? "SHIP" : part == LOOK_TRAIL ? "TRAIL" : "DEATH", o->name, o->stars, o->coins);
            by_stars += o->stars > 0;
            by_coins += o->coins > 0;
            lo = o->stars && o->stars < lo ? o->stars : lo;
            hi = o->stars > hi ? o->stars : hi;
        }
    snprintf(what, sizeof(what), "3.1 adds %d unlocks by stars (%d to %d) and %d by coins", by_stars, lo, hi, by_coins);
    check(by_stars >= 6 && by_coins >= 2 && lo >= 30 && lo <= 50 && hi >= 230 && hi <= 255, what);

    // All built-ins beaten with every coin: the stars and the new badges.
    reset_data();
    beat_builtins(-1);
    game_init(&g, 401);
    for (int f = 0; f < 31; f++)
        game_step(&g, 0);
    int stars = game_stars(&g), has59 = 0, ok = 1;
    for (int i = 0; i < BUILTIN_COUNT; i++)
        has59 |= !strncmp(BUILTIN_IDS[i], "59", 2);
    for (int w = 1; w <= B_COSMIC - B_ISLES + 1; w++) {
        LWorldStats ws;
        game_lworld_stats(&g, w, &ws);
        ok &= ((g.badges >> (B_ISLES + w - 1)) & 1) == (ws.count > 0);
    }
    snprintf(what, sizeof(what), "with every level beaten (%d stars): a badge per world that has levels, TREASURE HUNTER, "
             "SUPER STAR at 100, MEGA STAR at 200, CORE BREAKER for level 59 (%s)", stars, has59 ? "here" : "not built");
    check(ok && (g.badges >> B_WORLD_COINS & 1) && ((g.badges >> B_STARS100) & 1) == (stars >= 100) &&
          ((g.badges >> B_STARS200) & 1) == (stars >= 200) && ((g.badges >> B_CORE) & 1) == has59, what);
    // CORAL (40 stars) and TEAL (45 coins): worn when there are enough.
    g.title_sel = TITLE_CUSTOMIZE;
    press(&g, IN_CROSS);
    presses(&g, IN_LEFT, 2);
    int coral = !strcmp(LOOK_OPTIONS[LOOK_COL1][g.browse.v[LOOK_COL1]].name, "CORAL");
    snprintf(what, sizeof(what), "CORAL needs 40 stars: %s with %d", stars >= 40 ? "worn" : "not worn", stars);
    check(coral && (g.look.v[LOOK_COL1] == g.browse.v[LOOK_COL1]) == (stars >= 40) &&
          (stars >= 40 || strstr(g.msg, "NEEDS 40 STARS") != NULL), what);
    g.browse.v[LOOK_FACE] = LOOK_COUNT[LOOK_FACE] - 1;
    g.browse.v[LOOK_SHIP] = LOOK_COUNT[LOOK_SHIP] - 1;
    g.browse.v[LOOK_TRAIL] = LOOK_COUNT[LOOK_TRAIL] - 1;
    g.custom_row = LOOK_TRAIL;
    for (int f = 0; f < 23; f++)
        game_step(&g, 0);
    shot_game(&g, "14f_customize_new_looks");
    press(&g, IN_CIRCLE);
    g.badges = (uint32_t)((1ull << BADGE_COUNT) - 1);
    g.title_sel = TITLE_BADGES;
    press(&g, IN_CROSS);
    g.badge_sel = B_CORE;
    shot_game(&g, "22b_badges_all");
    press(&g, IN_CIRCLE);

    // One level of a world missing: no badge for that world.
    for (int w = 1; w <= B_COSMIC - B_ISLES + 1; w++) {
        int last = -1;
        for (int i = 0; i < BUILTIN_COUNT; i++)
            last = BUILTIN_IDS[i][0] == '0' + w ? i : last;
        if (last < 0)
            continue;
        reset_data();
        beat_builtins(last);
        game_init(&g, 402);
        for (int f = 0; f < 31; f++)
            game_step(&g, 0);
        snprintf(what, sizeof(what), "%s not beaten: no %s badge", BUILTIN_IDS[last], BADGES[B_ISLES + w - 1].name);
        check(!(g.badges >> (B_ISLES + w - 1) & 1) && ((g.badges >> B_CORE) & 1) == (has59 && w != 5), what);
    }

    // The new looks, one per box: colours, faces, the ship, trails and death effects.
    static Particle parts[MAX_PARTS];
    uint32_t rng = 77;
    draw_set_target(fb);
    draw_set_offset(0, 0);
    run_draw_backdrop(LEVEL_PALETTES[3], 0, 0);
    static const struct { int part, v; } SHOW[] = {
        { LOOK_COL1, 12 }, { LOOK_COL1, 13 }, { LOOK_FACE, 16 }, { LOOK_FACE, 17 }, { LOOK_SHIP, 4 },
        { LOOK_TRAIL, 7 }, { LOOK_TRAIL, 8 }, { LOOK_DEATH, 6 }, { LOOK_DEATH, 7 },
    };
    for (int i = 0; i < (int)(sizeof(SHOW) / sizeof(SHOW[0])); i++) {
        int x = 30 + (i % 5) * 376, y = 60 + (i / 5) * 500;
        Look l = { { 0, 1, 1, 0, 0, 0 }, 0 };
        l.v[SHOW[i].part] = SHOW[i].v;
        if (SHOW[i].part == LOOK_COL1)
            l.v[LOOK_COL2] = 8;
        ui_panel(x, y, 350, 440, LOOK_COLORS[l.v[LOOK_COL1]]);
        const char *name = LOOK_OPTIONS[SHOW[i].part][SHOW[i].v].name;
        draw_text_glow(x + (350 - text_width(name, ui_fit_scale(name, 320, 5))) / 2, y + 20, ui_fit_scale(name, 320, 5), name,
                       RGB(255, 255, 255), LOOK_COLORS[l.v[LOOK_COL1]]);
        look_set(&l);
        if (SHOW[i].part == LOOK_DEATH) {
            fx_burst(parts, &l, (float)(x + 175), (float)(GROUND_Y - (y + 220)), &rng);
            for (int f = 0; f < 9; f++)
                fx_update(parts);
            fx_draw(parts, 0, 0, RGB(10, 6, 26));
        } else {
            run_draw_cube((float)(x + 90), (float)(y + 150), 0);
            look_draw_preview(x + 250, y + 330, y + 400, 8);
        }
    }
    save_ppm("40_new_looks");
}

static void render(int16_t *pcm, int frames);
static double mean_square(const int16_t *pcm, int frames, int *peak);

// Example songs: read-only, they play, and X copies one into his first empty slot.
static void test_song_examples(void)
{
    static int16_t pcm[48000 * 2];
    int peak;
    static Game g;
    static char a[10][2048], b[10][2048], text[8192];
    char what[200], path[160], name[SONG_NAME_MAX + 1];
    UserSong want;
    reset_data();
    write_text(SONGS_DIR "/song01.json", "{\"format\":\"fritzdash-song\",\"name\":\"MINE\",\"bpm\":90,\"lead\":\"1.2.3.4.\"}");
    write_text(SONGS_DIR "/song02.json", "{\"format\":\"fritzdash-song\",\"name\":\"MINE TOO\",\"bpm\":95,\"bass\":\"1...\"}");
    game_init(&g, 121);
    game_step(&g, 0);
    g.title_sel = TITLE_SONGS;
    press(&g, IN_CROSS);
    presses(&g, IN_DOWN, 2);
    check(g.sm.screen == SM_SLOTS && g.sm.slot_sel == USER_SONGS, "down twice on the song slots reaches the examples");
    shot_game(&g, "33d_song_examples");
    // Each one opens, plays at its tempo, and has the instruments its name says.
    static const int TRACKS[NUM_SONG_EXAMPLES] = { 1 << TRACK_DRUMS, 1 << TRACK_LEAD, 1 << TRACK_BASS | 1 << TRACK_DRUMS, 15 };
    int ok = 1;
    for (int e = 0; e < NUM_SONG_EXAMPLES; e++) {
        press(&g, IN_CROSS);
        songs_example(e, &want, name, sizeof(name));
        int used = 0;
        for (int t = 0; t < TRACK_COUNT; t++)
            for (int k = 0; k < USER_STEPS; k++)
                used |= g.sm.song.notes[t][k] ? 1 << t : 0;
        printf("     example %d %s: %d BPM, tracks %x, \"%s\"\n", e + 1, g.sm.name, g.sm.song.bpm, used, songs_example_caption(e));
        render(pcm, 48000);
        double ms = mean_square(pcm, 48000, &peak);
        ok &= g.sm.example == e + 1 && g.sm.playing && !memcmp(&g.sm.song, &want, sizeof(want)) && used == TRACKS[e] &&
              synth_bpm(SONG_USER + SONG_PREVIEW) == want.bpm && ms > 300.0 * 300.0 && peak < 32767;
        if (e == NUM_SONG_EXAMPLES - 1) {
            for (int f = 0; f < 50; f++)
                game_step(&g, 0);
            shot_game(&g, "33e_song_example_full");
        } else {
            press(&g, IN_CIRCLE);
            press(&g, IN_RIGHT);
        }
    }
    check(ok, "the four examples open and are heard: drums only, a melody, bass and drums, all four");
    UserSong was = g.sm.song;
    press(&g, IN_SQUARE);
    press(&g, IN_R2);
    press(&g, IN_TOUCH);
    press(&g, IN_CIRCLE);
    int locked = !memcmp(&was, &g.sm.song, sizeof(was)) && g.sm.screen == SM_SLOTS && !songs_used(2);
    press(&g, IN_CROSS);
    for (int i = 0; i < 2; i++) {
        snprintf(path, sizeof(path), SONGS_DIR "/song%02d.json", i + 1);
        read_text(path, a[i], sizeof(a[i]));
    }
    press(&g, IN_CROSS);
    int n = read_text(SONGS_DIR "/song03.json", text, sizeof(text));
    UserSong got;
    int parsed = n > 0 && song_parse(&got, name, sizeof(name), text, n) && !memcmp(&got, &want, sizeof(want));
    for (int i = 0; i < 2; i++) {
        snprintf(path, sizeof(path), SONGS_DIR "/song%02d.json", i + 1);
        read_text(path, b[i], sizeof(b[i]));
        parsed &= !strcmp(a[i], b[i]);
    }
    snprintf(what, sizeof(what), "an example cannot be changed; X copies it into song 3, the first empty slot (\"%s\"), "
             "opens it there, and songs 1 and 2 stay", name);
    check(locked && parsed && g.sm.example == 0 && g.sm.slot == 2 && g.sm.screen == SM_EDIT && songs_used(2), what);
    shot_game(&g, "33f_song_example_copied");
    press(&g, IN_CROSS);
    check(g.sm.dirty && memcmp(&g.sm.song, &want, sizeof(want)) != 0, "the copy can be changed like his own songs");

    // All ten slots used: nothing is written over.
    for (int i = 0; i < USER_SONGS; i++) {
        snprintf(path, sizeof(path), SONGS_DIR "/song%02d.json", i + 1);
        snprintf(a[i], sizeof(a[i]), "{\"format\":\"fritzdash-song\",\"name\":\"SONG %d\",\"bpm\":%d}", i + 1, 80 + i);
        write_text(path, a[i]);
    }
    game_init(&g, 122);
    game_step(&g, 0);
    g.title_sel = TITLE_SONGS;
    press(&g, IN_CROSS);
    presses(&g, IN_UP, 1);
    press(&g, IN_CROSS);
    press(&g, IN_CROSS);
    int same = 1;
    for (int i = 0; i < USER_SONGS; i++) {
        snprintf(path, sizeof(path), SONGS_DIR "/song%02d.json", i + 1);
        read_text(path, b[i], sizeof(b[i]));
        same &= !strcmp(a[i], b[i]);
    }
    check(same && g.sm.example == 1 && strstr(g.sm.msg, "ALL 10") != NULL,
          "with all ten slots used, X on an example writes nothing and says so");
    shot_game(&g, "33g_song_example_full");
    press(&g, IN_CIRCLE);
    press(&g, IN_CIRCLE);
}

// --- Synth ----------------------------------------------------------------------

static void write_wav(const char *name, const int16_t *pcm, int frames)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.wav", outdir, name);
    FILE *f = fopen(path, "wb");
    if (!f)
        return;
    uint32_t data = (uint32_t)frames * 4, v32;
    uint16_t v16;
    fwrite("RIFF", 1, 4, f); v32 = 36 + data; fwrite(&v32, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v32 = 16; fwrite(&v32, 4, 1, f); v16 = 1; fwrite(&v16, 2, 1, f); v16 = 2; fwrite(&v16, 2, 1, f);
    v32 = 48000; fwrite(&v32, 4, 1, f); v32 = 48000 * 4; fwrite(&v32, 4, 1, f); v16 = 4; fwrite(&v16, 2, 1, f);
    v16 = 16; fwrite(&v16, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    fwrite(pcm, 4, (size_t)frames, f);
    fclose(f);
}

static void render(int16_t *pcm, int frames)
{
    for (int off = 0; off < frames; off += 256)
        synth_render(pcm + off * 2, frames - off < 256 ? frames - off : 256);
}

static double mean_square(const int16_t *pcm, int frames, int *peak)
{
    double sum = 0;
    *peak = 0;
    for (int i = 0; i < frames * 2; i++) {
        int v = pcm[i] < 0 ? -pcm[i] : pcm[i];
        sum += (double)pcm[i] * pcm[i];
        if (v > *peak)
            *peak = v;
    }
    return sum / (frames * 2);
}

static void test_synth(void)
{
    static int16_t pcm[48000 * 2 * 16];
    char what[160], name[64];
    int peak;
    synth_init(48000);
    synth_volume(7, 8);
    for (int s = 0; s <= NUM_SONGS; s++) {
        synth_song(s);
        render(pcm, 48000 * 16);
        double ms = mean_square(pcm, 48000 * 16, &peak);
        snprintf(what, sizeof(what), "song %d (%s) plays: mean square %.0f, peak %d", s,
                 s < NUM_SONGS ? SONG_NAMES[s] : "MENU", ms, peak);
        check(ms > 900.0 * 900.0 && peak < 32767, what);
        snprintf(name, sizeof(name), "song_%d", s);
        write_wav(name, pcm, 48000 * 16);
    }
    int tempos = 1;
    for (int a = 0; a < NUM_SONGS; a++)
        for (int b = a + 1; b < NUM_SONGS; b++)
            tempos &= synth_bpm(a) != synth_bpm(b) && strcmp(SONG_NAMES[a], SONG_NAMES[b]);
    check(NUM_SONGS == 15 && tempos, "15 level songs (10..14 are the world themes), each with its own name and tempo");
    synth_song(SONG_THEME_SHAPESHIFT);
    render(pcm, 48000 * 16);
    double prism = mean_square(pcm, 48000 * 16, &peak);
    for (int a = 0; a <= NUM_SONGS; a++)
        tempos &= synth_bpm(a) != synth_bpm(SONG_THEME_SHAPESHIFT);
    for (int a = 0; a < NUM_SECRET_SONGS; a++)
        tempos &= synth_bpm(SONG_SECRET + a) != synth_bpm(SONG_THEME_SHAPESHIFT);
    snprintf(what, sizeof(what), "SHAPESHIFT's theme PRISM SHIFT (song %d, after every older id) plays with a tempo of "
             "its own: mean square %.0f, peak %d", SONG_THEME_SHAPESHIFT, prism, peak);
    check(SONG_THEME_SHAPESHIFT == SONG_SECRET + NUM_SECRET_SONGS && prism > 900.0 * 900.0 && peak < 32767 && tempos, what);
    write_wav("song_prism_shift", pcm, 48000 * 16);
    synth_song(-1);
    render(pcm, 24000);
    mean_square(pcm, 24000, &peak);
    check(peak == 0, "no song and no sound is silence");
    int all = 1;
    for (int k = 0; k < SND_COUNT; k++) {
        synth_sfx(k);
        render(pcm, 24000);
        all &= mean_square(pcm, 24000, &peak) > 100.0 * 100.0;
    }
    check(all, "every sound effect makes a sound");
    for (int k = 0; k < SND_COUNT; k++) {
        synth_sfx(k);
        render(pcm + k * 12000 * 2, 12000);
    }
    write_wav("sound_effects", pcm, 12000 * SND_COUNT);
    synth_volume(0, 0);
    synth_song(0);
    render(pcm, 24000);
    mean_square(pcm, 24000, &peak);
    check(peak == 0, "volume 0 is silent");
    synth_volume(7, 8);
}

// --- Song maker --------------------------------------------------------------------

static void test_songs(void)
{
    static Game g;
    static Level lv;
    static UserSong back;
    static int16_t pcm[48000 * 2 * 4];
    static char text[8192];
    char name[SONG_NAME_MAX + 1], what[160];
    int peak;
    reset_data();
    game_init(&g, 91);
    game_step(&g, 0);
    g.title_sel = TITLE_SONGS;
    press(&g, IN_CROSS);
    check(g.state == ST_SONGS && g.sm.screen == SM_SLOTS, "SONG MAKER opens the ten song slots");
    press(&g, IN_CROSS);
    const UserSong *u = &g.sm.song;
    check(g.sm.screen == SM_EDIT && u->bpm == 120 && u->notes[TRACK_DRUMS][0] == DRUM_KICK &&
          u->notes[TRACK_DRUMS][2] == DRUM_SNARE && !u->notes[TRACK_LEAD][0] && g.sm.row == 3,
          "a new song starts with a drum beat and an empty tune");
    press(&g, IN_CROSS);                            // lead note 5 on step 0
    press(&g, IN_RIGHT);
    press(&g, IN_UP);
    press(&g, IN_CROSS);                            // note 6 on step 1
    presses(&g, IN_RIGHT, 2);
    presses(&g, IN_DOWN, 3);
    press(&g, IN_CROSS);                            // note 3 on step 3
    press(&g, IN_CROSS);                            // and off again
    int toggled = u->notes[TRACK_LEAD][3] == 0;
    press(&g, IN_CROSS);
    press(&g, IN_SQUARE);
    int cleared = u->notes[TRACK_LEAD][3] == 0;
    press(&g, IN_CIRCLE);
    check(u->notes[TRACK_LEAD][0] == 5 && u->notes[TRACK_LEAD][1] == 6 && u->notes[TRACK_LEAD][3] == 3 && toggled &&
          cleared, "X puts a note in the row it is on (X again takes it off), Square clears, Circle undoes");
    press(&g, IN_R1);
    press(&g, IN_CROSS);                            // sparkle note 3 on step 3
    press(&g, IN_R1);
    presses(&g, IN_LEFT, 3);
    press(&g, IN_CROSS);                            // bass note 3 on step 0
    press(&g, IN_R1);
    int drums = g.sm.track == TRACK_DRUMS && g.sm.row == 2;
    press(&g, IN_CROSS);                            // kick off on step 0
    drums &= u->notes[TRACK_DRUMS][0] == 0;
    press(&g, IN_CROSS);
    press(&g, IN_RIGHT);
    press(&g, IN_UP);
    press(&g, IN_CROSS);                            // snare with the hat on step 1
    check(drums && u->notes[TRACK_SPARKLE][3] == 3 && u->notes[TRACK_BASS][0] == 3 &&
          u->notes[TRACK_DRUMS][0] == DRUM_KICK && u->notes[TRACK_DRUMS][1] == (DRUM_SNARE | DRUM_HAT),
          "L1 and R1 pick the sparkle, bass and drums; drum rows are hat, snare and kick");
    press(&g, IN_START);
    for (int f = 0; f < 40; f++)
        game_step(&g, 0);
    check(g.sm.playing && song_play_step(u, g.sm.play_t) == 2, "Options plays the loop (two steps a beat)");
    shot_game(&g, "33b_song_maker_drums");
    presses(&g, IN_L1, 3);
    for (int f = 0; f < 30; f++)
        game_step(&g, 0);
    shot_game(&g, "33a_song_maker");
    press(&g, IN_START);
    check(!g.sm.playing, "Options again stops it");

    press(&g, IN_R2);
    presses(&g, IN_DOWN, 2);
    press(&g, IN_RIGHT);                            // speed 130
    press(&g, IN_DOWN);
    press(&g, IN_RIGHT);                            // spooky
    press(&g, IN_DOWN);
    press(&g, IN_RIGHT);                            // round lead
    shot_game(&g, "33c_song_menu");
    press(&g, IN_DOWN);
    press(&g, IN_CROSS);                            // name
    presses(&g, IN_SQUARE, 12);
    press(&g, IN_RIGHT); press(&g, IN_CROSS);                          // B
    presses(&g, IN_RIGHT, 3); press(&g, IN_CROSS); press(&g, IN_CROSS); // E E
    press(&g, IN_DOWN); press(&g, IN_RIGHT); press(&g, IN_CROSS);      // P
    press(&g, IN_START);
    check(g.sm.screen == SM_MENU && !strcmp(g.sm.name, "BEEP") && u->bpm == 130 && u->scale == SCALE_SPOOKY &&
          u->lead_wave == 2, "the menu sets speed, notes, lead sound and name");
    presses(&g, IN_DOWN, 3);
    press(&g, IN_CROSS);                            // save and exit
    int n = plat_read_file(SONGS_DIR "/song01.json", text, (int)sizeof(text) - 1);
    int ok = n > 0 && song_parse(&back, name, sizeof(name), text, n);
    check(g.sm.screen == SM_SLOTS && ok && !memcmp(&back, u, sizeof(back)) && !strcmp(name, "BEEP"),
          "SAVE AND EXIT writes songs/song01.json and it reads back the same");
    shot_game(&g, "33_song_slots");
    press(&g, IN_CIRCLE);
    check(g.state == ST_TITLE, "Circle on the song slots goes back to the title");

    game_init(&g, 92);
    check(songs_used(0) && !strcmp(songs_name(0), "BEEP") && !songs_used(1) && songs_count() == 1,
          "saved songs are found on the next start");
    synth_init(48000);
    synth_volume(7, 8);
    songs_load_all();
    synth_song(SONG_USER);
    render(pcm, 48000 * 4);
    double ms = mean_square(pcm, 48000 * 4, &peak);
    write_wav("song_user_beep", pcm, 48000 * 4);
    synth_song(SONG_USER + 1);
    render(pcm, 48000);
    int empty_peak;
    mean_square(pcm, 48000, &empty_peak);
    snprintf(what, sizeof(what), "the synth plays his song (mean square %.0f, peak %d) and an empty slot is silent", ms, peak);
    check(ms > 300.0 * 300.0 && peak < 32767 && empty_peak == 0 && synth_bpm(SONG_USER) == 130, what);

    static const char LEVEL[] = "{\"name\":\"MY TUNE\",\"song\":\"user:1\",\"objects\":[{\"t\":\"end\",\"x\":40,\"y\":0}]}";
    level_parse(&lv, LEVEL, (int)strlen(LEVEL));
    n = level_write(&lv, text, (int)sizeof(text));
    ok = lv.user_song == 1 && n > 0 && strstr(text, "\"song\":\"user:1\"") != NULL;
    level_parse(&lv, text, n);
    check(ok && lv.user_song == 1, "a level's song can be his: \"song\": \"user:1\" reads and writes");
    write_text(LEVELS_DIR "/tune.json", LEVEL);
    game_init(&g, 93);
    game_step(&g, 0);
    open_world(&g);
    g.level_sel = find_level(&g, "file:tune.json");
    press(&g, IN_CROSS);
    check(g.state == ST_PLAY && g.song == SONG_USER && g.run.bpm == 130, "his level plays his song at its speed");
    write_text(LEVELS_DIR "/theme.json", "{\"name\":\"THEME\",\"song\":13,\"objects\":[{\"t\":\"end\",\"x\":60,\"y\":0}]}");
    game_init(&g, 95);
    game_step(&g, 0);
    open_world(&g);
    g.level_sel = find_level(&g, "file:theme.json");
    press(&g, IN_CROSS);
    snprintf(what, sizeof(what), "a level with \"song\": 13 plays %s at %d BPM", SONG_NAMES[13], synth_bpm(13));
    check(g.state == ST_PLAY && g.song == 13 && g.run.bpm == synth_bpm(13), what);

    // The editor's song list goes on to his saved songs.
    game_init(&g, 94);
    game_step(&g, 0);
    g.title_sel = TITLE_EDITOR;
    press(&g, IN_CROSS);
    g.ed.slot_sel = 4;
    press(&g, IN_CROSS);
    press(&g, IN_R2);
    presses(&g, IN_DOWN, 7);
    press(&g, IN_LEFT);
    shot_game(&g, "33d_editor_his_song");
    presses(&g, IN_DOWN, 3);
    press(&g, IN_CROSS);                            // save
    n = read_text(LEVELS_DIR "/slot05.json", text, (int)sizeof(text));
    check(g.ed.screen == ED_MENU && n > 0 && strstr(text, "\"song\":\"user:1\"") != NULL,
          "the editor's SONG goes on to his own songs and saves them in the level");

    // Two Squares delete a song.
    reset_data();
    write_text(SONGS_DIR "/song03.json", "{\"format\":\"fritzdash-song\",\"version\":1,\"name\":\"OLD\",\"bpm\":100,"
                                         "\"lead\":\"1.3.5.8\",\"drums\":\"K.S.K.S.\"}");
    game_init(&g, 95);
    game_step(&g, 0);
    g.title_sel = TITLE_SONGS;
    press(&g, IN_CROSS);
    presses(&g, IN_RIGHT, 2);
    press(&g, IN_SQUARE);
    int still = songs_used(2);
    press(&g, IN_SQUARE);
    check(still && !songs_used(2) && plat_read_file(SONGS_DIR "/song03.json", text, 16) < 0,
          "Square twice deletes a song");
    press(&g, IN_CIRCLE);
}

// --- Built-in levels: solve, then screenshots along the way ----------------

static Level blv;

static void test_levels(int nsamples, char **samples)
{
    static char text[1 << 20];
    int skipped = 0;
    test_over_the_top();
    for (int i = 0; i < BUILTIN_COUNT; i++) {
        level_parse(&blv, BUILTIN_JSON[i], (int)strlen(BUILTIN_JSON[i]));
        skipped += blv.skipped;
        solve_level(&blv, BUILTIN_IDS[i], 1);
    }
    check(skipped == 0, "every object of every built-in level is one the game knows");
    for (int i = 0; i < nsamples; i++) {
        FILE *f = fopen(samples[i], "rb");
        int n = f ? (int)fread(text, 1, sizeof(text), f) : -1;
        if (f)
            fclose(f);
        int ok = n > 0 && level_parse(&blv, text, n) == LEVEL_OK && blv.skipped == 0;
        check(ok, "sample level file parses, with no unknown objects");
        if (ok)
            solve_level(&blv, samples[i], 0);
        // Four screenshots along each sample's solved run: s_<file>_1..4.
        const char *stem = strrchr(samples[i], '/') ? strrchr(samples[i], '/') + 1 : samples[i];
        if (ok && solve(&blv, 2, 0, 400000000L) == 1) {
            Run r;
            run_start(&r, &blv, 0, 0);
            for (int t = 0, k = 1; t < ntrace; t++) {
                run_step(&r, trace[t]);
                if (t == k * ntrace / 5) {
                    char name[96];
                    snprintf(name, sizeof(name), "s_%.*s_%d", (int)(strlen(stem) - 5), stem, k++);
                    shot_run(&r, name);
                }
            }
        }
    }

    static Level tour;
    tour = *mode_tour();
    solve_level(&tour, "mode tour (ball, UFO, wave, mini)", 0);
    if (solve(&tour, 2, 0, 400000000L) == 1) {
        static const struct { float at; const char *name; } TOUR[] = {
            { 0.12f, "18_mode_ball_portal" }, { 0.22f, "18b_mode_ball" }, { 0.40f, "18c_mode_ufo" },
            { 0.64f, "18d_mode_wave" }, { 0.83f, "18e_mode_mini" },
        };
        Run tr;
        run_start(&tr, &tour, 0, 0);
        for (int t = 0; t < ntrace; t++) {
            run_step(&tr, trace[t]);
            for (int s = 0; s < (int)(sizeof(TOUR) / sizeof(TOUR[0])); s++)
                if (t == (int)(TOUR[s].at * ntrace))
                    shot_run(&tr, TOUR[s].name);
        }
    }

    // A coming ship tunnel shows its ceiling from its portal on; nothing hangs over the cube.
    {
        static Level cl;
        static const char CORRIDOR[] = "{\"name\":\"TUNNEL\",\"objects\":[{\"t\":\"portal_ship\",\"x\":18,\"y\":1},"
            "{\"t\":\"block\",\"x\":22,\"y\":9},{\"t\":\"block\",\"x\":22,\"y\":8},{\"t\":\"block\",\"x\":23,\"y\":9},"
            "{\"t\":\"portal_cube\",\"x\":40,\"y\":1},{\"t\":\"end\",\"x\":60,\"y\":0}]}";
        level_parse(&cl, CORRIDOR, (int)strlen(CORRIDOR));
        Run r;
        int ok = 1, line_y = GROUND_Y - 10 * TILE - 3;
        run_start(&r, &cl, 0, 0);
        for (int t = 0; t < 20; t++)
            run_step(&r, 0);
        shot_run(&r, "31_corridor_ahead");
        int px, py;
        run_screen_pos(18.5f * TILE, 0, &px, &py);
        color_t line = run_colors(&r).line;
        ok &= r.b.mode == MODE_CUBE && px > 200 && px < SCREEN_W - 200;
        ok &= (fb[line_y * SCREEN_W + px - 100] & 0xFFFFFF) != line && (fb[line_y * SCREEN_W + px + 100] & 0xFFFFFF) == line;
        while (r.b.mode == MODE_CUBE && r.state == RUN_PLAY)
            run_step(&r, 0);
        for (int t = 0; t < 30; t++)
            run_step(&r, t % 12 < 6 ? IN_CROSS : 0);
        draw_set_target(fb);
        run_draw(&r, HUD_PROGRESS | HUD_TEXT);
        run_screen_pos(18.5f * TILE, 0, &px, &py);
        int hy = (int)(GROUND_Y - (r.b.hi - r.b.cam_y)) - 3;
        ok &= r.b.mode == MODE_SHIP && px > 20 && hy > 0 && hy < SCREEN_H;
        ok &= (fb[hy * SCREEN_W + px - 15] & 0xFFFFFF) != line && (fb[hy * SCREEN_W + px + 100] & 0xFFFFFF) == line;
        shot_run(&r, "31b_corridor_from_portal");
        check(ok, "flying corridors are drawn from their portal on, ahead of the cube and behind the ship");
    }

    {
        const Level *sh = showcase();
        Run sr;
        run_start(&sr, sh, 0, 0);
        sr.bpm = 126;
        for (int t = 0; t < 60; t++)
            run_step(&sr, 0);
        shot_run(&sr, "19_objects_showcase");
    }

    // The tutorial: first in the list, no stars, signs, and the STUDENT badge.
    {
        static Game g;
        static Level tut;
        int k = bi("00_how_to_dash"), signs = 0;
        level_parse(&tut, BUILTIN_JSON[k], (int)strlen(BUILTIN_JSON[k]));
        for (int i = 0; i < tut.nobj; i++)
            signs += tut.objs[i].type == OBJ_SIGN;
        check(k == 0 && tut.stars == 0 && signs >= 10 && tut.ncoins == 3,
              "HOW TO DASH comes first, earns no stars, and has signs and 3 coins");
        int ok = solve(&tut, 2, 0, 400000000L) == 1;
        static const struct { float at; const char *name; } TUT[] = {
            { 0.06f, "27_tutorial_jump" }, { 0.30f, "27b_tutorial_blocks" }, { 0.43f, "27c_tutorial_orb" },
            { 0.55f, "27d_tutorial_gravity" }, { 0.72f, "27e_tutorial_ship" }, { 0.90f, "27f_tutorial_practice" },
        };
        reset_data();
        game_init(&g, 81);
        game_step(&g, 0);
        open_world(&g);
        check(g.level_sel == 0 && !strcmp(g.levels[0].name, "HOW TO DASH"), "LEVELS opens on the tutorial");
        shot_game(&g, "02e_levels_tutorial");
        game_step(&g, IN_CROSS);
        for (int t = 0; ok && t < ntrace; t++) {
            game_step(&g, trace[t]);
            for (int i = 0; i < (int)(sizeof(TUT) / sizeof(TUT[0])); i++)
                if (t == (int)(TUT[i].at * ntrace))
                    shot_game(&g, TUT[i].name);
        }
        for (int f = 0; f < 140 && g.state == ST_PLAY; f++)
            game_step(&g, 0);
        check(g.state == ST_COMPLETE && g.new_stars == 0 && (g.badges >> B_STUDENT & 1),
              "finishing the tutorial earns the STUDENT badge");
    }

    // The three levels for the new modes, along their solved runs.
    {
        static const struct { const char *id; float at; const char *name; } NEW[] = {
            { "06_tiny_town", 0.149f, "28_tiny_town_mini" }, { "06_tiny_town", 0.223f, "28b_tiny_town_tunnel" },
            { "06_tiny_town", 0.371f, "28c_tiny_town_ship" }, { "06_tiny_town", 0.506f, "28d_tiny_town_upside_down" },
            { "06_tiny_town", 0.84f, "28e_tiny_town_second_ship" },
            { "07_ufo_bounce", 0.172f, "29_ufo_bounce_ufo" }, { "07_ufo_bounce", 0.310f, "29b_ufo_bounce_ball" },
            { "07_ufo_bounce", 0.427f, "29c_ufo_bounce_slalom" }, { "07_ufo_bounce", 0.537f, "29d_ufo_bounce_flipped" },
            { "07_ufo_bounce", 0.72f, "29e_ufo_bounce_spike_floor" }, { "07_ufo_bounce", 0.92f, "29f_ufo_bounce_second_ball" },
            { "08_zigzag_zone", 0.135f, "30_zigzag_slalom" }, { "08_zigzag_zone", 0.196f, "30b_zigzag_channel" },
            { "08_zigzag_zone", 0.304f, "30c_zigzag_spikes" }, { "08_zigzag_zone", 0.560f, "30d_zigzag_mini" },
            { "08_zigzag_zone", 0.80f, "30e_zigzag_spike_corridor" }, { "08_zigzag_zone", 0.92f, "30f_zigzag_second_mini" },
        };
        static Level nl;
        for (int i = 0; i < (int)(sizeof(NEW) / sizeof(NEW[0])); i++) {
            int k = bi(NEW[i].id);
            level_parse(&nl, BUILTIN_JSON[k], (int)strlen(BUILTIN_JSON[k]));
            if (solve(&nl, 2, 0, 400000000L) != 1)
                continue;
            Run r;
            run_start(&r, &nl, 0, 0);
            r.bpm = 120;
            for (int t = 0; t < (int)(NEW[i].at * ntrace); t++)
                run_step(&r, trace[t]);
            shot_run(&r, NEW[i].name);
        }
        static Game g;
        reset_data();
        game_init(&g, 82);
        game_step(&g, 0);
        open_world(&g);
        g.level_sel = bi("08_zigzag_zone");
        shot_game(&g, "02f_levels_zigzag");
    }

    // Screenshots from a solved play-through of each built-in level.
    static const struct { int level; float at; const char *name; } SHOTS[] = {
        { 0, 0.03f, "03_basics_start" }, { 0, 0.30f, "04_basics_platforms" }, { 0, 0.62f, "04b_basics_pad" },
        { 0, 0.80f, "04c_basics_orb" }, { 1, 0.20f, "05_portals_2x" }, { 1, 0.33f, "05b_portals_upside_down" },
        { 1, 0.55f, "06_portals_ship" }, { 1, 0.66f, "06b_portals_ship_gap" }, { 1, 0.85f, "06c_portals_3x" },
    };
    double draw_total = 0, draw_max = 0;
    int draw_count = 0;
    for (int i = 0; i < 2; i++) {
        int k = bi(i ? "02_candy_climb" : "01_sunny_hop");
        level_parse(&blv, BUILTIN_JSON[k], (int)strlen(BUILTIN_JSON[k]));
        if (solve(&blv, 2, 0, 400000000L) != 1)
            continue;
        Run r;
        run_start(&r, &blv, 0, 0);
        for (int t = 0; t < ntrace; t++) {
            run_step(&r, trace[t]);
            for (int s = 0; s < (int)(sizeof(SHOTS) / sizeof(SHOTS[0])); s++)
                if (SHOTS[s].level == i && t == (int)(SHOTS[s].at * ntrace))
                    shot_run(&r, SHOTS[s].name);
            if (t % 5 == 0) {
                draw_set_target(fb);
                clock_t c0 = clock();
                run_draw(&r, HUD_PROGRESS | HUD_TEXT);
                double dt = (double)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
                draw_total += dt;
                if (dt > draw_max)
                    draw_max = dt;
                draw_count++;
            }
        }
    }
    if (draw_count)
        printf("level draw time on this CPU: avg %.2f ms, max %.2f ms over %d frames\n",
               draw_total / draw_count, draw_max, draw_count);

    // Practice with checkpoints, and a death, on level 1.
    level_parse(&blv, BUILTIN_JSON[bi("01_sunny_hop")], (int)strlen(BUILTIN_JSON[bi("01_sunny_hop")]));
    solve(&blv, 2, 0, 400000000L);
    Run r;
    run_start(&r, &blv, 1, 0);
    for (int t = 0; t < 250 && r.state == RUN_PLAY; t++) {
        int in = trace[t];
        if (t == 150 || t == 200 || t == 240)
            in |= IN_SQUARE;
        run_step(&r, in);
    }
    check(r.state == RUN_PLAY && r.ncp == 3, "practice run with three checkpoints for the screenshot");
    shot_run(&r, "08_practice_checkpoints");
    while (r.state == RUN_PLAY)
        run_step(&r, 0);
    for (int t = 0; t < 8; t++)
        run_step(&r, 0);
    shot_run(&r, "08b_death");
}

// SIM_PATH=<level id> prints that level as text with the solver's path in it
// (lower case where the player's centre went), to help place coins.
static void print_path(const char *id)
{
    static Level lv;
    static char map[14][LEVEL_MAX_X / 4];
    for (int i = 0; i < BUILTIN_COUNT; i++) {
        if (strcmp(BUILTIN_IDS[i], id))
            continue;
        level_parse(&lv, BUILTIN_JSON[i], (int)strlen(BUILTIN_JSON[i]));
        int w = lv.end_x / TILE + 2;
        for (int y = 0; y < 14; y++) {
            memset(map[y], ' ', (size_t)w);
            map[y][w] = 0;
        }
        for (int k = 0; k < lv.nobj; k++) {
            const LevelObj *o = &lv.objs[k];
            static const char CH[OBJ_COUNT] = { [OBJ_BLOCK] = '#', [OBJ_SPIKE] = '^', [OBJ_HALF_SPIKE] = ',',
                [OBJ_PLATFORM] = '=', [OBJ_PAD_YELLOW] = 'y', [OBJ_PAD_PINK] = 'p', [OBJ_ORB_YELLOW] = 'o',
                [OBJ_ORB_PINK] = '*', [OBJ_GRAVITY_UP] = 'G', [OBJ_GRAVITY_DOWN] = 'g', [OBJ_SPEED_05] = '1',
                [OBJ_SPEED_1] = '2', [OBJ_SPEED_2] = '3', [OBJ_SPEED_3] = '4', [OBJ_CUBE_PORTAL] = 'C',
                [OBJ_SHIP_PORTAL] = 'S', [OBJ_BALL_PORTAL] = 'B', [OBJ_UFO_PORTAL] = 'U', [OBJ_WAVE_PORTAL] = 'W',
                [OBJ_MINI_PORTAL] = 'n', [OBJ_BIG_PORTAL] = 'N', [OBJ_COIN] = '$', [OBJ_SAW] = 'O', [OBJ_SAW_BIG] = '@',
                [OBJ_EGG] = 'E', [OBJ_FAKE_WALL] = 'F', [OBJ_FAKE_FLOOR] = 'f', [OBJ_EGG_NOJUMP] = 'Z', [OBJ_EGG_MINI] = 'Z',
                [OBJ_EGG_UPSIDE] = 'Z', [OBJ_EGG_ORBS] = 'Z' };
            if (o->y < 14 && o->x < w && CH[o->type])
                map[o->y][o->x] = o->type == OBJ_SPIKE && o->rot == 2 ? 'v' : CH[o->type];
        }
        solve_far = 0;
        int period = getenv("SIM_PERIOD") ? atoi(getenv("SIM_PERIOD")) : 2;
        int phase = getenv("SIM_PHASE") ? atoi(getenv("SIM_PHASE")) : 0;
        if (solve(&lv, period, phase, 30000000L) != 1)
            printf("NOT SOLVED: the search got as far as cell %.1f\n", solve_far / TILE);
        else {
            Run r;
            run_start(&r, &lv, 0, 0);
            for (int t = 0; t < ntrace; t++) {
                run_step(&r, trace[t]);
                int x = (int)(r.b.x / TILE), y = (int)(r.b.y / TILE);
                if (y >= 0 && y < 14 && x < w && map[y][x] == ' ')
                    map[y][x] = '.';
            }
        }
        for (int x0 = 0; x0 < w; x0 += 100) {
            printf("x=%d\n", x0);
            for (int y = 13; y >= 0; y--)
                printf("%2d %.100s\n", y, map[y] + x0);
        }
    }
}

// SIM_COINS="<level id> x,y x,y ..." tries a coin at each cell: can a
// finishing run get it, on how many 100 ms grids, and does the plain
// solution pick it up anyway.
static void probe_coins(const char *spec)
{
    static Level base, lv;
    char id[64];
    int n = 0;
    if (sscanf(spec, "%63s%n", id, &n) != 1)
        return;
    for (int i = 0; i < BUILTIN_COUNT; i++)
        if (!strcmp(BUILTIN_IDS[i], id))
            level_parse(&base, BUILTIN_JSON[i], (int)strlen(BUILTIN_JSON[i]));
    for (const char *p = spec + n; *p;) {
        int x, y, k = 0;
        if (sscanf(p, " %d,%d%n", &x, &y, &k) != 2)
            break;
        p += k;
        lv = base;
        if (level_find(&lv, x, y) >= 0) {
            printf("%d,%d: taken\n", x, y);
            continue;
        }
        LevelObj c = { (int16_t)x, (int16_t)y, OBJ_COIN, 0, 0, 0, 0, 0, 0, { 0 } };
        level_put(&lv, c);
        level_update_end(&lv);
        int bit = 1 << lv.objs[level_find(&lv, x, y)].param;
        want_coins(&lv, 0);
        int got = solve(&lv, 2, 0, 400000000L) == 1, auto_got = 0;
        if (got) {
            Run r;
            run_start(&r, &lv, 0, 0);
            for (int t = 0; t < ntrace; t++)
                run_step(&r, trace[t]);
            auto_got = (r.b.coins & bit) != 0;
        }
        want_coins(&lv, bit);
        int fine = solve(&lv, 2, 0, 6000000L), grid = 0;
        for (int ph = 0; ph < 6 && fine == 1; ph++)
            grid += solve(&lv, 6, ph ? ph : 6, 3000000L) == 1;
        want_coins(&lv, 0);
        printf("%d,%d: fine=%d grid=%d/6 auto=%d\n", x, y, fine, grid, auto_got);
        fflush(stdout);
    }
}

// SIM_EGGS="<id> X,Y Z:X,Y,DX W:X,Y ...": for each spot, whether an egg (X,Y)
// or an egg zone (Z is n, m, u or o: NO JUMP, MINI, UPSIDE DOWN, ORBS, DX
// cells long) or a fake wall hiding one (F:X,Y) there can be taken on a
// finishing run, on how many 100 ms grid offsets, and whether the plain
// solution takes it anyway (auto=1: on the way). W:X,Y puts a plain fake wall
// there for the spots after it.
static void probe_eggs(const char *spec)
{
    static Level base, lv;
    char id[64];
    int n = 0;
    if (sscanf(spec, "%63s%n", id, &n) != 1)
        return;
    for (int i = 0; i < BUILTIN_COUNT; i++)
        if (!strcmp(BUILTIN_IDS[i], id))
            level_parse(&base, BUILTIN_JSON[i], (int)strlen(BUILTIN_JSON[i]));
    for (const char *p = spec + n; *p;) {
        int x, y, dx = 0, k = 0, type = OBJ_EGG;
        char kind = 0;
        while (*p == ' ')
            p++;
        if (!*p)
            break;
        if (p[1] == ':') {
            kind = p[0];
            p += 2;
        }
        if (sscanf(p, "%d,%d%n", &x, &y, &k) != 2)
            break;
        p += k;
        if (*p == ',' && sscanf(p, ",%d%n", &dx, &k) == 1)
            p += k;
        if (kind == 'W') {
            LevelObj w = { (int16_t)x, (int16_t)y, OBJ_FAKE_WALL, 0, 0, 0, 0, 0, 0, { 0 } };
            level_put(&base, w);
            level_update_end(&base);
            continue;
        }
        if (kind == 'n' || kind == 'm' || kind == 'u' || kind == 'o')
            type = kind == 'n' ? OBJ_EGG_NOJUMP : kind == 'm' ? OBJ_EGG_MINI : kind == 'u' ? OBJ_EGG_UPSIDE : OBJ_EGG_ORBS;
        lv = base;
        if (kind == 'F' && level_find(&lv, x, y) >= 0 && lv.objs[level_find(&lv, x, y)].type == OBJ_FAKE_WALL)
            level_remove(&lv, level_find(&lv, x, y));
        if (kind == 'F')
            type = OBJ_FAKE_WALL;
        if (level_find(&lv, x, y) >= 0) {
            printf("%c%d,%d: taken\n", kind ? kind : ' ', x, y);
            continue;
        }
        LevelObj e = { (int16_t)x, (int16_t)y, (uint8_t)type, 0, 8, 0, (int8_t)dx, 0, 0, { 0 } };
        level_put(&lv, e);
        level_update_end(&lv);
        want_eggs(&lv, 0);
        int got = solve(&lv, 2, 0, 400000000L) == 1, auto_got = 0;
        if (got) {
            Run r;
            run_start(&r, &lv, 0, 0);
            for (int t = 0; t < ntrace; t++)
                run_step(&r, trace[t]);
            auto_got = (r.b.eggs & 128) != 0;
        }
        want_eggs(&lv, 128);
        int fine = solve(&lv, 2, 0, 400000000L), grid = 0;
        for (int ph = 0; ph < 6 && fine == 1; ph++)
            grid += solve(&lv, 6, ph ? ph : 6, 200000000L) == 1;
        want_eggs(&lv, 0);
        printf("%c%d,%d: fine=%d grid=%d/6 auto=%d\n", kind ? kind : ' ', x, y, fine, grid, auto_got);
        fflush(stdout);
    }
}

// --- Easter eggs ------------------------------------------------------------------

#define EGG_GRID 4      // 100 ms grid offsets (of 6) an egg must be reachable on

static int egg_caught = -1;     // >= 0: fake wall problems are counted here, not failed (the check's own test)

static int is_egg_obj(const LevelObj *o)
{
    return o->type == OBJ_EGG || (o->type >= OBJ_EGG_NOJUMP && o->type <= OBJ_EGG_ORBS) ||
           ((o->type == OBJ_FAKE_WALL || o->type == OBJ_FAKE_FLOOR) && o->param);
}

// Every egg of a level has one object and one name, can be taken on a run
// that finishes the level (fine, and on the 100 ms press grid), and no fake
// wall or floor hides a hazard close behind it.
static void egg_proofs(const Level *lv, const char *label)
{
    char what[300];
    int mask = 0, dup = 0, named = 1;
    for (int i = 0; i < lv->nobj; i++) {
        const LevelObj *o = &lv->objs[i];
        if (!is_egg_obj(o))
            continue;
        int bit = o->param >= 1 && o->param <= MAX_EGGS ? 1 << (o->param - 1) : 0;
        dup |= !bit || (mask & bit);
        mask |= bit;
        named &= level_egg(lv, o->param) != NULL;
    }
    for (int i = 0; i < lv->neggs; i++)
        named &= (mask >> (lv->eggs[i].n - 1)) & 1;
    if (mask || lv->neggs) {
        snprintf(what, sizeof(what), "%s: each of its %d eggs has one object and a name and hint", label, lv->neggs);
        check(named && !dup, what);
    }
    for (int n = 1; n <= MAX_EGGS; n++) {
        if (!(mask >> (n - 1) & 1))
            continue;
        want_eggs(lv, 1 << (n - 1));
        int fine = solve(lv, 2, 0, 400000000L) == 1 && replay(lv, trace, ntrace), grid = 0;
        for (int p = 0; p < 6 && fine; p++)
            grid += solve(lv, 6, p ? p : 6, 200000000L) == 1;
        want_eggs(lv, 0);
        const LevelEgg *e = level_egg(lv, n);
        snprintf(what, sizeof(what), "%s: egg %d %s can be taken on a run that finishes (100 ms grid: %d of 6)", label, n,
                 e ? e->name : "?", grid);
        check(fine && grid >= EGG_GRID, what);
    }
    for (int i = 0; i < lv->nobj; i++) {
        const LevelObj *o = &lv->objs[i];
        if (o->type != OBJ_FAKE_WALL && o->type != OBJ_FAKE_FLOOR)
            continue;
        int floor = o->type == OBJ_FAKE_FLOOR, bad = -1;
        for (int k = 0; k < lv->nobj && bad < 0; k++) {
            const LevelObj *h = &lv->objs[k];
            int kind = obj_kind(h->type), dx = h->x - o->x, dy = h->y - o->y, r = h->type == OBJ_SAW_BIG ? 1 : 0;
            if (kind != K_HAZARD && kind != K_SAW)
                continue;
            if (floor ? (dx >= -1 - r && dx <= 1 + r && dy <= -1 + r && dy >= -2 - r)
                      : (dx >= 1 - r && dx <= 2 + r && dy >= -1 - r && dy <= 1 + r))
                bad = k;
        }
        if (bad >= 0) {
            snprintf(what, sizeof(what), "%s: the fake %s at %d,%d hides a hazard at %d,%d", label, floor ? "floor" : "wall",
                     o->x, o->y, lv->objs[bad].x, lv->objs[bad].y);
            if (egg_caught < 0)
                check(0, what);
            else
                egg_caught++;
        }
    }
}

static Level egv;

static const Level *egg_level(const char *objects)
{
    static char json[8192];
    int n = snprintf(json, sizeof(json),
                     "{\"name\":\"EGGY\",\"eggs\":[{\"n\":1,\"name\":\"ONE\",\"hint\":\"A\"},{\"n\":2,\"name\":\"TWO\",\"hint\":\"B\"}],"
                     "\"objects\":[%s,{\"t\":\"end\",\"x\":44,\"y\":0}]}", objects);
    if (level_parse(&egv, json, n) != LEVEL_OK || egv.skipped) {
        printf("FAIL: egg test level did not parse\n");
        exit(1);
    }
    return &egv;
}

// Runs the level with X pressed (held one frame) whenever the player's cell is
// in presses; returns the eggs taken and whether it finished.
static int egg_run(const Level *lv, const int *presses, int n, int *done)
{
    Run r;
    run_start(&r, lv, 0, 0);
    *done = 0;
    for (int f = 0; f < 2000; f++) {
        int in = 0, cell = (int)(r.b.x / TILE);
        for (int k = 0; k < n; k++)
            in |= cell == presses[k] && !(r.prev_input & IN_CROSS) ? IN_CROSS : 0;
        int ev = run_step(&r, in);
        if (ev == EV_DIED)
            return r.b.eggs;
        if (ev == EV_COMPLETE) {
            *done = 1;
            return r.b.eggs;
        }
    }
    return r.b.eggs;
}

static void test_egg_objects(void)
{
    static Level plain, back;
    static char text[1 << 16];
    char what[200];
    const Level *lv = egg_level("{\"t\":\"egg\",\"x\":10,\"y\":0,\"c\":1},{\"t\":\"fake_wall\",\"x\":16,\"y\":0},"
                                "{\"t\":\"fake_wall\",\"x\":16,\"y\":1},{\"t\":\"egg_nojump\",\"x\":20,\"y\":0,\"c\":2,\"dx\":6}");
    check(lv->neggs == 2 && !strcmp(level_egg(lv, 1)->name, "ONE") && !strcmp(level_egg(lv, 2)->hint, "B") &&
          obj_kind(OBJ_EGG) == K_NONE && obj_kind(OBJ_FAKE_WALL) == K_NONE,
          "a level file names its eggs; eggs, fake walls and zones are not solid");
    static const char PLAIN[] = "{\"name\":\"EGGY\",\"objects\":[{\"t\":\"end\",\"x\":44,\"y\":0}]}";
    level_parse(&plain, PLAIN, (int)strlen(PLAIN));
    check(level_hash(lv) == level_hash(&plain), "eggs, fake walls and egg zones leave level_hash (ghosts, VERIFIED) alone");
    int n = level_write(lv, text, (int)sizeof(text));
    int ok = n > 0 && level_parse(&back, text, n) == LEVEL_OK && back.neggs == 2 && back.nobj == lv->nobj &&
             !strcmp(back.eggs[1].name, "TWO") && back.objs[level_find(&back, 20, 0)].dx == 6;
    check(ok, "a level with eggs writes and reads back the same");

    int done, got = egg_run(lv, NULL, 0, &done);
    check(got == 3 && done, "running straight takes egg 1, goes through the fake wall and earns the NO JUMP zone");
    static const int JUMP[1] = { 22 };
    got = egg_run(lv, JUMP, 1, &done);
    check(got == 1 && done, "a jump inside the NO JUMP zone keeps its egg");
    Run r;
    run_start(&r, lv, 0, 0);
    int frame = -1;
    for (int f = 0; f < 600 && frame < 0; f++) {
        run_step(&r, 0);
        if (r.egg_new & 1)
            frame = f;
    }
    check(frame > 0 && (r.sfx & SFX_EGG) && r.b.eggs == 1, "taking an egg sets egg_new and the egg sound for one frame");
    for (int f = 0; f < 6; f++)
        run_step(&r, 0);
    shot_run(&r, "70a_egg_taken");
    run_start(&r, lv, 0, 0);
    for (int f = 0; f < 50; f++)
        run_step(&r, 0);
    r.eggs_had = 0;
    shot_run(&r, "70_egg_in_level");
    want_eggs(lv, 3);
    int solved = solve(lv, 2, 0, 50000000L) == 1 && replay(lv, trace, ntrace);
    want_eggs(lv, 0);
    check(solved, "the solver finds a finishing run that takes both eggs");

    // A fake floor drops him through; mini, upside down and orb zones.
    lv = egg_level("{\"t\":\"block\",\"x\":0,\"y\":2},{\"t\":\"block\",\"x\":1,\"y\":2},{\"t\":\"block\",\"x\":2,\"y\":2},"
                   "{\"t\":\"block\",\"x\":3,\"y\":2},{\"t\":\"fake_floor\",\"x\":4,\"y\":2},{\"t\":\"fake_floor\",\"x\":5,\"y\":2},"
                   "{\"t\":\"fake_floor\",\"x\":6,\"y\":2},{\"t\":\"fake_floor\",\"x\":7,\"y\":2},"
                   "{\"t\":\"egg\",\"x\":7,\"y\":0,\"c\":1},{\"t\":\"egg_mini\",\"x\":30,\"y\":0,\"c\":2,\"dx\":5}");
    run_start(&r, lv, 0, 0);
    run_start_at(&r, 1);
    for (int f = 0; f < 80; f++)
        run_step(&r, 0);
    snprintf(what, sizeof(what), "a fake floor looks like a platform but he falls through it (y %.1f cells)", r.b.y / TILE);
    check(r.b.y < TILE && (r.b.eggs & 1) && r.state == RUN_PLAY, what);
    got = egg_run(lv, NULL, 0, &done);
    check(!(got & 2), "the MINI zone pays nothing at full size");
    lv = egg_level("{\"t\":\"portal_mini\",\"x\":20,\"y\":0},{\"t\":\"egg_mini\",\"x\":26,\"y\":0,\"c\":2,\"dx\":5},"
                   "{\"t\":\"egg\",\"x\":3,\"y\":0,\"c\":1}");
    got = egg_run(lv, NULL, 0, &done);
    check(got == 3, "crossing the MINI zone at mini size earns its egg");
    lv = egg_level("{\"t\":\"egg\",\"x\":3,\"y\":0,\"c\":1},{\"t\":\"portal_gravity_up\",\"x\":10,\"y\":1},"
                   "{\"t\":\"egg_upside\",\"x\":16,\"y\":0,\"c\":2,\"dx\":6},{\"t\":\"portal_gravity_down\",\"x\":30,\"y\":3}," 
                   "{\"t\":\"block\",\"x\":8,\"y\":5},{\"t\":\"block\",\"x\":9,\"y\":5},{\"t\":\"block\",\"x\":10,\"y\":5},"
                   "{\"t\":\"block\",\"x\":11,\"y\":5},{\"t\":\"block\",\"x\":12,\"y\":5},{\"t\":\"block\",\"x\":13,\"y\":5},"
                   "{\"t\":\"block\",\"x\":14,\"y\":5},{\"t\":\"block\",\"x\":15,\"y\":5},{\"t\":\"block\",\"x\":16,\"y\":5},"
                   "{\"t\":\"block\",\"x\":17,\"y\":5},{\"t\":\"block\",\"x\":18,\"y\":5},{\"t\":\"block\",\"x\":19,\"y\":5},"
                   "{\"t\":\"block\",\"x\":20,\"y\":5},{\"t\":\"block\",\"x\":21,\"y\":5},{\"t\":\"block\",\"x\":22,\"y\":5},"
                   "{\"t\":\"block\",\"x\":23,\"y\":5},{\"t\":\"block\",\"x\":24,\"y\":5},{\"t\":\"block\",\"x\":25,\"y\":5},"
                   "{\"t\":\"block\",\"x\":26,\"y\":5},{\"t\":\"block\",\"x\":27,\"y\":5},{\"t\":\"block\",\"x\":28,\"y\":5},"
                   "{\"t\":\"block\",\"x\":29,\"y\":5},{\"t\":\"block\",\"x\":30,\"y\":5},{\"t\":\"block\",\"x\":31,\"y\":5}");
    got = egg_run(lv, NULL, 0, &done);
    check(got == 3 && done, "running along the ceiling upside down earns the UPSIDE DOWN zone");
    lv = egg_level("{\"t\":\"egg\",\"x\":3,\"y\":0,\"c\":1},{\"t\":\"egg_orbs\",\"x\":10,\"y\":0,\"c\":2,\"dx\":20},"
                   "{\"t\":\"orb_yellow\",\"x\":14,\"y\":1},{\"t\":\"orb_yellow\",\"x\":19,\"y\":2},{\"t\":\"orb_yellow\",\"x\":24,\"y\":1}");
    got = egg_run(lv, NULL, 0, &done);
    want_eggs(lv, 2);
    solved = solve(lv, 2, 0, 50000000L) == 1 && replay(lv, trace, ntrace);
    want_eggs(lv, 0);
    check(!(got & 2) && solved, "the 3 ORBS zone pays only for three orbs hit inside it, and the solver finds how");

    // The sim's own proofs catch a hazard behind a fake wall.
    lv = egg_level("{\"t\":\"fake_wall\",\"x\":10,\"y\":0},{\"t\":\"spike\",\"x\":11,\"y\":0},{\"t\":\"egg\",\"x\":3,\"y\":0,\"c\":1},"
                   "{\"t\":\"egg\",\"x\":5,\"y\":0,\"c\":2}");
    egg_caught = 0;
    egg_proofs(lv, "test level");
    check(egg_caught == 1, "the egg proofs catch a spike hidden behind a fake wall");
    egg_caught = -1;
}

// The game's side: an egg taken in a built-in level is saved at once with a
// toast, counts in practice too, and stays found after a death.
// Plays built-in level key with the solver's presses for egg n, stopping a
// few frames after the egg is taken (returns 1 if it was).
static int play_for_egg(Game *g, const char *key, int n, int practice)
{
    g->level_sel = find_level(g, key);
    int b = g->levels[g->level_sel].builtin;
    level_parse(&egv, BUILTIN_JSON[b], (int)strlen(BUILTIN_JSON[b]));
    want_eggs(&egv, 1 << (n - 1));
    int ok = solve(&egv, 2, 0, 400000000L) == 1, got = -1;
    want_eggs(&egv, 0);
    game_step(g, 0);
    game_step(g, practice ? IN_SQUARE : IN_CROSS);
    for (int t = 0; ok && t < ntrace && g->state == ST_PLAY && (got < 0 || t < got + 4); t++) {
        game_step(g, trace[t]);
        if (got < 0 && (g->run.egg_new >> (n - 1) & 1))
            got = t;
    }
    return ok && got >= 0;
}

static void test_egg_game(void)
{
    static Game g;
    static char buf[8192];
    char what[200];
    reset_data();
    write_text(BADGES_FILE, "first_win 1\ncore_breaker 1\n");
    game_init(&g, 150);
    game_step(&g, 0);
    check(eggs_count() == 0 && eggs_total() >= GAME_EGG_COUNT && badge_got(&g, B_FIRST_WIN) && badge_got(&g, B_CORE) &&
          !badge_got(&g, B_EGG_FIRST) && g.badges2 == 0, "an old save loads: no eggs.txt, old badges kept, no egg badges");
    int regs = eggs_reg_count(), levels_with = 0;
    for (int i = 0; i < BUILTIN_COUNT; i++) {
        level_parse(&egv, BUILTIN_JSON[i], (int)strlen(BUILTIN_JSON[i]));
        levels_with += egv.neggs > 0;
    }
    snprintf(what, sizeof(what), "the SECRETS registry holds every built-in egg: %d level eggs in %d levels, %d game eggs",
             regs, levels_with, GAME_EGG_COUNT);
    check(regs >= 3 * levels_with && levels_with > 0, what);
    int bad = 0;
    for (int i = 0; i < GAME_EGG_COUNT; i++) {
        int r = -1;
        for (int k = 0; k < REWARD_COUNT; k++)
            if (!strcmp(REWARDS[k].key, GAME_EGGS[i].reward))
                r = k;
        bad += r < 0 || strlen(GAME_EGGS[i].name) > 20 || strlen(GAME_EGGS[i].riddle) > 60;
        for (int k = 0; k < i; k++)
            bad += !strcmp(GAME_EGGS[k].reward, GAME_EGGS[i].reward) || !strcmp(GAME_EGGS[k].key, GAME_EGGS[i].key);
    }
    int unlock = 0;
    for (int r = 0; r < REWARD_COUNT; r++) {
        int bound = 0;
        for (int i = 0; i < GAME_EGG_COUNT; i++)
            bound += !strcmp(GAME_EGGS[i].reward, REWARDS[r].key);
        unlock += (bound > 0) + (reward_needs(r) > 0) == 1;
        for (int k = 0; k < r; k++)
            bad += !strcmp(REWARDS[k].key, REWARDS[r].key);
    }
    snprintf(what, sizeof(what), "%d game eggs, each with its own reward; %d rewards, each unlocked one way", GAME_EGG_COUNT,
             REWARD_COUNT);
    check(GAME_EGG_COUNT >= 30 && REWARD_COUNT >= 80 && !bad && unlock == REWARD_COUNT, what);
    int every10 = 1;
    for (int c = 10; c <= GOLDEN_EGGS; c += 10)
        every10 &= reward_at_count(c) >= 0;
    check(every10, "every tenth egg unlocks a milestone reward, up to the GOLDEN cube at 200");

    // Egg 1 of HOW TO DASH, in a normal run.
    char id[EGG_ID];
    eggs_level_id(bi("00_how_to_dash"), 1, id);
    g.title_sel = TITLE_LEVELS;
    open_world(&g);
    int solved = play_for_egg(&g, "builtin:00_how_to_dash", 1, 0);
    read_text(EGGS_FILE, buf, sizeof(buf));
    snprintf(what, sizeof(what), "taking egg %s saves it to eggs.txt at once and pops up the cracking egg (%d toast)", id,
             g.negg_toasts);
    check(solved && eggs_found(id) && strstr(buf, id) && g.negg_toasts == 1 && eggs_count() == 1 && g.state == ST_PLAY,
          what);
    for (int f = 0; f < 3; f++)
        game_step(&g, 0);
    shot_game(&g, "71_egg_toast_crack");
    for (int f = 0; f < 50; f++)
        game_step(&g, 0);
    shot_game(&g, "71a_egg_toast");
    check(reward_unlocked(0) == 0 && reward_is_on(10), "the first egg unlocks HEARTS and puts it on");
    // Practice counts, and a death after it keeps it.
    eggs_level_id(bi("00_how_to_dash"), 2, id);
    press(&g, IN_START);
    presses(&g, IN_DOWN, 3);
    press(&g, IN_CROSS);
    solved = play_for_egg(&g, "builtin:00_how_to_dash", 2, 1);
    snprintf(what, sizeof(what), "an egg taken in practice counts too (state %d, practice %d)", g.state, g.run.practice);
    check(solved && eggs_found(id) && g.run.practice, what);
    for (int f = 0; f < 40; f++)
        game_step(&g, 0);
    shot_game(&g, "71b_egg_practice");
    game_init(&g, 151);
    game_step(&g, 0);
    check(eggs_count() == 2 && reward_is_on(10), "found eggs and worn rewards load back from eggs.txt");
}

// Every game egg, each by what finds it.
static void test_game_eggs(void)
{
    static Game g;
    static Level lv;
    reset_data();
    game_init(&g, 160);
    game_step(&g, 0);
    static const int CODE[8] = { IN_UP, IN_UP, IN_DOWN, IN_DOWN, IN_LEFT, IN_RIGHT, IN_LEFT, IN_RIGHT };
    for (int k = 0; k < 8; k++)
        press(&g, CODE[k]);
    check(eggs_found("g:konami") && g.state == ST_TITLE, "UP UP DOWN DOWN LEFT RIGHT LEFT RIGHT on the title");
    presses(&g, IN_TOUCH, 10);
    check(eggs_found("g:logo"), "the touchpad 10 times on the title");
    for (int f = 0; f < 70; f++)
        game_step(&g, IN_L2 | IN_R2);
    check(eggs_found("g:shoulders"), "L2 and R2 held on the title");
    for (int f = 0; f < EGG_IDLE_FRAMES; f++)
        game_step(&g, 0);
    shot_game(&g, "72_screensaver");
    check(eggs_found("g:idle"), "two minutes untouched on the title starts the screensaver");
    game_step(&g, IN_R1);
    game_step(&g, 0);
    check(g.egg_idle == 1 && g.state == ST_TITLE, "a press wakes it");
    strcpy(g.tagline, "I LOVE EGGS");
    game_step(&g, 0);
    strcpy(g.tagline, "FRITZ RULES");
    game_step(&g, 0);
    check(eggs_found("g:tag_egg") && eggs_found("g:tag_fritz"), "EGG and FRITZ under the logo");
    memset(&lv, 0, sizeof(lv));
    g.state = ST_EDITOR;
    lv.nobj = 500;
    eggs_step(&g, 0, 0, &lv);
    check(!eggs_found("g:editor_100"), "the editor's slot list does not count as building a level");
    lv.nobj = 0;
    g.ed.screen = ED_EDIT;
    static const char *const NAMES[3] = { "RAINBOW", "DISCO", "SNOW" };
    for (int i = 0; i < 3; i++) {
        strcpy(lv.name, NAMES[i]);
        eggs_step(&g, 0, 0, &lv);
    }
    lv.nobj = 100;
    eggs_step(&g, 0, 0, &lv);
    check(eggs_found("g:rainbow") && eggs_found("g:disco") && eggs_found("g:snow") && eggs_found("g:editor_100"),
          "levels named RAINBOW, DISCO and SNOW, and one with 100 things");
    g.state = ST_SONGS;
    for (int s = 0; s < USER_STEPS; s++)
        g.sm.song.notes[TRACK_DRUMS][s] = DRUM_KICK, g.sm.song.notes[TRACK_LEAD][s] = 3;
    g.frame = 9;
    game_step(&g, 0);
    check(eggs_found("g:drums") && eggs_found("g:melody"), "a song with every drum step on, and every lead step");
    g.state = ST_TITLE;
    write_text(ICONS_FILE, "icon1 0000000000200200000000000300003000333300000000000000000000000000\n"
                           "icon2 2222222222222222222222222222222222222222222222222222222222222222\n");
    game_init(&g, 161);
    game_step(&g, 0);
    g.title_sel = TITLE_ICONS;
    press(&g, IN_CROSS);
    for (int f = 0; f < 12; f++)
        game_step(&g, 0);
    press(&g, IN_R2);
    for (int f = 0; f < 12; f++)
        game_step(&g, 0);
    check(eggs_found("g:smiley") && eggs_found("g:full_icon"), "a smiley in the icon maker, and an icon all one colour");
    press(&g, IN_CIRCLE);
    g.title_sel = TITLE_CUSTOMIZE;
    press(&g, IN_CROSS);
    for (int row = 0; row < LOOK_PARTS; row++) {
        g.look.v[row] = (g.look.v[row] + 1) % LOOK_COUNT[row];
        game_step(&g, 0);
    }
    check(eggs_found("g:fashion"), "changing every row in Customize");
    g.look.v[LOOK_COL2] = g.look.v[LOOK_COL1];
    game_step(&g, 0);
    check(eggs_found("g:twins"), "colour 1 the same as colour 2");
    press(&g, IN_TRIANGLE);
    for (int k = 0; k < STOCK_SET_COUNT; k++)
        press(&g, IN_R1);
    check(eggs_found("g:stock_tour"), "every set of STOCK ICONS");
    press(&g, IN_CIRCLE);
    press(&g, IN_CIRCLE);
    g.title_sel = TITLE_BADGES;
    press(&g, IN_CROSS);
    presses(&g, IN_RIGHT, BADGE_COUNT);
    check(eggs_found("g:badge_tour"), "the cursor on every badge");
    press(&g, IN_CIRCLE);
    eggs_set_clock(10, 31, 21);
    game_step(&g, 0);
    eggs_set_clock(12, 25, 6);
    game_step(&g, 0);
    eggs_set_clock(1, 1, 12);
    game_step(&g, 0);
    eggs_set_clock(0, 0, 0);
    check(eggs_found("g:halloween") && eggs_found("g:christmas") && eggs_found("g:newyear") && eggs_found("g:night_owl") &&
          eggs_found("g:early_bird"), "Halloween, Christmas, New Year's Day, after 8 at night and before 7 in the morning");
    g.stats[STAT_JUMPS] = 1000;
    game_step(&g, 0);
    g.state = ST_ENDLESS;
    g.endless.score = 25;
    eggs_step(&g, 0, 0, NULL);
    g.state = ST_RACE_DONE;
    g.winner = 2;
    eggs_step(&g, 0, 0, NULL);
    g.state = ST_WORLD;
    g.world.n = 5;
    eggs_step(&g, 0, 0, NULL);
    g.state = ST_PLAY;
    g.run.practice = 1;
    g.run.ncp = 20;
    g.hint.state = HINT_SEARCH;
    eggs_step(&g, 0, 0, NULL);
    g.hint.state = HINT_IDLE;
    check(eggs_found("g:jumps") && eggs_found("g:endless") && eggs_found("g:player2") && eggs_found("g:world_path") &&
          eggs_found("g:checkpoints") && eggs_found("g:show_me"),
          "1000 jumps, 25 in Endless, player 2 winning, a 5 level world path, 20 checkpoints and SHOW ME HOW");
    g.level_sel = find_level(&g, "builtin:01_sunny_hop");
    g.playtest = 0;
    g.run.practice = 0;
    g.run.pct = 99;
    eggs_after_run(&g, EV_DIED);
    g.levels[g.level_sel].attempts = 100;
    eggs_after_run(&g, EV_DIED);
    g.music_vol = 0;
    g.run.pct = 100;
    g.run.b.coins = (1 << g.levels[g.level_sel].ncoins) - 1;
    eggs_after_run(&g, EV_COMPLETE);
    check(eggs_found("g:so_close") && eggs_found("g:never_give_up") && eggs_found("g:silent_win") &&
          eggs_found("g:coin_run") && g.levels[g.level_sel].ncoins > 0,
          "crashing at 99%, 100 tries on a level, a win with the music off, and every coin in one run");
    g.state = ST_TITLE;
    for (int f = 0; f < 40; f++)
        game_step(&g, 0);
    char what[160];
    snprintf(what, sizeof(what), "all %d game eggs found (%d), and the RIDDLE MASTER badge", GAME_EGG_COUNT, eggs_game_found());
    check(eggs_game_found() == GAME_EGG_COUNT && badge_got(&g, B_EGG_GAME) && badge_got(&g, B_EGG_FIRST), what);
    for (int f = 0; f < 3000 && g.negg_toasts; f++)
        game_step(&g, 0);
    check(g.negg_toasts == 0, "the egg toasts pop up one at a time and go");
}

// --- Rewards: a screenshot of each, and what a frame costs with it ---------------

static Level rwl;
static Run rw_run;

static double ms_now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e3 + (double)t.tv_nsec / 1e6;
}

// Milliseconds run_draw takes a frame: the best of 5 tries of n frames, so
// other work on the machine does not count.
static double draw_ms(const Run *r, int n)
{
    draw_set_target(fb);
    double best = 1e9;
    for (int k = 0; k < 5; k++) {
        double t0 = ms_now();
        for (int i = 0; i < n; i++) {
            rewards_frame(i);
            run_draw(r, HUD_PROGRESS | HUD_TEXT);
        }
        double t = (ms_now() - t0) / n;
        best = t < best ? t : best;
    }
    return best;
}

// Level id played with its solution to frame f (or to its death without
// presses when f < 0), with jump confetti fed as the game does.
static void rw_play(const char *id, int f)
{
    level_parse(&rwl, BUILTIN_JSON[bi(id)], (int)strlen(BUILTIN_JSON[bi(id)]));
    int ok = f < 0 || solve(&rwl, 2, 0, 400000000L) == 1;
    run_start(&rw_run, &rwl, 0, 0);
    Look l = { { 0, 1, 1, 0, 0, 0 }, 0 };
    rw_run.look = l;
    look_set(&l);
    for (int t = 0; ok && t < (f < 0 ? 600 : f); t++) {
        int ev = run_step(&rw_run, f < 0 ? 0 : t < ntrace ? trace[t] : 0);
        if (rw_run.sfx & SFX_JUMP)
            rw_event(RW_JUMP, rw_run.b.x, rw_run.b.y);
        rw_tick();
        if (f < 0 && ev == EV_DIED)
            f = t + 14;
    }
}

static void rw_shot(const char *name)
{
    draw_set_target(fb);
    rewards_frame(rw_run.frame);
    run_draw(&rw_run, HUD_PROGRESS | HUD_TEXT);
    save_ppm(name);
}

static int reward_named(const char *key)
{
    for (int r = 0; r < REWARD_COUNT; r++)
        if (!strcmp(REWARDS[r].key, key))
            return r;
    printf("FAIL: no reward %s\n", key);
    exit(1);
}

static void test_egg_rewards(void)
{
    char name[64], what[300];
    reset_data();
    eggs_load();
    rw_play("01_sunny_hop", 260);
    double plain = draw_ms(&rw_run, 12), worst = 0;
    int worst_r = -1, bad = 0, nshots = 0;
    rw_shot("73_reward_none");
    printf("     a plain frame of 01_sunny_hop draws in %.2f ms here\n", plain);
    for (int r = 0; r < REWARD_COUNT; r++) {
        int kind = REWARDS[r].kind;
        if (kind == RK_ICON || kind == RK_VAULT || kind == RK_SONG || kind == RK_SOUNDS || kind == RK_TITLE)
            continue;
        reward_wear(r, 1);
        const char *key = REWARDS[r].key;
        if (kind == RK_DEATH || !strcmp(key, "pa_slowmo"))
            rw_play("01_sunny_hop", -1);
        else if (!strcmp(key, "pa_fireworks")) {
            rw_play("00_how_to_dash", 6000);
            for (int f = 0; f < 30; f++)
                run_step(&rw_run, 0);
        }
        else if (kind == RK_FINISH || kind == RK_PET || !strcmp(key, "pa_hat") || !strcmp(key, "pa_googly") ||
                 !strcmp(key, "pa_bighead"))
            rw_play("04_rocket_run", 330);
        else
            rw_play("01_sunny_hop", kind == RK_TRAIL || !strcmp(key, "pa_confetti") ? 1238 : 260);
        snprintf(name, sizeof(name), "74_%s", key);
        rw_shot(name);
        nshots++;
        double ms = draw_ms(&rw_run, 12);
        reward_wear(r, 0);
        double base = draw_ms(&rw_run, 12), ratio = ms / base;
        printf("     %-14s %.3f ms, the same frame plain %.3f ms (%.2fx)\n", key, ms, base, ratio);
        if (ratio > worst)
            worst = ratio, worst_r = r;
        bad += ratio > 1.6;
    }
    snprintf(what, sizeof(what), "%d reward screenshots; every one draws within 1.6x the same frame plain (the dearest, %s, %.2fx)",
             nshots, worst_r >= 0 ? REWARDS[worst_r].name : "?", worst);
    check(nshots >= 50 && !bad, what);

    // The finishes, hat and googly eyes up close on the cube, and every pet.
    draw_set_target(fb);
    draw_gradient(RGB(20, 14, 40), RGB(50, 20, 80));
    static const char *const CLOSE[] = { "fi_chrome", "fi_glitch", "fi_rainbow", "fi_galaxy", "fi_neon", "fi_candy",
                                         "fi_golden", "pa_hat", "pa_googly" };
    Look l = { { 0, 1, 1, 0, 0, 0 }, 0 };
    look_set(&l);
    for (int i = 0; i < 9; i++) {
        int r = reward_named(CLOSE[i]);
        reward_wear(r, 1);
        rewards_frame(12 + i * 7);
        run_draw_pilot(140 + (float)(i % 5) * 370, 260 + (float)(i / 5) * 420, 2.6f, 0.15f);
        draw_text(40 + (i % 5) * 370, 440 + (i / 5) * 420, 4, REWARDS[r].name, RGB(255, 255, 255));
        reward_wear(r, 0);
    }
    save_ppm("75_finishes");
    draw_gradient(RGB(30, 60, 110), RGB(80, 150, 220));
    for (int p = 0; p < 8; p++) {
        int r = reward_named((const char *const[]){ "pet_robot", "pet_bird", "pet_ghost", "pet_dragon", "pet_ufo", "pet_cat",
                                                    "pet_slime", "pet_star" }[p]);
        reward_wear(r, 1);
        float tx[TRAIL_N], ty[TRAIL_N];
        for (int i = 0; i < TRAIL_N; i++)
            tx[i] = (float)(220 + (p % 4) * 440 - i * 12), ty[i] = (float)(GROUND_Y - 300 - (p / 4) * 420);
        look_draw_preview(240 + (p % 4) * 440, 300 + (p / 4) * 420, 2000, 10 + p * 9);
        draw_text(120 + (p % 4) * 440, 470 + (p / 4) * 420, 4, REWARDS[r].name, RGB(255, 255, 255));
        reward_wear(r, 0);
    }
    save_ppm("75b_pets");
}

static void test_egg_screens(void)
{
    static Game g;
    char what[200];
    reset_data();
    game_init(&g, 170);
    game_step(&g, 0);
    // A few eggs found: two in HOW TO DASH (one level beaten), some game eggs.
    write_text(EGGS_FILE, "00:1\ng:konami\ng:logo\n00:3\ng:smiley\n+tr_hearts\n+bg_retro\n");
    game_init(&g, 171);
    game_step(&g, 0);
    check(eggs_count() == 5 && reward_is_on(reward_named("bg_retro")) && !reward_is_on(reward_named("pa_hat")),
          "eggs.txt brings back found eggs and only the rewards it turned on");
    g.levels[find_level(&g, "builtin:00_how_to_dash")].best = 100;
    g.title_sel = TITLE_SECRETS;
    press(&g, IN_CROSS);
    check(g.state == ST_BOOK && g.book_page == 0, "SECRETS on the title opens the book on the game eggs page");
    presses(&g, IN_RIGHT, 2);
    shot_game(&g, "76_book_game");
    presses(&g, IN_DOWN, 1);
    shot_game(&g, "76a_book_riddle");
    press(&g, IN_R1);
    check(g.book_page == 1, "R1 turns the page");
    shot_game(&g, "76b_book_world1");
    press(&g, IN_RIGHT);
    shot_game(&g, "76c_book_hint");
    press(&g, IN_L1);
    press(&g, IN_CIRCLE);
    check(g.state == ST_TITLE, "Circle closes the book");
    shot_game(&g, "76d_title_secrets");
    g.title_sel = TITLE_SETTINGS;
    press(&g, IN_CROSS);
    g.settings_sel = 7;
    press(&g, IN_CROSS);
    check(g.state == ST_SECRETS, "SECRETS in Settings opens the SECRETS page");
    press(&g, IN_RIGHT);
    int next = reward_worn(RK_BG);
    press(&g, IN_LEFT);
    snprintf(what, sizeof(what), "Right on BACKGROUND changes it (to %d) and Left puts RETRO GRID back (%d)", next,
             reward_worn(RK_BG));
    check(next != BGX_RETRO && reward_worn(RK_BG) == BGX_RETRO, what);
    presses(&g, IN_DOWN, 1);
    for (int f = 0; f < 30; f++)
        game_step(&g, 0);
    shot_game(&g, "77_secrets_page");
    presses(&g, IN_UP, 2);
    press(&g, IN_CROSS);
    check(!reward_is_on(reward_named("pa_floor")), "a party trick not unlocked yet stays off");
    press(&g, IN_CIRCLE);
    check(g.state == ST_SETTINGS, "Circle goes back to Settings");
    shot_game(&g, "77a_settings_secrets");
    press(&g, IN_CIRCLE);
    // The title variants.
    static const char *const TITLES[4] = { "ti_night", "ti_rainbow", "ti_eggs", "ti_party" };
    for (int i = 0; i < 4; i++) {
        reward_wear(reward_named(TITLES[i]), 1);
        g.nbadge_toasts = g.negg_toasts = 0;
        for (int f = 0; f < 20; f++)
            game_step(&g, 0);
        char name[40];
        snprintf(name, sizeof(name), "78_title_%s", TITLES[i] + 3);
        shot_game(&g, name);
    }
    reward_wear(reward_named("ti_party"), 0);
    // The badge screen with the egg badges.
    g.title_sel = TITLE_BADGES;
    press(&g, IN_CROSS);
    presses(&g, IN_RIGHT, B_EGG_FIRST);
    shot_game(&g, "79_badges_eggs");
    check(badge_got(&g, B_EGG_FIRST) && g.badge_sel == B_EGG_FIRST, "EGG HUNTER is earned and shows on the badge screen");
    press(&g, IN_CIRCLE);
}

// The editor shows eggs, fake walls (marked) and zones; in a level a fake
// wall looks like any other block.
static void test_egg_looks(void)
{
    static Game g;
    reset_data();
    eggs_load();
    const Level *lv = egg_level("{\"t\":\"egg\",\"x\":4,\"y\":1,\"c\":1},{\"t\":\"fake_wall\",\"x\":8,\"y\":0},"
                                "{\"t\":\"fake_wall\",\"x\":8,\"y\":1,\"c\":2},{\"t\":\"block\",\"x\":9,\"y\":0},"
                                "{\"t\":\"block\",\"x\":9,\"y\":1},{\"t\":\"fake_floor\",\"x\":12,\"y\":2},"
                                "{\"t\":\"egg_nojump\",\"x\":15,\"y\":0,\"dx\":5,\"c\":3},{\"t\":\"egg_orbs\",\"x\":15,\"y\":3,\"dx\":4,\"c\":4}");
    draw_set_target(fb);
    level_draw(lv, LEVEL_PALETTES[2], 0, 0, 30);
    save_ppm("70c_egg_editor");
    Run r;
    run_start(&r, lv, 0, 0);
    for (int f = 0; f < 20; f++)
        run_step(&r, 0);
    shot_run(&r, "70d_fake_wall_play");
    level_parse(&rwl, BUILTIN_JSON[bi("00_how_to_dash")], (int)strlen(BUILTIN_JSON[bi("00_how_to_dash")]));
    want_eggs(&rwl, 4);
    int ok = solve(&rwl, 2, 0, 400000000L) == 1;
    want_eggs(&rwl, 0);
    run_start(&r, &rwl, 0, 0);
    for (int t = 0; ok && t < ntrace && r.b.x < 254.0f * TILE; t++)
        run_step(&r, trace[t]);
    shot_run(&r, "70e_knock_knock");
    check(ok, "HOW TO DASH's last wall hides KNOCK KNOCK (screenshot)");
    // The editor: the EASTER EGG brush, and Triangle sets its number.
    static Level ed;
    static char text[1 << 16];
    reset_data();
    game_init(&g, 174);
    game_step(&g, 0);
    g.title_sel = TITLE_EDITOR;
    press(&g, IN_CROSS);
    press(&g, IN_CROSS);
    for (int k = 0; k < 200 && editor_brush_type(&g.ed) != OBJ_EGG; k++)
        press(&g, IN_R1);
    presses(&g, IN_RIGHT, 6);
    press(&g, IN_UP);
    press(&g, IN_CROSS);
    press(&g, IN_TRIANGLE);
    int panel = g.ed.screen == ED_OBJSET;
    presses(&g, IN_RIGHT, 2);
    shot_game(&g, "70f_editor_egg_settings");
    press(&g, IN_CIRCLE);
    press(&g, IN_RIGHT);
    for (int k = 0; k < 200 && editor_brush_type(&g.ed) != OBJ_FAKE_WALL; k++)
        press(&g, IN_R1);
    press(&g, IN_CROSS);
    for (int f = 0; f < 20; f++)
        game_step(&g, 0);
    shot_game(&g, "70g_editor_fake_wall");
    press(&g, IN_R2);
    presses(&g, IN_DOWN, 10);
    press(&g, IN_CROSS);
    int n = plat_read_file(LEVELS_DIR "/slot01.json", text, (int)sizeof(text)), egg = -1, wall = -1;
    for (int i = 0; n > 0 && level_parse(&ed, text, n) == LEVEL_OK && i < ed.nobj; i++) {
        if (ed.objs[i].type == OBJ_EGG)
            egg = i;
        if (ed.objs[i].type == OBJ_FAKE_WALL)
            wall = i;
    }
    check(panel && egg >= 0 && ed.objs[egg].param == 3 && wall >= 0,
          "the editor places an EASTER EGG (Triangle sets its number to 3) and a FAKE WALL, saved in his level");

    // last.txt from 3.1 (MY LEVELS was world 6) comes back to the right worlds.
    reset_data();
    write_text(LAST_FILE, "world 6\n6 file:slot01.json\n0 builtin:01_sunny_hop\n");
    game_init(&g, 175);
    check(g.lworld_sel == LWORLD_MINE && !strcmp(g.last_key[LWORLD_MINE], "file:slot01.json") &&
          !strcmp(g.last_key[0], "builtin:01_sunny_hop"), "a 3.1 last.txt keeps MY LEVELS and its levels");
}

// The VAULT: hidden until the first egg, then a locked card until 100 eggs.
static void test_egg_vault(void)
{
    static Game g;
    static char buf[MAX_FOUND * EGG_ID];
    char what[200];
    int vault = 0;
    for (int i = 0; i < BUILTIN_COUNT; i++)
        vault += BUILTIN_IDS[i][0] == '7';
    snprintf(what, sizeof(what), "the VAULT world has 5 bonus levels, 70 to 74 (%d)", vault);
    check(vault == 5, what);
    reset_data();
    game_init(&g, 180);
    game_step(&g, 0);
    g.title_sel = TITLE_LEVELS;
    press(&g, IN_CROSS);
    int seen = 0;
    for (int k = 0; k < NUM_LWORLDS; k++) {
        press(&g, IN_RIGHT);
        seen |= g.lworld_sel == LWORLD_VAULT;
    }
    check(!seen, "with no eggs the VAULT card is hidden");
    write_text(EGGS_FILE, "00:1\n");
    game_init(&g, 181);
    game_step(&g, 0);
    g.title_sel = TITLE_LEVELS;
    press(&g, IN_CROSS);
    for (int k = 0; k < NUM_LWORLDS && g.lworld_sel != LWORLD_VAULT; k++)
        press(&g, IN_RIGHT);
    press(&g, IN_CROSS);
    check(g.lworld_sel == LWORLD_VAULT && g.picking && g.msg_frames > 0 && strstr(g.msg, "100"),
          "after the first egg the VAULT card shows, locked: X says how many eggs open it");
    shot_game(&g, "80a_vault_locked");
    // Enough eggs for 100: spare egg numbers on every built-in level, for this test only.
    static Level extra;
    memset(&extra, 0, offsetof(Level, objs));
    extra.neggs = MAX_EGGS;
    for (int n = 1; n <= MAX_EGGS; n++) {
        extra.eggs[n - 1].n = n;
        snprintf(extra.eggs[n - 1].name, EGG_NAME, "TEST EGG %d", n);
        snprintf(extra.eggs[n - 1].hint, EGG_HINT, "A TEST");
    }
    for (int b = 0; b < BUILTIN_COUNT; b++)
        eggs_register(b, &extra);
    int n = 0, len = 0;
    for (int i = 0; i < GAME_EGG_COUNT && n < VAULT_EGGS; i++, n++)
        len += snprintf(buf + len, sizeof(buf) - (size_t)len, "g:%s\n", GAME_EGGS[i].key);
    for (int i = 0; i < eggs_reg_count() && n < VAULT_EGGS; i++, n++) {
        char id[EGG_ID];
        eggs_level_id(eggs_reg(i)->builtin, eggs_reg(i)->e.n, id);
        len += snprintf(buf + len, sizeof(buf) - (size_t)len, "%s\n", id);
    }
    write_text(EGGS_FILE, buf);
    game_init(&g, 182);
    game_step(&g, 0);
    for (int f = 0; f < 31; f++)
        game_step(&g, 0);
    snprintf(what, sizeof(what), "100 eggs open the VAULT and earn VAULT KEY (%d eggs)", eggs_count());
    check(eggs_count() == VAULT_EGGS && eggs_vault_open() && badge_got(&g, B_EGG100) && !reward_unlocked(reward_named("fi_golden")),
          what);
    g.nbadge_toasts = 0;
    g.title_sel = TITLE_LEVELS;
    press(&g, IN_CROSS);
    for (int k = 0; k < NUM_LWORLDS && g.lworld_sel != LWORLD_VAULT; k++)
        press(&g, IN_RIGHT);
    shot_game(&g, "80b_vault_open");
    press(&g, IN_CROSS);
    check(!g.picking && g.levels[g.level_sel].world == LWORLD_VAULT, "X goes into the VAULT");
    shot_game(&g, "80c_vault_levels");
    game_step(&g, 0);
    game_step(&g, IN_CROSS);
    for (int f = 0; f < 150; f++)
        game_step(&g, 0);
    shot_game(&g, "80d_vault_play");
    g.state = ST_TITLE;
    g.title_sel = TITLE_SECRETS;
    press(&g, IN_CROSS);
    press(&g, IN_L1);
    shot_game(&g, "80e_book_vault");
    check(g.state == ST_BOOK, "the book has a page for the VAULT once it is open");
}

// MODE LAB: a PLAY entry with all 30 modes; each has a how-to line and a
// built-in demo the solver finishes with over 5 s in that mode. X plays it as
// a trial that changes no progress, stats, coins, eggs or badges, and the
// pause menu and Circle go back to the lab.
static void test_mode_lab(void)
{
    static Game g;
    static Level lv;
    static char before[4096], after[4096];
    char what[400], missing[240] = "";
    int have = 0;
    for (int m = 0; m < MODE_COUNT; m++) {
        int i = lab_level(m), frames = 0;
        if (i >= 0 && level_parse(&lv, LAB_JSON[i], (int)strlen(LAB_JSON[i])) == LEVEL_OK &&
            solve(&lv, 2, 0, 400000000L) == 1) {
            Run r;
            run_start(&r, &lv, 0, 0);
            for (int t = 0; t < ntrace; t++) {
                run_step(&r, trace[t]);
                frames += r.b.mode == m;
            }
        }
        if (frames > 300 && lab_how(m)[0])
            have++;
        else
            snprintf(missing + strlen(missing), sizeof(missing) - strlen(missing), " %s", MODES[m] ? MODES[m]->label : "?");
    }
    snprintf(what, sizeof(what), "MODE LAB has all 30 modes (%d of %d demos built in), each with a how-to line and a "
             "demo the solver finishes spending over 5 s in it%s%s", have, LAB_COUNT, missing[0] ? "; not" : "", missing);
    check(have == MODE_COUNT && LAB_COUNT == MODE_COUNT, what);

    reset_data();
    beat_builtins(-1);
    game_init(&g, 200);
    for (int f = 0; f < 90; f++)                    // the badges all those wins earn come first
        game_step(&g, 0);
    g.nbadge_toasts = g.negg_toasts = 0;            // (their pop-ups would cover the pictures)
    int n0 = read_text(PROGRESS_FILE, before, sizeof(before)), stats0[STAT_COUNT];
    memcpy(stats0, g.stats, sizeof(stats0));
    uint32_t badges0 = g.badges, badges20 = g.badges2;
    g.title_sel = TITLE_LAB;
    press(&g, IN_CROSS);
    int open = g.state == ST_LAB;
    presses(&g, IN_RIGHT, MODE_GLIDER);
    for (int f = 0; f < 20; f++)
        game_step(&g, 0);
    shot_game(&g, "82a_mode_lab");
    press(&g, IN_CROSS);
    int trial = g.state == ST_PLAY && g.playtest == PLAYTEST_LAB && g.run.lv && !strcmp(g.run.lv->name, "SKY SAIL");
    char name[40];
    snprintf(name, sizeof(name), "%s", g.run.lv ? g.run.lv->name : "");
    // Die on the first spike a few times (no presses), then a run along the solver's way for a picture.
    int died = 0;
    for (int f = 0; f < 400; f++) {
        int was = g.run.state;
        game_step(&g, 0);
        died += was != RUN_DEAD && g.run.state == RUN_DEAD;
    }
    if (solve(g.run.lv, 2, 0, 400000000L) == 1) {
        run_start(&g.run, g.run.lv, 0, 0);
        g.run.look = g.look;
        for (int t = 0; t < ntrace && t < 900; t++)
            run_step(&g.run, trace[t]);
        shot_game(&g, "82b_mode_lab_glider");
    }
    game_step(&g, IN_START);
    game_step(&g, 0);
    presses(&g, IN_DOWN, 3);                        // RESUME, RESTART, PRACTICE, BACK TO MODE LAB
    for (int f = 0; f < 10; f++)
        game_step(&g, 0);
    shot_game(&g, "82c_mode_lab_pause");
    press(&g, IN_CROSS);
    int back = g.state == ST_LAB && !g.playtest;
    int n1 = read_text(PROGRESS_FILE, after, sizeof(after));
    int same = n0 == n1 && !memcmp(before, after, (size_t)(n0 > 0 ? n0 : 0)) && !memcmp(stats0, g.stats, sizeof(stats0)) &&
               g.badges == badges0 && g.badges2 == badges20;
    press(&g, IN_CIRCLE);
    snprintf(what, sizeof(what), "MODE LAB opens from PLAY (%d), X tries the GLIDER demo (%s, a trial %d), deaths in it "
             "(%d) change no progress, stats or badges (%d), EXIT goes back to the lab (%d) and Circle to the title (%d)",
             open, name, trial, died, same, back, g.state == ST_TITLE);
    check(open && trial && died && same && back && g.state == ST_TITLE, what);
}

// World 7 SHAPESHIFT (ids 6x): its card shows only once it has levels, after
// COSMIC CORE and before the VAULT; every 6x level is in it; its level list
// plays PRISM SHIFT; beating all of them earns SHAPE MASTER.
static void test_shapeshift_world(void)
{
    static Game g;
    static char prog[MAX_LEVELS * 100];
    char what[240];
    int n = 0, np = 0, inside = 0;
    for (int i = 0; i < BUILTIN_COUNT; i++)
        if (BUILTIN_IDS[i][0] == '6') {
            n++;
            np += snprintf(prog + np, sizeof(prog) - (size_t)np, "1 100 0 builtin:%s\n", BUILTIN_IDS[i]);
        }
    reset_data();
    write_text(EGGS_FILE, "00:1\n");
    game_init(&g, 190);
    for (int f = 0; f < 40; f++)
        game_step(&g, 0);
    g.nbadge_toasts = g.negg_toasts = 0;            // EGG HUNTER's pop-up would cover the cards
    for (int i = 0; i < g.nlevels; i++)
        inside += g.levels[i].builtin >= 0 && BUILTIN_IDS[g.levels[i].builtin][0] == '6' &&
                  g.levels[i].world == LWORLD_SHAPESHIFT;
    game_step(&g, 0);
    g.title_sel = TITLE_LEVELS;
    press(&g, IN_CROSS);
    int pos[NUM_LWORLDS], at = 0;
    for (int w = 0; w < NUM_LWORLDS; w++)
        pos[w] = -1;
    for (int k = 0; k < 2 * NUM_LWORLDS; k++) {
        if (pos[g.lworld_sel] < 0)
            pos[g.lworld_sel] = at++;
        press(&g, IN_RIGHT);
    }
    int shown = pos[LWORLD_SHAPESHIFT] >= 0;
    snprintf(what, sizeof(what), "SHAPESHIFT has the %d built-in levels whose id starts with 6 (%d) and its card %s",
             n, inside, n ? "shows between COSMIC CORE and THE VAULT" : "is hidden while it has none");
    check(inside == n && shown == (n > 0) &&
          (!shown || (pos[LWORLD_SHAPESHIFT] > pos[5] && pos[LWORLD_SHAPESHIFT] < pos[LWORLD_VAULT])), what);
    for (int k = 0; k < 2 * NUM_LWORLDS && shown && g.lworld_sel != LWORLD_SHAPESHIFT; k++)
        press(&g, IN_RIGHT);
    for (int f = 0; f < 30; f++)
        game_step(&g, 0);
    shot_game(&g, "81a_world_cards");
    if (shown) {
        press(&g, IN_CROSS);
        for (int f = 0; f < 30; f++)
            game_step(&g, 0);
        shot_game(&g, "81b_shapeshift_levels");
        snprintf(what, sizeof(what), "the SHAPESHIFT level list plays PRISM SHIFT (song %d)", g.song);
        check(!g.picking && g.song == SONG_THEME_SHAPESHIFT, what);
    }

    reset_data();
    write_text(PROGRESS_FILE, prog);
    game_init(&g, 191);
    badges_update(&g);
    snprintf(what, sizeof(what), "SHAPE MASTER %s", n ? "is earned by beating every SHAPESHIFT level" : "waits for the SHAPESHIFT levels");
    check(badge_got(&g, B_SHAPESHIFT) == (n > 0), what);
}

// Easter eggs are touch pickups in every mode: a column of egg 1 the whole
// height of the screen is taken by each of the 30 modes running into it with
// nothing pressed (the blaster firing Square all the way), and an egg that
// only the TWIN's upper half touches is his too, taken once.
static void test_int_eggs(void)
{
    static Level lv;
    static char json[4096];
    char what[400], missed[240] = "";
    int took = 0, modes = 0;
    for (int m = 0; m < MODE_COUNT; m++) {
        if (!MODES[m])
            continue;
        modes++;
        int len = snprintf(json, sizeof(json), "{\"mode\":\"%s\",\"eggs\":[{\"n\":1,\"name\":\"TEST\",\"hint\":\"TEST\"}],"
                           "\"objects\":[", MODES[m]->name);
        for (int y = 0; y < 12; y++)
            len += snprintf(json + len, sizeof(json) - (size_t)len, "{\"t\":\"egg\",\"x\":14,\"y\":%d,\"c\":1},", y);
        len += snprintf(json + len, sizeof(json) - (size_t)len, "{\"t\":\"end\",\"x\":40,\"y\":0}]}");
        Run r;
        level_parse(&lv, json, len);
        run_start(&r, &lv, 0, 0);
        int fire = m == MODE_BLASTER ? IN_SQUARE : 0;
        for (int f = 0; f < 600 && r.state == RUN_PLAY && r.b.x < 16 * TILE; f++)
            run_step(&r, f & 8 ? fire : 0);
        if (r.b.eggs & 1)
            took++;
        else
            snprintf(missed + strlen(missed), sizeof(missed) - strlen(missed), " %s", MODES[m]->label);
    }
    snprintf(what, sizeof(what), "every mode takes an egg it runs into: %d of %d%s%s", took, modes, missed[0] ? ", not" : "",
             missed);
    check(took == modes, what);

    int len = snprintf(json, sizeof(json), "{\"mode\":\"twin\",\"eggs\":[{\"n\":1,\"name\":\"TEST\",\"hint\":\"TEST\"}],"
                       "\"objects\":[");
    for (int y = 4; y < 12; y++)
        len += snprintf(json + len, sizeof(json) - (size_t)len, "{\"t\":\"egg\",\"x\":14,\"y\":%d,\"c\":1},", y);
    len += snprintf(json + len, sizeof(json) - (size_t)len, "{\"t\":\"end\",\"x\":40,\"y\":0}]}");
    Run r;
    level_parse(&lv, json, len);
    run_start(&r, &lv, 0, 0);
    int takes = 0;
    float top = 0;
    for (int f = 0; f < 600 && r.state == RUN_PLAY && r.b.x < 16 * TILE; f++) {
        run_step(&r, 0);
        takes += (r.sfx & SFX_EGG) != 0;
        if (r.b.x > 12 * TILE && r.b.x < 15 * TILE)
            top = r.b.mf[0] / TILE;
    }
    snprintf(what, sizeof(what), "an egg only the twin's upper half touches (at %.1f cells, the lower at %.1f) is taken "
             "once (%d)", top, r.b.y / TILE, takes);
    check((r.b.eggs & 1) && takes == 1 && r.b.y < 2 * TILE && r.state == RUN_PLAY, what);
}

static void test_eggs(void)
{
    test_egg_objects();
    test_egg_game();
    test_game_eggs();
    test_egg_rewards();
    test_egg_screens();
    test_egg_looks();
    test_egg_vault();
}

int main(int argc, char **argv)
{
    if (getenv("SIM_COINS")) {
        probe_coins(getenv("SIM_COINS"));
        return 0;
    }
    if (getenv("SIM_EGGS")) {
        probe_eggs(getenv("SIM_EGGS"));
        return 0;
    }
    if (getenv("SIM_PATH")) {
        print_path(getenv("SIM_PATH"));
        return 0;
    }
    if (argc > 1)
        outdir = argv[1];
    snprintf(data_root, sizeof(data_root), "%s/data", outdir);
    reset_data();

    test_parser();
    test_mode_table();
    test_traces();
    test_morph();
    test_shifter();
    test_physics();
    test_modes();
    test_moving();
    test_practice();
    test_scan_speed();
    test_app();
    test_level_write();
    test_editor();
    test_editor_groups();
    test_editor_signs();
    test_editor_upgrades();
    test_remix();
    test_world();
    test_title();
    test_coins();
    test_customize();
    test_icons();
    test_stock();
    test_icon_examples();
    test_rate_verify();
    test_world_themes();
    test_badges();
    test_rewards();
    test_ghost();
    test_hint();
    test_race();
    test_deaths();
    test_synth();
    test_songs();
    test_song_examples();
    test_levels(argc > 2 ? argc - 2 : 0, argv + 2);
    test_transform(argc > 2 ? argc - 2 : 0, argv + 2);
    test_new_modes(argc > 2 ? argc - 2 : 0, argv + 2);

    // Mode pack A (modes 6-15): its test calls go here.
    test_pack_a();

    // Mode pack B (modes 16-25): its test calls go here.
    test_packb();

    // Both packs together (3.2 integration).
    test_int_palette();

    // Easter eggs: their test calls go here.
    test_eggs();

    // Eggs in every mode (3.2 integration).
    test_int_eggs();

    // World 7 SHAPESHIFT (3.2 integration).
    test_shapeshift_world();

    // MODE LAB (3.2 integration).
    test_mode_lab();

    test_endless();

    printf("%s (%d failures)\n", failures ? "SIM FAILED" : "SIM OK", failures);
    return failures ? 1 : 0;
}
