// Fritz Dash - the song maker and his saved songs.
#include <stdio.h>
#include <string.h>

#include "draw.h"
#include "input.h"
#include "platform.h"
#include "songs.h"

#define COL_WHITE  RGB(255, 255, 255)
#define COL_YELLOW RGB(255, 235, 40)
#define COL_CYAN   RGB(0, 230, 255)
#define COL_GREEN  RGB(90, 255, 40)
#define COL_PINK   RGB(255, 50, 200)
#define COL_DIM    RGB(150, 150, 190)
#define COL_PANEL  RGB(10, 6, 26)

#define GRID_X     150
#define CELL_W     52
#define TUNE_Y     250
#define TUNE_H     54
#define DRUM_Y     262
#define DRUM_H     136

enum { SMM_RESUME, SMM_PLAY, SMM_TEMPO, SMM_SCALE, SMM_SOUND, SMM_NAME, SMM_CLEAR, SMM_SAVE, SMM_SAVE_EXIT, SMM_COUNT };

static const char *const TRACK_NAMES[TRACK_COUNT] = { "LEAD", "SPARKLE", "BASS", "DRUMS" };
static const color_t TRACK_COLS[TRACK_COUNT] = { RGB(255, 50, 200), RGB(0, 230, 255), RGB(255, 150, 30), RGB(90, 255, 40) };
static const color_t NOTE_COLS[9] = { 0, RGB(255, 60, 60), RGB(255, 150, 30), RGB(255, 235, 40), RGB(90, 255, 40),
                                      RGB(0, 230, 255), RGB(60, 120, 255), RGB(170, 80, 255), RGB(255, 80, 200) };
static const char *const DRUM_ROWS[3] = { "HAT", "SNARE", "KICK" };
static const int DRUM_BITS[3] = { DRUM_HAT, DRUM_SNARE, DRUM_KICK };
static const char DRUM_CHARS[9] = ".KSZHXYA";

// Songs to learn from, written like his song files. They only play and copy.
static const struct { const char *caption, *text; } EXAMPLES[NUM_SONG_EXAMPLES] = {
    { "A BEAT: KICK, SNARE AND HAT MAKE A RHYTHM",
      "{\"format\":\"fritzdash-song\",\"name\":\"A BEAT\",\"bpm\":110,\"scale\":0,\"sound\":1,"
      "\"drums\":\"XHYHXHYHXHYHXXYHXHYHXHYHXHYHYYYA\"}" },
    { "A MELODY: A TUNE ON THE SCALE, WITH RESTS AND A REPEAT",
      "{\"format\":\"fritzdash-song\",\"name\":\"A MELODY\",\"bpm\":120,\"scale\":0,\"sound\":1,"
      "\"lead\":\"1.3.5.3.443.2...1.3.5.3.54321...\"}" },
    { "A BASS LINE: LOW NOTES AND DRUMS MAKE A GROOVE",
      "{\"format\":\"fritzdash-song\",\"name\":\"A BASS LINE\",\"bpm\":100,\"scale\":1,\"sound\":0,"
      "\"bass\":\"1.18.15.6.68.65.4.48.45.5.58.57.\",\"drums\":\"X.Y.XXY.X.Y.XXY.X.Y.XXY.X.Y.XYYA\"}" },
    { "A FULL SONG: LEAD, SPARKLE, BASS AND DRUMS TOGETHER",
      "{\"format\":\"fritzdash-song\",\"name\":\"A FULL SONG\",\"bpm\":132,\"scale\":0,\"sound\":2,"
      "\"lead\":\"3.5.8.5.7...5...6.5.3.5.4.3.2...\",\"sparkle\":\"..5...5...7...7...6...6...4...4.\","
      "\"bass\":\"1.1.1.1.5.5.5.5.6.6.6.6.4.4.4.4.\",\"drums\":\"XHYHXHYHXHYHXHYHXHYHXHYHXHYHYYYA\"}" },
};

static UserSong saved[USER_SONGS];
static char saved_names[USER_SONGS][SONG_NAME_MAX + 1];
static int saved_used[USER_SONGS];
static char text_buf[8192];

static void song_path(int n, char *out, size_t size)
{
    snprintf(out, size, "%s/song%02d.json", SONGS_DIR, n + 1);
}

static void default_song(UserSong *u)
{
    memset(u, 0, sizeof(*u));
    u->bpm = 120;
    u->scale = SCALE_HAPPY;
    u->lead_wave = 1;
    for (int k = 0; k < USER_STEPS; k++)
        u->notes[TRACK_DRUMS][k] = (uint8_t)((k % 4 == 0 ? DRUM_KICK : k % 4 == 2 ? DRUM_SNARE : 0) | (k % 2 ? DRUM_HAT : 0));
}

// Our own files only, so a key search is enough: the value after "key":.
static const char *find_key(const char *t, int len, const char *key)
{
    char want[24];
    int k = snprintf(want, sizeof(want), "\"%s\"", key);
    for (int i = 0; i + k <= len; i++)
        if (!memcmp(t + i, want, (size_t)k)) {
            const char *p = t + i + k;
            while (p < t + len && (*p == ' ' || *p == ':'))
                p++;
            return p < t + len ? p : NULL;
        }
    return NULL;
}

