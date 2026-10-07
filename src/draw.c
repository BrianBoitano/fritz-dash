// Fritz Dash - software drawing primitives and the built-in bitmap font.
#include "draw.h"

static uint32_t *g_fb;
static int g_dx, g_dy;
static int v_on, v_mirror, v_flip;
static float v_ax = 1, v_bx, v_ay = 1, v_by;
static int clip0, clip1 = SCREEN_W;
static int tint_on, tint_inv, tint_m[9];
static int filter;

// Same encoding as the OpenOrbis graphics sample (A8R8G8B8, alpha byte 0x80).
#define PIX(c) (0x80000000u | (uint32_t)(c))

void draw_set_target(uint32_t *fb) { g_fb = fb; }
void draw_set_offset(int dx, int dy) { g_dx = dx; g_dy = dy; }

void draw_set_view(float ax, float bx, float ay, float by)
{
    v_on = ax != 1 || bx != 0 || ay != 1 || by != 0;
    v_mirror = ax < 0;
    v_flip = ay < 0;
    v_ax = ax;
    v_bx = bx;
    v_ay = ay;
    v_by = by;
}

void draw_set_clip(int x0, int x1)
{
    clip0 = x0 < 0 ? 0 : x0;
    clip1 = x1 > SCREEN_W ? SCREEN_W : x1;
}

// Hue turning as a colour matrix (8.8 fixed point), the usual luma weights.
void draw_set_tint(float hue, int inv)
{
    float c = fcos(hue), s = fsin(hue);
    float m[9] = { 0.213f + c * 0.787f - s * 0.213f, 0.715f - c * 0.715f - s * 0.715f, 0.072f - c * 0.072f + s * 0.928f,
                   0.213f - c * 0.213f + s * 0.143f, 0.715f + c * 0.285f + s * 0.140f, 0.072f - c * 0.072f - s * 0.283f,
                   0.213f - c * 0.213f - s * 0.787f, 0.715f - c * 0.715f + s * 0.715f, 0.072f + c * 0.928f + s * 0.072f };
    for (int i = 0; i < 9; i++)
        tint_m[i] = (int)(m[i] * 256.0f + (m[i] < 0 ? -0.5f : 0.5f));
    tint_inv = inv < 0 ? 0 : inv > 256 ? 256 : inv;
    tint_on = hue != 0 || tint_inv;
}

static int clamp255(int v)
{
    return v < 0 ? 0 : v > 255 ? 255 : v;
}

void draw_set_filter(int f)
{
    filter = f > 0 && f < FILTER_COUNT ? f : 0;
}

// Each filter maps a colour by its brightness (or pushes its saturation).
static color_t filtered(color_t c)
{
    static const color_t GREENS[4] = { RGB(15, 56, 15), RGB(48, 98, 48), RGB(139, 172, 15), RGB(155, 188, 15) };
    static const color_t HEAT[6] = { RGB(10, 0, 60), RGB(90, 0, 160), RGB(220, 0, 110), RGB(255, 110, 0),
                                     RGB(255, 230, 40), RGB(255, 255, 230) };
    int r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF, l = (r * 54 + g * 183 + b * 19) >> 8;
    switch (filter) {
    case FILTER_CRT:
        return RGB(r + (255 - r) / 10, g + (255 - g) / 10, b + (255 - b) / 10);
    case FILTER_HANDHELD:
        return GREENS[l * 4 / 256];
    case FILTER_SEPIA:
        return RGB(clamp255(l * 290 / 256 + 10), clamp255(l * 236 / 256 + 6), clamp255(l * 170 / 256));
    case FILTER_NEON: {
        int k = 3;
        return RGB(clamp255(l + (r - l) * k / 2 + 12), clamp255(l + (g - l) * k / 2 + 12), clamp255(l + (b - l) * k / 2 + 12));
    }
    case FILTER_THERMAL: {
        int t = l * 5;
        return col_lerp(HEAT[t / 256], HEAT[t / 256 + (t < 5 * 255)], t % 256);
    }
    case FILTER_PASTEL:
        return col_lerp(RGB(clamp255(l + (r - l) * 3 / 4), clamp255(l + (g - l) * 3 / 4), clamp255(l + (b - l) * 3 / 4)),
                        RGB(255, 245, 250), 100);
    }
    return c;
}

