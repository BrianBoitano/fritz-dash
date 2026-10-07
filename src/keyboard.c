// Fritz Dash - on-screen keyboard.
#include <stdio.h>
#include <string.h>

#include "input.h"
#include "keyboard.h"

static const char *const KEY_ROWS[4] = { "ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ0123", "456789-!?." };
static const char *const KEY_ACTIONS[3] = { "SPACE", "DELETE", "DONE" };

void kb_open(Keyboard *k, const char *text, int max)
{
    k->max = max < KB_MAX ? max : KB_MAX;
    snprintf(k->text, sizeof(k->text), "%s", text);
    k->text[k->max] = 0;
    k->row = k->col = 0;
}

int kb_step(Keyboard *k, int edges)
{
    int n = (int)strlen(k->text), done = 0;
    if (edges & IN_CIRCLE)
        return KB_CANCEL;
    if (edges & IN_LEFT)
        k->col = (k->col + (k->row == 4 ? 2 : 9)) % (k->row == 4 ? 3 : 10);
    if (edges & IN_RIGHT)
        k->col = (k->col + 1) % (k->row == 4 ? 3 : 10);
    if (edges & IN_DOWN) {
        if (k->row == 3)
            k->col = k->col * 3 / 10;
        else if (k->row == 4)
            k->col = k->col * 10 / 3 + 1;
        k->row = (k->row + 1) % 5;
    }
    if (edges & IN_UP) {
        if (k->row == 4)
            k->col = k->col * 10 / 3 + 1;
        else if (k->row == 0)
            k->col = k->col * 3 / 10;
        k->row = (k->row + 4) % 5;
    }
    if ((edges & IN_SQUARE) && n > 0)
        k->text[--n] = 0;
    if (edges & IN_START)
        done = 1;
    if (edges & IN_CROSS) {
        if (k->row < 4) {
            if (n < k->max) {
                k->text[n] = KEY_ROWS[k->row][k->col];
                k->text[n + 1] = 0;
            }
        } else if (k->col == 0 && n < k->max && n > 0) {
            k->text[n] = ' ';
            k->text[n + 1] = 0;
        } else if (k->col == 1 && n > 0) {
            k->text[n - 1] = 0;
        } else if (k->col == 2) {
            done = 1;
        }
    }
    if (!done)
        return KB_TYPING;
    n = (int)strlen(k->text);
    while (n > 0 && k->text[n - 1] == ' ')
        k->text[--n] = 0;
    return KB_DONE;
}

void kb_draw(const Keyboard *k, const char *title, color_t edge, int frame)
{
    int w = 1300, h = 760, x = (SCREEN_W - w) / 2, y = 160;
    ui_panel(x, y, w, h, edge);
    draw_text_glow_centered(y + 30, 6, title, RGB(255, 255, 255), edge);
    char shown[40];
    snprintf(shown, sizeof(shown), "%s%s", k->text, (frame / 15) % 2 ? "" : "_");
    draw_rect(x + 100, y + 110, w - 200, 90, RGB(4, 2, 12));
    draw_text_glow(x + 130, y + 128, ui_fit_scale(shown, w - 260, 8), shown, RGB(255, 235, 40), RGB(200, 120, 0));
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 10; c++) {
            int kx = x + 100 + c * 110, ky = y + 240 + r * 100;
            char key[2] = { KEY_ROWS[r][c], 0 };
            int on = k->row == r && k->col == c;
            draw_rect(kx, ky, 96, 84, on ? RGB(90, 255, 40) : RGB(30, 24, 60));
            draw_text(kx + 33, ky + 21, 6, key, on ? RGB(0, 0, 0) : RGB(255, 255, 255));
        }
    for (int c = 0; c < 3; c++) {
        int kx = x + 100 + c * 370, ky = y + 640;
        int on = k->row == 4 && k->col == c;
        draw_rect(kx, ky, 356, 84, on ? RGB(90, 255, 40) : RGB(30, 24, 60));
        draw_text(kx + (356 - text_width(KEY_ACTIONS[c], 5)) / 2, ky + 24, 5, KEY_ACTIONS[c],
                  on ? RGB(0, 0, 0) : RGB(255, 255, 255));
    }
    draw_text_glow_centered(y + h - 30, 3, "D-PAD MOVE   X TYPE   SQUARE DELETE   OPTIONS DONE   CIRCLE CANCEL",
                            RGB(220, 220, 255), RGB(90, 90, 200));
}