static int key_int(const char *t, int len, const char *key, int def, int lo, int hi)
{
    const char *p = find_key(t, len, key);
    if (!p || !((*p >= '0' && *p <= '9') || *p == '-'))
        return def;
    int neg = *p == '-', v = 0;
    for (p += neg; p < t + len && *p >= '0' && *p <= '9'; p++)
        v = v * 10 + (*p - '0');
    v = neg ? -v : v;
    return v < lo ? lo : v > hi ? hi : v;
}

static int key_str(const char *t, int len, const char *key, char *out, int max)
{
    const char *p = find_key(t, len, key);
    int n = 0;
    if (!p || *p != '"')
        return 0;
    for (p++; p < t + len && *p != '"' && n < max - 1; p++) {
        if (*p == '\\' && p + 1 < t + len)
            p++;
        out[n++] = *p;
    }
    out[n] = 0;
    return 1;
}

int song_parse(UserSong *u, char *name, int name_max, const char *text, int len)
{
    char row[USER_STEPS + 8];
    if (!find_key(text, len, "format") || !strstr(text, "fritzdash-song"))
        return 0;
    memset(u, 0, sizeof(*u));
    u->bpm = key_int(text, len, "bpm", 120, 40, 240);
    u->scale = key_int(text, len, "scale", 0, 0, SCALE_COUNT - 1);
    u->lead_wave = key_int(text, len, "sound", 1, 0, 2);
    if (!key_str(text, len, "name", name, name_max))
        snprintf(name, (size_t)name_max, "MY SONG");
    static const char *const KEYS[TRACK_COUNT] = { "lead", "sparkle", "bass", "drums" };
    for (int t = 0; t < TRACK_COUNT; t++) {
        if (!key_str(text, len, KEYS[t], row, sizeof(row)))
            continue;
        for (int k = 0; k < USER_STEPS && row[k]; k++) {
            const char *d = strchr(DRUM_CHARS, row[k]);
            if (t == TRACK_DRUMS)
                u->notes[t][k] = (uint8_t)(d && row[k] ? d - DRUM_CHARS : 0);
            else
                u->notes[t][k] = (uint8_t)(row[k] >= '1' && row[k] <= '8' ? row[k] - '0' : 0);
        }
    }
    return 1;
}

int song_write(const UserSong *u, const char *name, char *buf, int max)
{
    char rows[TRACK_COUNT][USER_STEPS + 1], safe[SONG_NAME_MAX + 1];
    for (int t = 0; t < TRACK_COUNT; t++) {
        for (int k = 0; k < USER_STEPS; k++) {
            int v = u->notes[t][k];
            rows[t][k] = t == TRACK_DRUMS ? DRUM_CHARS[v & 7] : v >= 1 && v <= 8 ? (char)('0' + v) : '.';
        }
        rows[t][USER_STEPS] = 0;
    }
    int n = 0;
    for (const char *p = name; *p && n < SONG_NAME_MAX; p++)
        safe[n++] = *p == '"' || *p == '\\' ? '?' : *p;
    safe[n] = 0;
    n = snprintf(buf, (size_t)max,
                 "{\"format\":\"fritzdash-song\",\"version\":1,\"name\":\"%s\",\"bpm\":%d,\"scale\":%d,\"sound\":%d,\n"
                 "\"lead\":\"%s\",\n\"sparkle\":\"%s\",\n\"bass\":\"%s\",\n\"drums\":\"%s\"}\n",
                 safe, u->bpm, u->scale, u->lead_wave, rows[0], rows[1], rows[2], rows[3]);
    return n < max ? n : -1;
}

void songs_load_all(void)
{
    for (int i = 0; i < USER_SONGS; i++) {
        char path[160];
        song_path(i, path, sizeof(path));
        int n = plat_read_file(path, text_buf, (int)sizeof(text_buf) - 1);
        saved_used[i] = n > 0 && n < (int)sizeof(text_buf) - 1 &&
                        song_parse(&saved[i], saved_names[i], sizeof(saved_names[i]), text_buf, n);
        if (!saved_used[i]) {
            memset(&saved[i], 0, sizeof(saved[i]));
            saved[i].bpm = 120;
            saved_names[i][0] = 0;
        }
        synth_user_song(i, &saved[i]);
    }
}

int songs_used(int n)
{
    return n >= 0 && n < USER_SONGS && saved_used[n];
}

const char *songs_name(int n)
{
    return songs_used(n) ? saved_names[n] : "";
}

int songs_count(void)
{
    int c = 0;
    for (int i = 0; i < USER_SONGS; i++)
        c += saved_used[i];
    return c;
}

int songs_example(int e, UserSong *u, char *name, int name_max)
{
    const char *t = EXAMPLES[e < 0 || e >= NUM_SONG_EXAMPLES ? 0 : e].text;
    return song_parse(u, name, name_max, t, (int)strlen(t));
}