static uint32_t pix(color_t c)
{
    if (tint_on) {
        int r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
        int nr = clamp255((tint_m[0] * r + tint_m[1] * g + tint_m[2] * b) >> 8);
        int ng = clamp255((tint_m[3] * r + tint_m[4] * g + tint_m[5] * b) >> 8);
        int nb = clamp255((tint_m[6] * r + tint_m[7] * g + tint_m[8] * b) >> 8);
        c = col_lerp(RGB(nr, ng, nb), RGB(255 - nr, 255 - ng, 255 - nb), tint_inv);
    }
    if (filter)
        c = filtered(c);
    return PIX(c);
}

static int iround(float v)
{
    return v < 0 ? -(int)(0.5f - v) : (int)(v + 0.5f);
}

static int iceil(float v)
{
    int i = (int)v;
    return (v > (float)i) ? i + 1 : i;
}

static void hline(int y, int x0, int x1, uint32_t v)
{
    // Fills [x0, x1) on row y, already clipped by the caller.
    uint32_t *p = g_fb + (long)y * SCREEN_W;
    for (int x = x0; x < x1; x++)
        p[x] = v;
}

void draw_gradient(color_t top, color_t bottom)
{
    if (clip0 >= clip1)
        return;
    for (int y = 0; y < SCREEN_H; y++) {
        int k = v_ay < 0 ? SCREEN_H - 1 - y : y;
        color_t c = col_lerp(top, bottom, (k * 256) / SCREEN_H);
        hline(y, clip0, clip1, pix(c));
    }
}

void draw_rect(int x, int y, int w, int h, color_t c)
{
    x += g_dx;
    y += g_dy;
    if (v_on) {
        int ax = iround(v_ax * (float)x + v_bx), bx = iround(v_ax * (float)(x + w) + v_bx);
        int ay = iround(v_ay * (float)y + v_by), by = iround(v_ay * (float)(y + h) + v_by);
        x = ax < bx ? ax : bx;
        w = ax < bx ? bx - ax : ax - bx;
        y = ay < by ? ay : by;
        h = ay < by ? by - ay : ay - by;
    }
    int x0 = x < clip0 ? clip0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > clip1 ? clip1 : x + w;
    int y1 = y + h > SCREEN_H ? SCREEN_H : y + h;
    if (x0 >= x1 || y0 >= y1)
        return;
    uint32_t v = pix(c);
    for (int yy = y0; yy < y1; yy++)
        hline(yy, x0, x1, v);
}

void draw_frame(int x, int y, int w, int h, int t, color_t c)
{
    draw_rect(x, y, w, t, c);
    draw_rect(x, y + h - t, w, t, c);
    draw_rect(x, y + t, t, h - 2 * t, c);
    draw_rect(x + w - t, y + t, t, h - 2 * t, c);
}

void draw_poly(int n, const float *xs, const float *ys, color_t c)
{
    float px[8], py[8];
    float miny = 1e9f, maxy = -1e9f;
    if (n < 3 || n > 8)
        return;
    for (int i = 0; i < n; i++) {
        px[i] = xs[i] + (float)g_dx;
        py[i] = ys[i] + (float)g_dy;
        if (v_on) {
            px[i] = v_ax * px[i] + v_bx;
            py[i] = v_ay * py[i] + v_by;
        }
        if (py[i] < miny) miny = py[i];
        if (py[i] > maxy) maxy = py[i];
    }
    int y0 = iceil(miny - 0.5f);
    int y1 = iceil(maxy - 0.5f);
    if (y0 < 0) y0 = 0;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    uint32_t v = pix(c);
    for (int y = y0; y < y1; y++) {
        float yc = (float)y + 0.5f;
        float xl = 1e9f, xr = -1e9f;
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            float ya = py[i], yb = py[j];
            if ((ya <= yc && yb > yc) || (yb <= yc && ya > yc)) {
                float x = px[i] + (yc - ya) * (px[j] - px[i]) / (yb - ya);
                if (x < xl) xl = x;
                if (x > xr) xr = x;
            }
        }
        if (xl > xr)
            continue;
        int ix0 = iceil(xl - 0.5f);
        int ix1 = iceil(xr - 0.5f);
        if (ix0 < clip0) ix0 = clip0;
        if (ix1 > clip1) ix1 = clip1;
        if (ix0 < ix1)
            hline(y, ix0, ix1, v);
    }
}

