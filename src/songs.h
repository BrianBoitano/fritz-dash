// Fritz Dash - his songs: the song maker (a step sequencer with a lead, a
// sparkle, a bass and drums), ten song slots saved as SONGS_DIR/songNN.json,
// and the songs handed to the synth so his levels can play them.
#ifndef FRITZ_SONGS_H
#define FRITZ_SONGS_H

#include "keyboard.h"
#include "synth.h"

#define SONGS_DIR "/data/fritzdash/songs"
#define SONG_NAME_MAX 16
#define SONG_UNDO 24
#define NUM_SONG_EXAMPLES 4     // read-only songs to learn from, after his ten slots

enum { SM_SLOTS, SM_EDIT, SM_MENU, SM_NAME };

typedef struct {
    int screen;
    int slot, slot_sel, slot_delete;    // slot_delete: 1 + the slot Square was pressed on once;
                                        // slot_sel USER_SONGS + e is example e
    int example;                        // 1 + the example being looked at (read-only), or 0
    UserSong song;
    char name[SONG_NAME_MAX + 1];
    int track, step, row;               // cursor: instrument, step 0..31, row in its grid (0 = top)
    int playing, play_t;                // the loop is playing; frames since it started
    int menu_sel, clear_armed;
    int dirty, autosave;
    int hold_dir, hold_timer;
    UserSong undo[SONG_UNDO];
    int undo_count, undo_head;
    Keyboard kb;
    char msg[48];
    int msg_frames;
} SongMaker;

// Reads every song file and hands the songs to the synth (start-up).
void songs_load_all(void);
int songs_used(int n);
const char *songs_name(int n);
int songs_count(void);
// A song file: returns 1 if it is one.
int song_parse(UserSong *u, char *name, int name_max, const char *text, int len);
int song_write(const UserSong *u, const char *name, char *buf, int max);
// Example e (0..NUM_SONG_EXAMPLES-1) and its one-line caption.
int songs_example(int e, UserSong *u, char *name, int name_max);
const char *songs_example_caption(int e);
// The step the loop is on after `frames` frames of playing.
int song_play_step(const UserSong *u, int frames);

void songmaker_open(SongMaker *s);
// One frame; returns 1 when he goes back to the title.
int songmaker_step(SongMaker *s, int input, int edges);
void songmaker_draw(const SongMaker *s, int frame);

#endif