const char *songs_example_caption(int e)
{
    return EXAMPLES[e < 0 || e >= NUM_SONG_EXAMPLES ? 0 : e].caption;
}

int song_play_step(const UserSong *u, int frames)
{
    // Steps are eighth notes: two a beat.
    return (int)((long)frames * u->bpm * 2 / 3600) % USER_STEPS;
}

// ---------------------------------------------------------------------------
// The song maker

static void toast(SongMaker *s, const char *m)
{
    snprintf(s->msg, sizeof(s->msg), "%s", m);
    s->msg_frames = 100;
}

static int rows_of(int track)
{
    return track == TRACK_DRUMS ? 3 : 8;
}

void songmaker_open(SongMaker *s)
{
    int sel = s->slot_sel;
    memset(s, 0, sizeof(*s));
    s->slot_sel = sel;
    s->screen = SM_SLOTS;
    s->hold_dir = -1;
}

static int save_song(SongMaker *s)
{
    char path[160];
    song_path(s->slot, path, sizeof(path));
    int n = song_write(&s->song, s->name, text_buf, (int)sizeof(text_buf));
    if (n < 0 || plat_write_file(path, text_buf, n) != 0) {
        toast(s, "COULD NOT SAVE!");
        return -1;
    }
    saved[s->slot] = s->song;
    snprintf(saved_names[s->slot], sizeof(saved_names[0]), "%s", s->name);
    saved_used[s->slot] = 1;
    s->dirty = 0;
    s->autosave = 0;
    return 0;
}

static void open_song(SongMaker *s, int slot)
{
    s->slot = slot;
    s->example = 0;
    if (saved_used[slot]) {
        s->song = saved[slot];
        snprintf(s->name, sizeof(s->name), "%s", saved_names[slot]);
    } else {
        default_song(&s->song);
        snprintf(s->name, sizeof(s->name), "MY SONG %d", slot + 1);
    }
    s->screen = SM_EDIT;
    s->track = TRACK_LEAD;
    s->step = 0;
    s->row = 3;
    s->playing = 0;
    s->undo_count = 0;
    s->dirty = !saved_used[slot];
    s->hold_dir = -1;
}

static void stop(SongMaker *s)
{
    if (s->playing)
        synth_song(-1);
    s->playing = 0;
}

static void play(SongMaker *s)
{
    int n = s->example ? SONG_PREVIEW : s->slot;
    synth_user_song(n, &s->song);
    synth_song(SONG_USER + n);
    s->playing = 1;
    s->play_t = 0;
}

// Every change goes to the synth at once, so a playing loop has it.
static void changed(SongMaker *s)
{
    s->dirty = 1;
    synth_user_song(s->slot, &s->song);
}

static void push_undo(SongMaker *s)
{
    s->undo[s->undo_head] = s->song;
    s->undo_head = (s->undo_head + 1) % SONG_UNDO;
    if (s->undo_count < SONG_UNDO)
        s->undo_count++;
}

static void move_cursor(SongMaker *s, int input)
{
    const int dirs = IN_LEFT | IN_RIGHT | IN_UP | IN_DOWN | IN_RS_LEFT | IN_RS_RIGHT;
    int dir = input & dirs, move = 0;
    if (dir != s->hold_dir) {
        s->hold_dir = dir;
        s->hold_timer = 14;
        move = dir != 0;
    } else if (dir && --s->hold_timer <= 0) {
        s->hold_timer = 4;
        move = 1;
    }
    if (!move)
        return;
    int step = (dir & (IN_RS_LEFT | IN_RS_RIGHT)) ? 4 : 1;
    if (dir & (IN_LEFT | IN_RS_LEFT))
        s->step = (s->step + USER_STEPS - step) % USER_STEPS;
    if (dir & (IN_RIGHT | IN_RS_RIGHT))
        s->step = (s->step + step) % USER_STEPS;
    if ((dir & IN_UP) && s->row > 0)
        s->row--;
    if ((dir & IN_DOWN) && s->row < rows_of(s->track) - 1)
        s->row++;
}