void draw_rot_square(float cx, float cy, float s, float angle, color_t c)
{
    float h = s * 0.5f;
    float cs = fcos(angle) * h, sn = fsin(angle) * h;
    // Corners (+-h, +-h) rotated about the centre.
    float xs[4] = { cx - cs + sn, cx + cs + sn, cx + cs - sn, cx - cs - sn };
    float ys[4] = { cy - sn - cs, cy + sn - cs, cy + sn + cs, cy - sn + cs };
    draw_poly(4, xs, ys, c);
}

void draw_circle(int cx, int cy, int r, color_t c)
{
    // Walk the half width in from r as the row moves away from the centre.
    int hw = r;
    for (int dy = 0; dy <= r; dy++) {
        while (hw > 0 && hw * hw + dy * dy > r * r)
            hw--;
        draw_rect(cx - hw, cy + dy, 2 * hw + 1, 1, c);
        if (dy)
            draw_rect(cx - hw, cy - dy, 2 * hw + 1, 1, c);
    }
}

// ---------------------------------------------------------------------------
// Font: 5x7 glyphs, one byte per row, bit 4 is the leftmost column.
static const uint8_t FONT[128][7] = {
    ['A'] = { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 },
    ['B'] = { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E },
    ['C'] = { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E },
    ['D'] = { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E },
    ['E'] = { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F },
    ['F'] = { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 },
    ['G'] = { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F },
    ['H'] = { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 },
    ['I'] = { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E },
    ['J'] = { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C },
    ['K'] = { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 },
    ['L'] = { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F },
    ['M'] = { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 },
    ['N'] = { 0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11 },
    ['O'] = { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E },
    ['P'] = { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 },
    ['Q'] = { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D },
    ['R'] = { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 },
    ['S'] = { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E },
    ['T'] = { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 },
    ['U'] = { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E },
    ['V'] = { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 },
    ['W'] = { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A },
    ['X'] = { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 },
    ['Y'] = { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 },
    ['Z'] = { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F },
    ['0'] = { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E },
    ['1'] = { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E },
    ['2'] = { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F },
    ['3'] = { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E },
    ['4'] = { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 },
    ['5'] = { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E },
    ['6'] = { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E },
    ['7'] = { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 },
    ['8'] = { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E },
    ['9'] = { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C },
    ['!'] = { 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04 },
    [':'] = { 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00 },
    ['-'] = { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 },
    ['.'] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C },
    ['\''] = { 0x04, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00 },
    ['%'] = { 0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03 },
    ['/'] = { 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10 },
    ['?'] = { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04 },
    [','] = { 0x00, 0x00, 0x00, 0x00, 0x0C, 0x04, 0x08 },
    ['('] = { 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02 },
    [')'] = { 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08 },
    ['<'] = { 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02 },
    ['>'] = { 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08 },
    ['+'] = { 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00 },
    ['_'] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F },
    ['&'] = { 0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D },
    ['='] = { 0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00 },
};

int text_width(const char *s, int scale)
{
    int n = 0;
    while (s[n])
        n++;
    return n > 0 ? (n * 6 - 1) * scale : 0;
}

