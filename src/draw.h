// Fritz Dash - software drawing into a 1920x1080 32-bit framebuffer.
// Everything here is write-only: the PS4 framebuffer is write-combined memory,
// so we never read pixels back (no blending). "Glow" is faked with layers.
#ifndef FRITZ_DRAW_H
#define FRITZ_DRAW_H

#include <stdint.h>

#define SCREEN_W 1920
#define SCREEN_H 1080

// Colours are plain 0xRRGGBB.
typedef uint32_t color_t;

#define RGB(r, g, b) ((color_t)(((r) << 16) | ((g) << 8) | (b)))

void draw_set_target(uint32_t *fb);
void draw_set_offset(int dx, int dy);
// The level transformations' view: after the offset, screen x becomes
// ax * x + bx and y becomes ay * y + by (a negative scale mirrors; text
// stays readable). draw_set_view(1, 0, 1, 0) turns it off.
void draw_set_view(float ax, float bx, float ay, float by);
// Only columns x0 .. x1 - 1 are drawn (a sweep across the screen).
void draw_set_clip(int x0, int x1);
// Colour effects on everything drawn: hue turned by angle (radians) and
// colours inverted by inv (0..256). draw_set_tint(0, 0) turns them off.
void draw_set_tint(float hue, int inv);

// A screen filter (easter egg rewards) on every colour drawn after the tint;
// FILTER_NONE turns it off. FILTER_CRT only brightens: its lines are drawn on top.
enum { FILTER_NONE, FILTER_CRT, FILTER_HANDHELD, FILTER_SEPIA, FILTER_NEON, FILTER_THERMAL, FILTER_PASTEL,
       FILTER_COUNT };
void draw_set_filter(int filter);

// Vertical gradient over the whole screen (ignores the shake offset).
void draw_gradient(color_t top, color_t bottom);
void draw_rect(int x, int y, int w, int h, color_t c);
void draw_frame(int x, int y, int w, int h, int t, color_t c);
// Convex polygon, up to 8 points.
void draw_poly(int n, const float *xs, const float *ys, color_t c);
// Square of side s centred on (cx, cy), rotated by angle (radians).
void draw_rot_square(float cx, float cy, float s, float angle, color_t c);
void draw_circle(int cx, int cy, int r, color_t c);

// Built-in 5x7 bitmap font. Each glyph cell is 6x8 "font pixels".
int  text_width(const char *s, int scale);
void draw_text(int x, int y, int scale, const char *s, color_t c);
void draw_text_glow(int x, int y, int scale, const char *s, color_t core, color_t glow);
void draw_text_glow_centered(int y, int scale, const char *s, color_t core, color_t glow);

// Neon-edged dark panel; biggest text scale up to max that fits width w.
void ui_panel(int x, int y, int w, int h, color_t edge);
int ui_fit_scale(const char *s, int w, int max);
// Five-pointed star of radius r.
void ui_star(float cx, float cy, float r, color_t c);

color_t col_scale(color_t c, int num, int den);
color_t col_lerp(color_t a, color_t b, int t256);

float fsin(float x);
float fcos(float x);

#endif