static void step_edit(SongMaker *s, int input, int edges)
{
    if (edges & (IN_R2 | IN_TOUCH)) {
        s->screen = SM_MENU;
        s->menu_sel = SMM_RESUME;
        s->clear_armed = 0;
        return;
    }
    if (edges & (IN_START | IN_TRIANGLE)) {
        if (s->playing)
            stop(s);
        else
            play(s);
    }
    move_cursor(s, input);
    if (edges & (IN_L1 | IN_R1)) {
        s->track = (s->track + ((edges & IN_R1) ? 1 : TRACK_COUNT - 1)) % TRACK_COUNT;
        s->row = s->track == TRACK_DRUMS ? 2 : s->row > 7 ? 7 : s->row;
        synth_sfx(SND_MOVE);
    }
    uint8_t *cell = &s->song.notes[s->track][s->step];
    if (edges & IN_CROSS) {
        push_undo(s);
        if (s->track == TRACK_DRUMS) {
            *cell ^= (uint8_t)DRUM_BITS[s->row];
            if (*cell & DRUM_BITS[s->row])
                synth_tone(&s->song, TRACK_DRUMS, DRUM_BITS[s->row]);
        } else {
            int note = 8 - s->row;
            *cell = (uint8_t)(*cell == note ? 0 : note);
            if (*cell)
                synth_tone(&s->song, s->track, note);
        }
        changed(s);
    } else if ((edges & IN_SQUARE) && *cell) {
        push_undo(s);
        *cell = 0;
        changed(s);
        synth_sfx(SND_DELETE);
    }
    if (edges & IN_CIRCLE) {
        if (!s->undo_count) {
            toast(s, "NOTHING TO UNDO");
        } else {
            s->undo_head = (s->undo_head + SONG_UNDO - 1) % SONG_UNDO;
            s->undo_count--;
            s->song = s->undo[s->undo_head];
            changed(s);
            toast(s, "UNDO");
        }
    }
}

// An example plays on its own; it opens on its first instrument that has notes.
static void open_example(SongMaker *s, int e)
{
    songs_example(e, &s->song, s->name, sizeof(s->name));
    s->example = e + 1;
    s->screen = SM_EDIT;
    s->track = TRACK_LEAD;
    for (int t = TRACK_COUNT - 1; t >= 0; t--)
        for (int k = 0; k < USER_STEPS; k++)
            s->track = s->song.notes[t][k] ? t : s->track;
    s->step = 0;
    s->row = s->track == TRACK_DRUMS ? 2 : 3;
    s->undo_count = 0;
    s->dirty = 0;
    s->hold_dir = -1;
    play(s);
}

// COPY TO MY SLOT: the example goes into his first empty slot, saved, and
// stays open there to change. With every slot used nothing is touched.
static void copy_example(SongMaker *s)
{
    int slot = 0;
    while (slot < USER_SONGS && saved_used[slot])
        slot++;
    if (slot == USER_SONGS) {
        toast(s, "ALL 10 SONG SLOTS ARE USED - DELETE ONE FIRST");
        synth_sfx(SND_BACK);
        return;
    }
    stop(s);
    s->example = 0;
    s->slot = s->slot_sel = slot;
    s->dirty = 1;
    if (save_song(s) != 0)
        return;
    synth_user_song(slot, &s->song);
    char m[48];
    snprintf(m, sizeof(m), "COPIED TO SONG %d - NOW CHANGE IT!", slot + 1);
    toast(s, m);
    synth_sfx(SND_SELECT);
}

static void step_example(SongMaker *s, int input, int edges)
{
    if (edges & IN_CIRCLE) {
        stop(s);
        s->example = 0;
        s->screen = SM_SLOTS;
        synth_sfx(SND_BACK);
        return;
    }
    if (edges & (IN_START | IN_TRIANGLE)) {
        if (s->playing)
            stop(s);
        else
            play(s);
    }
    move_cursor(s, input);
    if (edges & (IN_L1 | IN_R1)) {
        s->track = (s->track + ((edges & IN_R1) ? 1 : TRACK_COUNT - 1)) % TRACK_COUNT;
        s->row = s->track == TRACK_DRUMS ? 2 : s->row > 7 ? 7 : s->row;
        synth_sfx(SND_MOVE);
    }
    if (edges & IN_CROSS)
        copy_example(s);
    else if (edges & (IN_SQUARE | IN_R2 | IN_TOUCH))
        toast(s, "AN EXAMPLE CAN'T BE CHANGED - X COPIES IT");
}

static int step_menu(SongMaker *s, int edges)
{
    if (edges & (IN_CIRCLE | IN_R2 | IN_TOUCH)) {
        s->screen = SM_EDIT;
        return 0;
    }
    if (edges & (IN_UP | IN_DOWN)) {
        s->menu_sel = (s->menu_sel + ((edges & IN_UP) ? SMM_COUNT - 1 : 1)) % SMM_COUNT;
        s->clear_armed = 0;
        synth_sfx(SND_MOVE);
    }
    int step = (edges & (IN_CROSS | IN_RIGHT)) ? 1 : (edges & IN_LEFT) ? -1 : 0, pick = (edges & IN_CROSS) != 0;
    if (!step)
        return 0;
    switch (s->menu_sel) {
    case SMM_RESUME:
        if (pick)
            s->screen = SM_EDIT;
        break;
    case SMM_PLAY:
        if (pick) {
            if (s->playing)
                stop(s);
            else
                play(s);
            s->screen = SM_EDIT;
        }
        break;
    case SMM_TEMPO:
        s->song.bpm = s->song.bpm + step * 10 < 60 ? 200 : s->song.bpm + step * 10 > 200 ? 60 : s->song.bpm + step * 10;
        changed(s);
        if (s->playing)
            play(s);
        break;
    case SMM_SCALE:
        s->song.scale = (s->song.scale + step + SCALE_COUNT) % SCALE_COUNT;
        changed(s);
        break;
    case SMM_SOUND:
        s->song.lead_wave = (s->song.lead_wave + step + 3) % 3;
        changed(s);
        synth_tone(&s->song, TRACK_LEAD, 5);
        break;
    case SMM_NAME:
        if (pick) {
            kb_open(&s->kb, s->name, SONG_NAME_MAX);
            s->screen = SM_NAME;
        }
        break;
    case SMM_CLEAR:
        if (!pick)
            break;
        if (!s->clear_armed) {
            s->clear_armed = 1;
            toast(s, "PRESS X AGAIN TO CLEAR IT");
        } else {
            push_undo(s);
            memset(s->song.notes[s->track], 0, USER_STEPS);
            changed(s);
            s->clear_armed = 0;
            s->screen = SM_EDIT;
            toast(s, "CLEARED - CIRCLE UNDOES IT");
        }
        break;
    case SMM_SAVE:
        if (pick && save_song(s) == 0)
            toast(s, "SAVED!");
        break;
    case SMM_SAVE_EXIT:
        if (pick && save_song(s) == 0) {
            stop(s);
            s->screen = SM_SLOTS;
        }
        break;
    }
    return 0;
}

