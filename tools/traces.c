// Fritz Dash - solver trace digests. For a level: the fine solution, the six
// 100 ms grid solutions and the coin solutions the simulator looks for, each
// hashed with the body it passes through frame by frame. traces_base.txt holds
// them for the built-in levels from before the 3.2 mode framework; the
// simulator checks that every unchanged level still plays exactly the same.
//   cc -DTRACES_MAIN tools/traces.c tools/solver.c <game sources> builtin_levels.c
//   -> prints "<id> <level hash> <digests...>" for every built-in level.
#include <stdio.h>
#include <string.h>

#include "../src/game.h"
#include "solver.h"

static uint64_t mix(uint64_t h, uint64_t v)
{
    return (h ^ v) * 1099511628211ull;
}

static uint64_t digest_solution(const Level *lv, int rc)
{
    uint64_t h = mix(1469598103934665603ull, (uint64_t)(rc + 2));
    if (rc != 1)
        return h;
    Run r;
    run_start(&r, lv, 0, 0);
    for (int i = 0; i < ntrace; i++) {
        h = mix(h, trace[i]);
        int ev = run_step(&r, trace[i]);
        const Body *b = &r.b;
        uint32_t f[5];
        memcpy(&f[0], &b->x, 4);
        memcpy(&f[1], &b->y, 4);
        memcpy(&f[2], &b->vy, 4);
        f[3] = (uint32_t)(b->mode | b->speed << 8 | (b->grav > 0) << 12 | b->mini << 13 | b->on_ground << 14 |
                          b->coins << 16 | r.state << 24 | ev << 28);
        f[4] = (uint32_t)(r.ntouch | r.buffered << 8 | r.held_lock << 9 | r.jumps << 12);
        for (int k = 0; k < 5; k++)
            h = mix(h, f[k]);
    }
    return h;
}

// The same searches, in the same order, as solve_level in tools/sim.c.
int trace_digest(const Level *lv, char *out, int max)
{
    int n = snprintf(out, (size_t)max, "%08x", (unsigned)level_hash(lv));
    int rc = solve(lv, 2, 0, 400000000L);
    n += snprintf(out + n, (size_t)(max - n), " %016llx", (unsigned long long)digest_solution(lv, rc));
    if (rc != 1)
        return n;
    for (int p = 0; p < 6; p++) {
        rc = solve(lv, 6, p ? p : 6, 200000000L);
        n += snprintf(out + n, (size_t)(max - n), " %016llx", (unsigned long long)digest_solution(lv, rc));
    }
    for (int k = 0; k <= lv->ncoins && lv->ncoins; k++) {
        want_coins(lv, k < lv->ncoins ? 1 << k : (1 << lv->ncoins) - 1);
        rc = solve(lv, 2, 0, 400000000L);
        n += snprintf(out + n, (size_t)(max - n), " %016llx", (unsigned long long)digest_solution(lv, rc));
    }
    want_coins(lv, 0);
    return n;
}

#ifdef TRACES_MAIN
int plat_read_file(const char *path, char *buf, int max) { (void)path; (void)buf; (void)max; return -1; }
int plat_write_file(const char *path, const char *buf, int len) { (void)path; (void)buf; (void)len; return -1; }
int plat_remove_file(const char *path) { (void)path; return -1; }
int plat_list_dir(const char *dir, void (*fn)(const char *name, void *ctx), void *ctx)
{
    (void)dir; (void)fn; (void)ctx;
    return -1;
}

int main(void)
{
    static Level lv;
    static char line[4096];
    for (int i = 0; i < BUILTIN_COUNT; i++) {
        level_parse(&lv, BUILTIN_JSON[i], (int)strlen(BUILTIN_JSON[i]));
        trace_digest(&lv, line, sizeof(line));
        printf("%s %s\n", BUILTIN_IDS[i], line);
        fflush(stdout);
    }
    return 0;
}
#endif
