// Fritz Dash - his pixel icons: six 8x8 pictures in four colours (the cube's
// own colour, colour 2, white and black), drawn in the ICON MAKER and worn as
// a face (MY ICON 1..6 in Customize). Saved in ICONS_FILE.
#ifndef FRITZ_ICONS_H
#define FRITZ_ICONS_H

#include <stdint.h>

#define ICONS_FILE "/data/fritzdash/icons.txt"
#define NUM_ICONS  6
#define ICON_N     8
#define ICON_UNDO  16
#define NUM_ICON_EXAMPLES 5 // read-only icons to learn from; IconMaker.icon NUM_ICONS + e is example e
#define FACE_ICON0 10       // face number of MY ICON 1 (Customize); faces after MY ICON 6 are drawn ones again
#define FACE_IS_ICON(f) ((f) >= FACE_ICON0 && (f) < FACE_ICON0 + NUM_ICONS)

enum { PX_CUBE, PX_COL2, PX_WHITE, PX_BLACK, PX_COLOURS };
enum { ICR_NONE, ICR_BACK, ICR_WEAR };

typedef struct {
    int icon, cx, cy, pen;
    int clear_armed;
    int x_lock;             // X is ignored until released (it just copied an example)
    int hold_dir, hold_timer;
    uint8_t undo[ICON_UNDO][ICON_N * ICON_N];
    int undo_count, undo_head;
    char msg[48];
    int msg_frames;
} IconMaker;

void icons_load(void);
int icons_save(void);
int icon_empty(int k);
// The pixels of icon k, row by row from the top (PX_ values).
const uint8_t *icon_pixels(int k);
// Example e's pixels, short name and one-line caption.
const uint8_t *icon_example(int e);
const char *icon_example_name(int e);
const char *icon_example_caption(int e);

void iconmaker_open(IconMaker *m, int icon);
// One frame; returns an ICR_ value (ICR_WEAR: wear m->icon as the face).
int iconmaker_step(IconMaker *m, int input, int edges);
struct Look;
void iconmaker_draw(const IconMaker *m, const struct Look *look, int frame);

#endif