// Two rows of five slots, then a row of the examples.
static int step_slots(SongMaker *s, int edges)
{
    int sel = s->slot_sel, row = sel < 5 ? 0 : sel < USER_SONGS ? 1 : 2;
    int n = row < 2 ? 5 : NUM_SONG_EXAMPLES, col = sel - row * 5;
    if (edges & IN_LEFT) col = (col + n - 1) % n;
    if (edges & IN_RIGHT) col = (col + 1) % n;
    if (edges & (IN_UP | IN_DOWN)) {
        row = (row + ((edges & IN_UP) ? 2 : 1)) % 3;
        n = row < 2 ? 5 : NUM_SONG_EXAMPLES;
        col = col < n ? col : n - 1;
    }
    sel = row * 5 + col;
    if (sel != s->slot_sel) {
        s->slot_sel = sel;
        s->slot_delete = 0;
        synth_sfx(SND_MOVE);
    }
    if (edges & IN_CIRCLE)
        return 1;
    if ((edges & IN_CROSS) && sel >= USER_SONGS) {
        open_example(s, sel - USER_SONGS);
        synth_sfx(SND_SELECT);
    } else if (edges & IN_CROSS) {
        open_song(s, sel);
        synth_sfx(SND_SELECT);
    } else if ((edges & IN_SQUARE) && sel < USER_SONGS && saved_used[sel]) {
        if (s->slot_delete == sel + 1) {
            char path[160];
            song_path(sel, path, sizeof(path));
            plat_remove_file(path);
            saved_used[sel] = 0;
            saved_names[sel][0] = 0;
            memset(&saved[sel], 0, sizeof(saved[sel]));
            saved[sel].bpm = 120;
            synth_user_song(sel, &saved[sel]);
            s->slot_delete = 0;
            toast(s, "DELETED");
        } else {
            s->slot_delete = sel + 1;
            toast(s, "PRESS SQUARE AGAIN TO DELETE IT");
        }
    }
    return 0;
}

