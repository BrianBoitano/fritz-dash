// Fritz Dash - a small chiptune synth: built-in songs and sound effects,
// mixed to 16-bit stereo. The game thread asks for songs and sounds; the
// audio thread (or the simulator) calls synth_render.
#ifndef FRITZ_SYNTH_H
#define FRITZ_SYNTH_H

#include <stdint.h>

#define NUM_SONGS 15        // songs a level can use ("song": N); 10..14 are the 3.1 world themes
#define SONG_MENU NUM_SONGS // the menu music
#define SONG_PRACTICE 9     // the calm song practice mode plays
#define SONG_THEME1 10      // worlds 1..5 of the level select play songs 10..14

extern const char *const SONG_NAMES[NUM_SONGS];

// His own songs (the song maker): 32 eighth-note steps, looped. Each step of
// a tune track holds 0 (rest) or a note 1..8 on the scale; the drum track
// holds DRUM_ bits.
#define USER_SONGS 10
#define USER_STEPS 32
#define SONG_USER  (NUM_SONGS + 1)     // synth_song(SONG_USER + n) plays his song n
#define SONG_PREVIEW USER_SONGS         // one more user slot: the song maker's examples play from it
#define NUM_SECRET_SONGS 3
#define SONG_SECRET (SONG_USER + USER_SONGS + 1)  // the easter eggs' secret songs (menu music)
#define SONG_THEME_SHAPESHIFT (SONG_SECRET + NUM_SECRET_SONGS)  // world 7's theme, after every older id
enum { TRACK_LEAD, TRACK_SPARKLE, TRACK_BASS, TRACK_DRUMS, TRACK_COUNT };
enum { DRUM_KICK = 1, DRUM_SNARE = 2, DRUM_HAT = 4 };
enum { SCALE_HAPPY, SCALE_SPOOKY, SCALE_DREAMY, SCALE_COUNT };
typedef struct {
    int bpm, scale, lead_wave;          // lead_wave: 0 thin, 1 soft, 2 round
    uint8_t notes[TRACK_COUNT][USER_STEPS];
} UserSong;
extern const char *const SCALE_NAMES[SCALE_COUNT];
extern const char *const LEAD_NAMES[3];
// Hands the audio thread a copy of his song n, or of an example as SONG_PREVIEW
// (safe while it plays).
void synth_user_song(int n, const UserSong *s);
// One note of a track right now (the song maker's preview of a press).
void synth_tone(const UserSong *s, int track, int value);

enum { SND_JUMP, SND_ORB, SND_PAD, SND_PORTAL, SND_DEATH, SND_DONE, SND_CHECKPOINT,
       SND_MOVE, SND_SELECT, SND_BACK, SND_PLACE, SND_DELETE, SND_COIN, SND_BADGE, SND_MORPH, SND_EGG, SND_COUNT };
// Silly sound packs (easter egg rewards) for jumps, orbs, pads, deaths, coins
// and finishes; 0 is the normal sounds.
enum { SFXPACK_NONE, SFXPACK_BOING, SFXPACK_QUACK, SFXPACK_CHIP, SFXPACK_ROBOT, SFXPACK_COUNT };
void synth_sfx_pack(int pack);
// Mode pack B's sounds, numbered from 24 so other packs can add theirs before SND_COUNT.
enum { SND_BLAST = 24, SND_BREAK, SND_BOOM, SND_TICK, SND_SPLAT, SND_SWAP, SND_WHOO, SND_LAST = SND_WHOO };

void synth_init(int rate);
// A song's tempo in beats per minute (for things that pulse with it).
int synth_bpm(int song);
// Starts a song from the beginning (even the one already playing); -1 stops.
void synth_song(int song);
// Starts a song part way in, where it would be after `frames` 60 fps frames.
void synth_song_at(int song, int frames);
void synth_pause(int paused);
void synth_sfx(int sound);
// Volumes 0..10.
void synth_volume(int music, int sfx);
// Fills frames stereo frames (2 * frames samples).
void synth_render(int16_t *out, int frames);

#endif
