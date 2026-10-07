// Fritz Dash - the level editor: 20 save slots, a grid cursor, an object
// palette, rotate, undo, copy and paste, colours, song, name, and playtest.
#ifndef FRITZ_EDITOR_H
#define FRITZ_EDITOR_H

#include "keyboard.h"
#include "level.h"
#include "rate.h"

#define EDIT_SLOTS 20
#define MAX_CLIP   1024

enum { ED_SLOTS, ED_EDIT, ED_MENU, ED_NAME, ED_GROUPS, ED_MAP, ED_TEMPLATES, ED_OBJSET };
enum { EDR_NONE, EDR_EXIT, EDR_PLAYTEST, EDR_VERIFY };

typedef struct {
    int screen;
    int slot, slot_sel, slot_delete;    // slot_delete: 1 + the slot Square was pressed on once
    char slot_names[EDIT_SLOTS][32];
    int slot_used[EDIT_SLOTS];
    int cx, cy;                         // cursor cell
    float cam_x, cam_y;
    int brush, brush_rot, brush_color, brush_sign;
    int brush_group;                    // group new objects go in, 0 for none
    int move_group, move_dx, move_dy, move_time;   // what a new move trigger does
    int group_sel;
    int anchor_x, anchor_y, selecting, has_sel;
    int sel_x0, sel_y0, sel_x1, sel_y1;
    int nclip;
    LevelObj clip[MAX_CLIP];
    int undo_count;
    int hold_dir, hold_timer;           // cursor key repeat
    int menu_sel;
    int palette;                        // level colour preset, or -1 for colours from a file
    Keyboard kb;                        // naming the level
    int test_x;                         // playtest start column; 0 is the level start
    char msg[48];
    int msg_frames;
    int dirty;
    int autosave;                       // frames since the last automatic save
    Rate rate;                          // the suggested difficulty of a saved slot (it carries on after leaving)
    int rate_slot, rate_saved;
    int map_x;                          // level map: the first column of the box
    int tmpl_sel;                       // templates: the one picked
    int sel_group;                      // SELECTION TO GROUP: the group (0 takes them out)
    int edit_trigger;                   // 1 + index of the move trigger the groups panel is changing, or 0
    int music_on;                       // the level's song is playing from the cursor
    int l2_had_sel;                     // there was a selection before L2 went down
    int last_saved;                     // 1 + the slot saved last, or 0
    int obj_edit;                       // settings panel: 1 + the object it changes, -1 for the brush's
    int obj_row;
    LevelObj tmpl;                      // the settings new 3.2 objects of its type get
} Editor;

// Opens the slot picker. lv is used as scratch to read the slot names.
void editor_open(Editor *e, Level *lv);
// One frame. lv is the level being edited. Returns an EDR_ value; for
// EDR_PLAYTEST the level is saved and e->test_x says where to start.
int editor_step(Editor *e, Level *lv, int input, int edges);
// Back from a playtest.
void editor_resume(Editor *e);
// After editor_open: copies lv (any level) into the first empty slot, named
// "<name> REMIX", and opens it for editing. Returns the slot, or -1 if every
// slot is taken.
int editor_remix(Editor *e, Level *lv);
// The level was beaten from the start: mark it verified and save it.
void editor_verified(Editor *e, Level *lv);
// Rates the last saved level a slice at a time (every frame, anywhere in the
// game); lv is the level being edited, or NULL when the editor is not open.
void editor_tick(Editor *e, Level *lv);
// Ready-made pieces: how many, and putting piece k with its bottom-left
// corner at cell (x, y) (-1 when the level is full).
int editor_templates(void);
int editor_template_put(Level *lv, int k, int x, int y);
// Frames the player takes from the start to column x (music from here).
int editor_frames_to(const Level *lv, int x);
void editor_draw(const Editor *e, const Level *lv, int frame);
// The object type the palette is on.
int editor_brush_type(const Editor *e);

#endif