// Draws every lit run of every glyph row as one rectangle grown by `grow`.
// In a mirrored or upside down view the string is laid out that way too, so
// it still reads the right way.
static void text_layer(int x, int y, int scale, const char *s, int grow, color_t c)
{
    int w = text_width(s, scale), x0 = x;
    for (; *s; s++, x += 6 * scale) {
        unsigned ch = (unsigned char)*s;
        if (ch >= 'a' && ch <= 'z')
            ch -= 32;
        if (ch >= 128)
            continue;
        for (int r = 0; r < 7; r++) {
            uint8_t bits = FONT[ch][r];
            int col = 0;
            while (col < 5) {
                if (!(bits & (0x10 >> col))) {
                    col++;
                    continue;
                }
                int start = col;
                while (col < 5 && (bits & (0x10 >> col)))
                    col++;
                int rx = x + start * scale - grow, rw = (col - start) * scale + 2 * grow;
                int ry = y + r * scale - grow, rh = scale + 2 * grow;
                if (v_mirror)
                    rx = 2 * x0 + w - rx - rw;
                if (v_flip)
                    ry = 2 * y + 7 * scale - ry - rh;
                draw_rect(rx, ry, rw, rh, c);
            }
        }
    }
}

void draw_text(int x, int y, int scale, const char *s, color_t c)
{
    text_layer(x, y, scale, s, 0, c);
}

void draw_text_glow(int x, int y, int scale, const char *s, color_t core, color_t glow)
{
    int g1 = scale * 3 / 5 + 2;
    int g2 = scale / 4 + 1;
    text_layer(x, y, scale, s, g1, col_scale(glow, 3, 10));
    text_layer(x, y, scale, s, g2, glow);
    text_layer(x, y, scale, s, 0, core);
}

void draw_text_glow_centered(int y, int scale, const char *s, color_t core, color_t glow)
{
    draw_text_glow((SCREEN_W - text_width(s, scale)) / 2, y, scale, s, core, glow);
}

void ui_panel(int x, int y, int w, int h, color_t edge)
{
    draw_rect(x - 14, y - 14, w + 28, h + 28, col_scale(edge, 1, 4));
    draw_rect(x - 6, y - 6, w + 12, h + 12, edge);
    draw_rect(x, y, w, h, RGB(10, 6, 26));
}

void ui_star(float cx, float cy, float r, color_t c)
{
    float ox[5], oy[5], ix[5], iy[5];
    for (int i = 0; i < 5; i++) {
        float a = -1.5707963f + (float)i * 1.2566371f;
        ox[i] = cx + fcos(a) * r;
        oy[i] = cy + fsin(a) * r;
        ix[i] = cx + fcos(a + 0.6283185f) * r * 0.42f;
        iy[i] = cy + fsin(a + 0.6283185f) * r * 0.42f;
    }
    draw_poly(5, ix, iy, c);
    for (int i = 0; i < 5; i++) {
        int j = (i + 4) % 5;
        float tx[3] = { ox[i], ix[i], ix[j] }, ty[3] = { oy[i], iy[i], iy[j] };
        draw_poly(3, tx, ty, c);
    }
}

int ui_fit_scale(const char *s, int w, int max)
{
    int scale = max;
    while (scale > 3 && text_width(s, scale) > w)
        scale--;
    return scale;
}

// ---------------------------------------------------------------------------
color_t col_scale(color_t c, int num, int den)
{
    int r = ((c >> 16) & 0xFF) * num / den;
    int g = ((c >> 8) & 0xFF) * num / den;
    int b = (c & 0xFF) * num / den;
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return RGB(r, g, b);
}

color_t col_lerp(color_t a, color_t b, int t)
{
    if (t < 0) t = 0;
    if (t > 256) t = 256;
    int ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    int br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    return RGB(ar + (br - ar) * t / 256, ag + (bg - ag) * t / 256, ab + (bb - ab) * t / 256);
}

// Small sine so we do not depend on libm: range-reduce to [-pi, pi], then a
// parabola with one refinement step (max error about 0.001).
float fsin(float x)
{
    const float PI = 3.14159265f, TWO_PI = 6.28318531f;
    int k = (int)(x / TWO_PI);
    x -= (float)k * TWO_PI;
    if (x > PI) x -= TWO_PI;
    if (x < -PI) x += TWO_PI;
    float y = 1.27323954f * x - 0.40528473f * x * (x < 0 ? -x : x);
    return 0.225f * (y * (y < 0 ? -y : y) - y) + y;
}

float fcos(float x)
{
    return fsin(x + 1.57079633f);
}