int songmaker_step(SongMaker *s, int input, int edges)
{
    if (s->msg_frames > 0)
        s->msg_frames--;
    if (s->playing)
        s->play_t++;
    switch (s->screen) {
    case SM_SLOTS:
        return step_slots(s, edges);
    case SM_EDIT:
        if (s->example) {
            step_example(s, input, edges);
            break;
        }
        step_edit(s, input, edges);
        if (s->dirty && ++s->autosave >= 3600)
            save_song(s);
        break;
    case SM_MENU:
        step_menu(s, edges);
        break;
    case SM_NAME: {
        int r = kb_step(&s->kb, edges);
        if (r == KB_DONE && s->kb.text[0]) {
            snprintf(s->name, sizeof(s->name), "%s", s->kb.text);
            s->dirty = 1;
        }
        if (r != KB_TYPING)
            s->screen = SM_MENU;
        break;
    }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Drawing

static void draw_slots(const SongMaker *s, int frame)
{
    draw_gradient(RGB(20, 4, 40), RGB(70, 10, 90));
    for (int x = -((frame * 2) % 192); x < SCREEN_W; x += 192)
        draw_rect(x, 0, 3, SCREEN_H, RGB(60, 20, 100));
    draw_text_glow_centered(40, 9, "MY SONGS", COL_WHITE, COL_PINK);
    for (int i = 0; i < USER_SONGS; i++) {
        int x = 100 + (i % 5) * 350, y = 180 + (i / 5) * 226;
        int on = i == s->slot_sel;
        ui_panel(x, y, 310, 190, on ? COL_GREEN : RGB(80, 60, 140));
        char num[8];
        snprintf(num, sizeof(num), "%d", i + 1);
        draw_text_glow(x + 16, y + 14, 5, num, COL_YELLOW, RGB(200, 120, 0));
        for (int k = 0; k < 4; k++)     // a little equaliser picture
            draw_rect(x + 230 + k * 16, y + 60 - ((k * 7 + i * 3) % 4) * 10, 10, 10 + ((k * 7 + i * 3) % 4) * 10,
                      saved_used[i] ? TRACK_COLS[k] : RGB(50, 40, 80));
        if (saved_used[i]) {
            int sc = ui_fit_scale(saved_names[i], 280, 4);
            draw_text(x + 16, y + 94, sc, saved_names[i], COL_WHITE);
            char info[32];
            snprintf(info, sizeof(info), "%s  %d BPM", SCALE_NAMES[saved[i].scale % SCALE_COUNT], saved[i].bpm);
            draw_text(x + 16, y + 150, 3, info, COL_DIM);
        } else {
            draw_text(x + 16, y + 104, 3, on ? "X TO MAKE A NEW SONG" : "EMPTY", COL_DIM);
        }
    }
    draw_text_glow(100, 640, 4, "EXAMPLES TO LEARN FROM", COL_YELLOW, RGB(200, 120, 0));
    for (int e = 0; e < NUM_SONG_EXAMPLES; e++) {
        int x = 100 + e * 440, y = 696, on = USER_SONGS + e == s->slot_sel;
        UserSong u;
        char name[SONG_NAME_MAX + 1], num[16], parts[48] = "";
        songs_example(e, &u, name, sizeof(name));
        ui_panel(x, y, 400, 160, on ? COL_GREEN : RGB(200, 140, 30));
        snprintf(num, sizeof(num), "EXAMPLE %d", e + 1);
        draw_text(x + 16, y + 14, 3, num, COL_YELLOW);
        draw_text_glow(x + 16, y + 52, ui_fit_scale(name, 368, 5), name, COL_WHITE, COL_PINK);
        for (int t = 0; t < TRACK_COUNT; t++)
            for (int k = 0; k < USER_STEPS; k++)
                if (u.notes[t][k]) {
                    snprintf(parts + strlen(parts), sizeof(parts) - strlen(parts), "%s%s", parts[0] ? " " : "", TRACK_NAMES[t]);
                    break;
                }
        if (text_width(parts, 3) > 368)
            snprintf(parts, sizeof(parts), "ALL FOUR TOGETHER");
        draw_text(x + 16, y + 118, 3, parts, COL_DIM);
    }
    if (s->msg_frames > 0)
        draw_text_glow_centered(SCREEN_H - 170, 5, s->msg, COL_YELLOW, COL_PINK);
    if (s->slot_sel >= USER_SONGS) {
        const char *c = songs_example_caption(s->slot_sel - USER_SONGS);
        draw_text_glow_centered(SCREEN_H - 125, ui_fit_scale(c, SCREEN_W - 120, 5), c, COL_WHITE, RGB(90, 90, 200));
        draw_text_glow_centered(SCREEN_H - 60, 4, "X LISTEN AND LOOK   CIRCLE BACK", RGB(220, 220, 255), RGB(90, 90, 200));
        return;
    }
    draw_text_glow_centered(SCREEN_H - 120, 4, "YOUR LEVELS CAN PLAY THESE SONGS: PICK ONE IN THE EDITOR MENU",
                            COL_WHITE, RGB(90, 90, 200));
    draw_text_glow_centered(SCREEN_H - 60, 4, "X OPEN   SQUARE DELETE   CIRCLE BACK", RGB(220, 220, 255), RGB(90, 90, 200));
}

static void draw_note_cell(int x, int y, int w, int h, int note, int faint)
{
    color_t c = NOTE_COLS[note];
    if (faint)
        c = col_scale(c, 1, 3);
    draw_rect(x + 3, y + 3, w - 6, h - 6, col_scale(c, 1, 2));
    draw_rect(x + 7, y + 7, w - 14, h - 14, c);
}

static void draw_grid(const SongMaker *s, int frame)
{
    int tune = s->track != TRACK_DRUMS, rows = rows_of(s->track);
    int y0 = tune ? TUNE_Y : DRUM_Y, rh = tune ? TUNE_H : DRUM_H, h = rows * rh;
    int ps = s->playing ? song_play_step(&s->song, s->play_t) : -1;
    draw_rect(GRID_X - 6, y0 - 6, USER_STEPS * CELL_W + 12, h + 12, TRACK_COLS[s->track]);
    draw_rect(GRID_X, y0, USER_STEPS * CELL_W, h, RGB(8, 4, 20));
    for (int r = 0; r < rows; r++) {
        int y = y0 + r * rh;
        if (r % 2 == 0)
            draw_rect(GRID_X, y, USER_STEPS * CELL_W, rh, RGB(16, 10, 34));
        char lab[8];
        if (tune) {
            snprintf(lab, sizeof(lab), "%d", 8 - r);
            draw_rect(GRID_X - 70, y + 8, 50, rh - 16, NOTE_COLS[8 - r]);
            draw_text(GRID_X - 45 - text_width(lab, 4) / 2, y + rh / 2 - 14, 4, lab, RGB(0, 0, 0));
        } else {
            draw_text(GRID_X - 16 - text_width(DRUM_ROWS[r], 3), y + rh / 2 - 10, 3, DRUM_ROWS[r], COL_WHITE);
        }
    }
    for (int k = 0; k <= USER_STEPS; k++)
        draw_rect(GRID_X + k * CELL_W - 1, y0, k % 8 == 0 ? 4 : 2, h,
                  k % 8 == 0 ? RGB(150, 140, 200) : k % 2 == 0 ? RGB(60, 50, 100) : RGB(30, 24, 60));
    if (ps >= 0)
        draw_rect(GRID_X + ps * CELL_W, y0, CELL_W, h, RGB(60, 60, 110));
    for (int k = 0; k < USER_STEPS; k++) {
        int v = s->song.notes[s->track][k], x = GRID_X + k * CELL_W;
        if (tune && v >= 1 && v <= 8)
            draw_note_cell(x, y0 + (8 - v) * rh, CELL_W, rh, v, 0);
        for (int r = 0; !tune && r < 3; r++)
            if (v & DRUM_BITS[r]) {
                int cx = x + CELL_W / 2, cy = y0 + r * rh + rh / 2, big = k == ps ? 6 : 0;
                draw_circle(cx, cy, 22 + big, col_scale(TRACK_COLS[TRACK_DRUMS], 1, 2));
                draw_circle(cx, cy, 16 + big, r == 2 ? COL_GREEN : r == 1 ? COL_WHITE : COL_YELLOW);
            }
    }
    int cx = GRID_X + s->step * CELL_W, cy = y0 + s->row * rh;
    if (s->screen == SM_EDIT && !s->example)
        draw_frame(cx - 5, cy - 5, CELL_W + 10, rh + 10, 5, (frame / 6) % 2 ? COL_YELLOW : COL_WHITE);
    if (ps >= 0) {
        float px = (float)(GRID_X + ps * CELL_W + CELL_W / 2);
        float xs[3] = { px - 16, px + 16, px }, ys[3] = { (float)y0 - 40, (float)y0 - 40, (float)y0 - 14 };
        draw_poly(3, xs, ys, COL_WHITE);
    }
}

// All four instruments small, so the whole song shows at once.
static void draw_overview(const SongMaker *s)
{
    int y0 = 720;
    for (int t = 0; t < TRACK_COUNT; t++) {
        int y = y0 + t * 34;
        draw_text(GRID_X - 16 - text_width(TRACK_NAMES[t], 2), y + 8, 2, TRACK_NAMES[t], t == s->track ? COL_WHITE : COL_DIM);
        draw_rect(GRID_X, y, USER_STEPS * CELL_W, 28, t == s->track ? RGB(30, 20, 60) : RGB(14, 8, 30));
        for (int k = 0; k < USER_STEPS; k++) {
            int v = s->song.notes[t][k];
            if (!v)
                continue;
            color_t c = t == TRACK_DRUMS ? TRACK_COLS[t] : NOTE_COLS[v > 8 ? 8 : v];
            int hh = t == TRACK_DRUMS ? 20 : 6 + v * 2;
            draw_rect(GRID_X + k * CELL_W + 8, y + 28 - hh - 2, CELL_W - 16, hh, c);
        }
    }
}

static void draw_menu(const SongMaker *s)
{
    char items[SMM_COUNT][48];
    snprintf(items[SMM_RESUME], 48, "BACK TO MAKING");
    snprintf(items[SMM_PLAY], 48, s->playing ? "STOP" : "PLAY");
    snprintf(items[SMM_TEMPO], 48, "SPEED: < %d >", s->song.bpm);
    snprintf(items[SMM_SCALE], 48, "NOTES: < %s >", SCALE_NAMES[s->song.scale % SCALE_COUNT]);
    snprintf(items[SMM_SOUND], 48, "LEAD SOUND: < %s >", LEAD_NAMES[s->song.lead_wave % 3]);
    snprintf(items[SMM_NAME], 48, "NAME: %s", s->name);
    snprintf(items[SMM_CLEAR], 48, s->clear_armed ? "CLEAR %s: SURE? PRESS X" : "CLEAR ALL %s", TRACK_NAMES[s->track]);
    snprintf(items[SMM_SAVE], 48, "SAVE");
    snprintf(items[SMM_SAVE_EXIT], 48, "SAVE AND EXIT");
    static const char *const HELP[SMM_COUNT] = {
        "", "LISTEN TO YOUR SONG", "LEFT AND RIGHT: SLOWER OR FASTER", "HAPPY, SPOOKY OR DREAMY NOTES",
        "HOW THE LEAD SOUNDS", "GIVE YOUR SONG A NAME", "TAKES EVERY NOTE OFF THIS INSTRUMENT", "", "",
    };
    int w = 1100, h = 880, x = (SCREEN_W - w) / 2, y = 100;
    ui_panel(x, y, w, h, COL_PINK);
    draw_text_glow_centered(y + 26, 7, "SONG MENU", COL_WHITE, COL_PINK);
    for (int i = 0; i < SMM_COUNT; i++) {
        int yy = y + 130 + i * 68, scale = ui_fit_scale(items[i], w - 120, 5);
        if (i == s->menu_sel) {
            int tw = text_width(items[i], scale) + 40;
            draw_frame((SCREEN_W - tw) / 2, yy - 12, tw, 7 * scale + 24, 4, COL_GREEN);
            draw_text_glow_centered(yy, scale, items[i], COL_WHITE, COL_CYAN);
        } else {
            draw_text((SCREEN_W - text_width(items[i], scale)) / 2, yy, scale, items[i], COL_DIM);
        }
    }
    draw_text_glow_centered(y + h - 110, 4, HELP[s->menu_sel], COL_YELLOW, RGB(200, 120, 0));
    draw_text_glow_centered(y + h - 50, 3, "UP DOWN MOVE   X PICK   LEFT RIGHT CHANGE   CIRCLE BACK",
                            RGB(220, 220, 255), RGB(90, 90, 200));
}

void songmaker_draw(const SongMaker *s, int frame)
{
    draw_set_offset(0, 0);
    if (s->screen == SM_SLOTS) {
        draw_slots(s, frame);
        return;
    }
    draw_gradient(RGB(14, 4, 34), RGB(50, 10, 76));
    char buf[80];
    if (s->example)
        snprintf(buf, sizeof(buf), "EXAMPLE: %s", s->name);
    else
        snprintf(buf, sizeof(buf), "%s%s", s->name, s->dirty ? " *" : "");
    draw_text_glow(40, 30, 6, buf, COL_WHITE, s->example ? RGB(200, 140, 30) : COL_PINK);
    if (s->example)
        snprintf(buf, sizeof(buf), "%d BPM   %s   %s LEAD", s->song.bpm, SCALE_NAMES[s->song.scale % SCALE_COUNT],
                 LEAD_NAMES[s->song.lead_wave % 3]);
    else
        snprintf(buf, sizeof(buf), "SONG %d   %d BPM   %s   %s LEAD", s->slot + 1, s->song.bpm,
                 SCALE_NAMES[s->song.scale % SCALE_COUNT], LEAD_NAMES[s->song.lead_wave % 3]);
    draw_text(SCREEN_W - 40 - text_width(buf, 4), 40, 4, buf, COL_DIM);
    for (int t = 0; t < TRACK_COUNT; t++) {
        int w = 320, x = SCREEN_W / 2 - 2 * (w + 20) + t * (w + 20) + 10, on = t == s->track;
        draw_rect(x, 120, w, 76, on ? TRACK_COLS[t] : RGB(30, 20, 60));
        draw_rect(x + 5, 125, w - 10, 66, on ? col_scale(TRACK_COLS[t], 1, 3) : COL_PANEL);
        draw_text(x + (w - text_width(TRACK_NAMES[t], 5)) / 2, 140, 5, TRACK_NAMES[t], on ? COL_WHITE : COL_DIM);
    }
    draw_text(60, 140, 4, "< L1", COL_YELLOW);
    draw_text(SCREEN_W - 60 - text_width("R1 >", 4), 140, 4, "R1 >", COL_YELLOW);
    draw_grid(s, frame);
    draw_overview(s);
    if (s->playing && (frame / 20) % 2)
        draw_text_glow(40, 880, 5, "PLAYING", COL_GREEN, RGB(30, 140, 10));
    if (s->msg_frames > 0 && s->screen == SM_EDIT)
        draw_text_glow_centered(880, 5, s->msg, COL_YELLOW, COL_PINK);
    if (s->example) {
        const char *c = songs_example_caption(s->example - 1);
        draw_text_glow_centered(950, ui_fit_scale(c, SCREEN_W - 120, 5), c, COL_YELLOW, RGB(200, 120, 0));
        draw_text_glow_centered(1020, 3, "X COPY TO MY SONGS   OPTIONS PLAY OR STOP   L1 R1 INSTRUMENT   CIRCLE BACK",
                                RGB(220, 220, 255), RGB(90, 90, 200));
        return;
    }
    draw_text_glow_centered(960, 4, s->track == TRACK_DRUMS ? "X ADDS OR TAKES OFF A DRUM HIT" :
                            "X ADDS A NOTE (HIGHER ROWS ARE HIGHER NOTES)", COL_WHITE, RGB(90, 90, 200));
    draw_text_glow_centered(1020, 3, "D-PAD MOVE   X NOTE   SQUARE CLEAR   L1 R1 INSTRUMENT   OPTIONS PLAY OR STOP   "
                            "CIRCLE UNDO   R2 MENU", RGB(220, 220, 255), RGB(90, 90, 200));
    if (s->screen == SM_MENU)
        draw_menu(s);
    if (s->screen == SM_NAME)
        kb_draw(&s->kb, "NAME YOUR SONG", COL_PINK, frame);
    if (s->msg_frames > 0 && s->screen == SM_MENU)
        draw_text_glow_centered(SCREEN_H - 60, 5, s->msg, COL_YELLOW, COL_PINK);
}
