// Fritz Dash - ghost files. A ghost file is text:
//   fritzdash-ghost 1
//   <level hash, hex> <best %> <frames>
//   <frames released> <frames held> <frames released> ...
//   square <frames released> <frames held> ...      (only when Square was held)
// Older files have no square line, and older builds stop reading before it.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "ghost.h"
#include "platform.h"

static char text[160 * 1024];

int ghost_bit(const Ghost *gh, int i)
{
    return i >= 0 && i < gh->frames && (gh->bits[i >> 3] >> (i & 7) & 1);
}

int ghost_square(const Ghost *gh, int i)
{
    return i >= 0 && i < gh->frames && (gh->square[i >> 3] >> (i & 7) & 1);
}

int ghost_push(Ghost *gh, int x_held)
{
    if (gh->frames >= GHOST_MAX_FRAMES)
        return 0;
    int i = gh->frames++;
    if (x_held)
        gh->bits[i >> 3] |= (uint8_t)(1 << (i & 7));
    else
        gh->bits[i >> 3] &= (uint8_t)~(1 << (i & 7));
    gh->square[i >> 3] &= (uint8_t)~(1 << (i & 7));
    return 1;
}

void ghost_mark_square(Ghost *gh)
{
    int i = gh->frames - 1;
    if (i >= 0)
        gh->square[i >> 3] |= (uint8_t)(1 << (i & 7));
}

// Run lengths of one button's frames, released first.
static int write_runs(const Ghost *gh, int (*bit)(const Ghost *, int), int n)
{
    int run = 0, cur = 0;
    for (int i = 0; i <= gh->frames && n < (int)sizeof(text) - 16; i++) {
        int b = i < gh->frames ? bit(gh, i) : !cur;
        if (b == cur) {
            run++;
            continue;
        }
        n += snprintf(text + n, sizeof(text) - (size_t)n, "%d ", run);
        cur = b;
        run = 1;
    }
    text[n++] = '\n';
    return n;
}

void ghost_path(const char *key, const char *ext, char *out, size_t n)
{
    char name[96];
    snprintf(name, sizeof(name), "%s", key);
    for (char *p = name; *p; p++)
        if (*p == ':' || *p == '/' || *p == ' ' || *p == '\\')
            *p = '_';
    snprintf(out, n, "%s/%s.%s", GHOSTS_DIR, name, ext);
}

int ghost_save(const Ghost *gh, const char *key)
{
    char path[200];
    ghost_path(key, "ghost", path, sizeof(path));
    int n = snprintf(text, sizeof(text), "fritzdash-ghost 1\n%08x %d %d\n", (unsigned)gh->hash, gh->pct, gh->frames);
    n = write_runs(gh, ghost_bit, n);
    int sq = 0;
    for (int i = 0; i < (gh->frames + 7) / 8; i++)
        sq |= gh->square[i];
    if (sq)
        n = write_runs(gh, ghost_square, n + snprintf(text + n, sizeof(text) - (size_t)n, "square "));
    return plat_write_file(path, text, n);
}

int deaths_load(int (*xy)[2], const char *key)
{
    char path[200];
    ghost_path(key, "deaths", path, sizeof(path));
    int n = plat_read_file(path, text, (int)sizeof(text) - 1), count = 0;
    text[n > 0 ? n : 0] = 0;
    for (char *p = text; *p && count < MAX_DEATHS;) {
        char *e;
        long x = strtol(p, &e, 10);
        if (e == p)
            break;
        long y = strtol(e, &p, 10);
        xy[count][0] = (int)x;
        xy[count][1] = (int)y;
        count++;
    }
    return count;
}

void deaths_save(const int (*xy)[2], int n, const char *key)
{
    char path[200];
    ghost_path(key, "deaths", path, sizeof(path));
    int len = 0;
    for (int i = 0; i < n; i++)
        len += snprintf(text + len, sizeof(text) - (size_t)len, "%d %d\n", xy[i][0], xy[i][1]);
    plat_write_file(path, text, len);
}

int ghost_load(Ghost *gh, const char *key, uint32_t hash)
{
    char path[200];
    ghost_path(key, "ghost", path, sizeof(path));
    memset(gh, 0, sizeof(*gh));
    int n = plat_read_file(path, text, (int)sizeof(text) - 1);
    if (n <= 0)
        return 0;
    text[n] = 0;
    unsigned h = 0;
    int pct = 0, frames = 0;
    char *p = strchr(text, '\n');
    if (strncmp(text, "fritzdash-ghost ", 16) || !p || sscanf(p + 1, "%x %d %d", &h, &pct, &frames) != 3 ||
        h != hash || frames <= 0 || frames > GHOST_MAX_FRAMES)
        return 0;
    p = strchr(p + 1, '\n');
    for (int cur = 0; p && *p && gh->frames < frames; cur = !cur) {
        long len = strtol(p, &p, 10);
        if (len <= 0 && cur == 1)
            break;
        for (long k = 0; k < len && gh->frames < frames; k++)
            ghost_push(gh, cur);
    }
    if (gh->frames != frames) {
        memset(gh, 0, sizeof(*gh));
        return 0;
    }
    char *q = strstr(text, "\nsquare ");
    if (q)
        q += 8;
    for (int cur = 0, i = 0; q && *q && i < frames; cur = !cur) {
        long len = strtol(q, &q, 10);
        if (len <= 0 && cur == 1)
            break;
        for (long k = 0; k < len && i < frames; k++, i++)
            if (cur)
                gh->square[i >> 3] |= (uint8_t)(1 << (i & 7));
    }
    gh->hash = h;
    gh->pct = pct;
    return 1;
}
