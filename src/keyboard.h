// Fritz Dash - the on-screen keyboard for names and the title tagline: a
// letter grid moved with the D-pad, X types, Square deletes, Options or DONE
// finishes, Circle leaves without a change.
#ifndef FRITZ_KEYBOARD_H
#define FRITZ_KEYBOARD_H

#include "draw.h"

#define KB_MAX 32

enum { KB_TYPING, KB_DONE, KB_CANCEL };

typedef struct {
    char text[KB_MAX + 1];
    int max;                // characters allowed
    int row, col;
} Keyboard;

void kb_open(Keyboard *k, const char *text, int max);
// One frame of button presses. KB_DONE leaves the text trimmed of spaces at
// the end (it may be empty: the caller keeps what it had).
int kb_step(Keyboard *k, int edges);
void kb_draw(const Keyboard *k, const char *title, color_t edge, int frame);

#endif
