// Fritz Dash - chiptune synth. Songs are written as a chord per bar plus a
// melody in scale degrees, and expanded into 16th-note steps at start-up.
// Requests from the game thread go through atomics; everything else is only
// touched by the thread that calls synth_render.
#include <string.h>

#include "synth.h"

#define MAX_BARS   16
#define MAX_STEPS  (MAX_BARS * 16)
#define HOLD       (-1)
#define OFF        (-2)

typedef struct {
    const char *name;
    int bpm, root, minor;
    const char *chords;     // one scale degree per bar
    const char *melody;     // one per 8th note: 1-9 scale degree (8, 9 = next octave), '.' hold, '-' stop
    const char *drums;      // 16 steps, repeated: K kick, S snare, H hat, X kick+hat, Y snare+hat
    int bass, arp;          // bass: 0 bouncing octaves, 1 driving 8ths; arp: 0 none, 1 16ths, 2 8ths
    int lead_wave;          // 0 thin square, 1 soft triangle, 2 round square
} SongDef;

static const SongDef SONGS[NUM_SONGS + 1] = {
    { "NEON STEPS", 126, 60, 0, "15641564",
      "5.3.1.3.2.5.7.5.6.8.6.5.4.6.5.3.5.5.8.5.7.5.2.5.3.6.8.9.8.6.4.-.",
      "X.H.Y.H.X.H.Y.HH", 0, 1, 0 },
    { "BOUNCE CASTLE", 136, 55, 0, "14516455",
      "1.3.5.8.6.4.6.8.7.5.2.5.8.-.5.3.6.5.3.6.4.6.8.6.5.7.9.7.5.4.3.2.",
      "X.H.Y.HXX.H.Y.H.", 1, 2, 0 },
    { "GRAVITY GROOVE", 140, 57, 1, "16711675",
      "1.3.5.3.6.5.3.1.7.5.2.7.8...-.5.3.5.8.5.6.8.6.5.7.9.7.5.5.7.5.-.",
      "X.H.Y.H.X.XHY.H.", 0, 1, 0 },
    { "SKY ROCKET", 150, 62, 1, "16371637",
      "8.5.3.5.6.4.1.4.3.5.8.5.7.5.2.5.8.9.8.5.6.8.6.4.5.3.5.8.7...-.-.",
      "X.HXY.H.X.HXY.HH", 1, 1, 0 },
    { "FINAL DASH", 160, 64, 1, "14651465",
      "1.5.8.5.4.6.8.6.6.8.3.8.5.7.2.7.8.7.5.3.4.3.1.3.6.5.6.8.7.-.5.7.",
      "X.HXY.HXX.HXY.YY", 1, 1, 0 },
    { "TINY STEPS", 132, 67, 0, "16451645",
      "5.8.5.3.6.8.9.8.6.4.6.8.7.5.2.5.8.5.3.5.6...8...9.8.6.4.5.7.5.-.",
      "X.H.Y.H.X.HXY.H.", 0, 1, 2 },
    { "UFO DISCO", 120, 57, 1, "14651471",
      "1.1.3.5.4...6.5.3.1.3.4.5...-.8.7.5.4.3.4.5.1.1.3.5.4.6.7.6.5.-.",
      "X.HHY.HHX.HHY.HX", 1, 2, 2 },
    { "ZIGZAG", 168, 62, 1, "16451675",
      "8.5.8.5.6.8.6.3.4.6.8.6.5.7.9.7.8.9.8.5.6.5.3.1.7.5.7.9.8...-.5.",
      "X.HXY.HXX.HXY.YY", 1, 1, 0 },
    { "FIRST STEPS", 116, 60, 0, "15641541",
      "1.3.5.3.5.7.5.2.6.5.3.1.4.6.4.1.3.5.8.5.7.5.2.5.4.6.8.6.8...-...",
      "K.H.S.H.K.H.S.H.", 0, 2, 1 },
    { "CHILL ZONE", 92, 65, 0, "16451625",
      "5...3...6...8...6...4...5...2...3...5...8...6...4...6...5.......",
      "K.......H.......", 0, 2, 1 },
    // 3.1: one theme per new world.
    { "ISLAND SUNRISE", 118, 62, 0, "14151451",
      "1.3.5.8.6...4.6.5.3.1.3.2.5.7.5.8.7.8.5.6.5.4.6.5.4.2.7.8...-...",
      "X..HY.HXX..HY.H.", 0, 2, 1 },
    { "BUBBLE REEF", 96, 64, 0, "16251625",
      "5.-.3.5.6.-.8...4.6.4.2.5...-...8.7.5.3.6.5.3.1.2.4.6.4.5...7...",
      "K..HH.K.S..H..H.", 0, 1, 2 },
    { "FORGE MARCH", 104, 55, 1, "11441155",
      "1...1.1.3...2.1.4...4.4.6...5.4.5...5.5.8...7.5.7.5.2.5.7...-...",
      "K.SSK.S.K.SSKSSS", 1, 0, 0 },
    { "NEON NIGHTS", 108, 60, 1, "16471647",
      "8...7.5.6.......4.5.6.8.7...5...8...9.8.6...5.3.4...6.4.5...-...",
      "K...Y...K.K.Y..H", 1, 1, 2 },
    { "COSMIC FINALE", 144, 59, 1, "1647164516431555",
      "1.5.8.5.6.8.9.8.4.6.8.6.7.9.7.5.8...5...6...8...9...8.6.5...-..."
      "8.8.9.8.6.6.8.6.4.6.8.9.3.5.8.5.8.9.8.5.7.5.2.5.7.9.7.5.5...-...",
      "X.H.YXH.X.HXYXHY", 1, 1, 2 },
    { "MENU", 112, 65, 0, "16451645",
      "5...3...6...8...4...6...5...-...8...5...6...3...4...2...1...-...",
      "K...H...S...H...", 0, 1, 0 },
};

// The easter eggs' secret songs.
static const SongDef SECRET_SONGS[NUM_SECRET_SONGS] = {
    { "EGG HUNT", 124, 62, 0, "15461546",
      "5.5.3.5.8...6.5.4.4.6.4.8...7.5.3.5.6.5.3.1.2.3.4.6.5...8...-...",
      "X.HHY.H.X.HXY.HH", 0, 2, 1 },
    { "MOON BOUNCE", 100, 57, 1, "16471643",
      "1...5.8.7.5.3...4...6.9.8.6.4...5.3.5.8.7...5...6.5.4.2.1...-...",
      "K..HS.H.K.KHS..H", 1, 1, 2 },
    { "GOLDEN", 146, 64, 0, "14561456",
      "8.7.5.8.9.8.6.5.6.8.9.8.7.5.2.5.8.9.8.7.5.3.5.8.9...8...9...8.-.",
      "X.HXY.HXX.HXY.YY", 1, 1, 0 },
};

// World 7 SHAPESHIFT's theme in the level list: the second half (bars 9 to
// 16) is lifted SHAPESHIFT_LIFT semitones, a key change halfway.
static const SongDef SHAPESHIFT_THEME = {
    "PRISM SHIFT", 152, 60, 0, "1564156416451455",
    "1.3.5.8.7.5.3.5.6.8.9.8.6.5.4.2.1.3.5.8.7.5.8.9.8.6.5.4.5...-..."
    "8.8.9.8.6.8.5.3.4.6.8.6.5.7.9.7.8.9.8.6.5.6.8.9.8...9...8.5.8.-.",
    "X.HXY.HXX.HXY.YY", 1, 1, 2 };
#define SHAPESHIFT_LIFT 2

const char *const SONG_NAMES[NUM_SONGS] = {
    "NEON STEPS", "BOUNCE CASTLE", "GRAVITY GROOVE", "SKY ROCKET", "FINAL DASH",
    "TINY STEPS", "UFO DISCO", "ZIGZAG", "FIRST STEPS", "CHILL ZONE",
    "ISLAND SUNRISE", "BUBBLE REEF", "FORGE MARCH", "NEON NIGHTS", "COSMIC FINALE",
};
const char *const SCALE_NAMES[SCALE_COUNT] = { "HAPPY", "SPOOKY", "DREAMY" };
const char *const LEAD_NAMES[3] = { "THIN", "SOFT", "ROUND" };

#define USER_SLOTS (USER_SONGS + 1)
#define ALL_SONGS (SONG_THEME_SHAPESHIFT + 1)

typedef struct {
    int nsteps, bpm, lead_wave;
    int16_t lead[MAX_STEPS], bass[MAX_STEPS], arp[MAX_STEPS];
    uint8_t drums[MAX_STEPS];
} Song;

enum { W_SQ50, W_SQ25, W_SQ12, W_TRI, W_NOISE };
enum { DR_KICK = 1, DR_SNARE = 2, DR_HAT = 4 };

typedef struct {
    float phase, inc, dinc;
    float amp, decay, vol, noise;
    int wave, sweep_left, delay;
} Voice;

enum { V_LEAD, V_ARP, V_BASS, V_KICK, V_NOISE, V_SFX, NVOICES = V_SFX + 3 };

static Song songs[ALL_SONGS];
static Voice voices[NVOICES];
static float note_hz[128];
static int rate;
static int cur_song = -1, step;
static float step_left, step_len;
static int paused;
static float music_gain = 0.49f, sfx_gain = 0.64f;
static uint32_t lfsr = 0xACE1u;

static volatile int req_song = -1, req_seq, req_pause, req_music = 7, req_fx = 8, req_start, req_tone;
static volatile uint32_t req_sfx;
static volatile int req_pack;
static int seen_seq;
// His songs: the game thread fills pending[n] and bumps user_seq[n]; the audio
// thread expands a song whose number moved (again if it moved while copying).
static UserSong pending[USER_SLOTS];
static volatile int user_seq[USER_SLOTS];
static int user_seen[USER_SLOTS];

static const int MAJOR[7] = { 0, 2, 4, 5, 7, 9, 11 };
static const int MINOR[7] = { 0, 2, 3, 5, 7, 8, 10 };

static int degree_note(const SongDef *d, int deg)
{
    const int *scale = d->minor ? MINOR : MAJOR;
    return d->root + scale[(deg - 1) % 7] + 12 * ((deg - 1) / 7);
}

static void expand(const SongDef *d, Song *s)
{
    int bars = (int)strlen(d->chords);
    if (bars > MAX_BARS)
        bars = MAX_BARS;
    s->nsteps = bars * 16;
    s->bpm = d->bpm;
    s->lead_wave = d->lead_wave;
    for (int i = 0; i < s->nsteps; i++)
        s->lead[i] = s->bass[i] = s->arp[i] = HOLD;
    int mlen = (int)strlen(d->melody);
    for (int i = 0; i < mlen && i * 2 < s->nsteps; i++) {
        char c = d->melody[i];
        if (c >= '1' && c <= '9')
            s->lead[i * 2] = (int16_t)(degree_note(d, c - '0') + 12);
        else if (c == '-')
            s->lead[i * 2] = OFF;
    }
    for (int b = 0; b < bars; b++) {
        int deg = d->chords[b] - '0';
        int root = degree_note(d, deg) - 24;
        int third = degree_note(d, deg + 2), fifth = degree_note(d, deg + 4);
        int tones[4] = { degree_note(d, deg), third, fifth, degree_note(d, deg) + 12 };
        for (int k = 0; k < 16; k++) {
            int i = b * 16 + k;
            if (d->bass == 0 && k % 2 == 0)
                s->bass[i] = (int16_t)(k % 4 == 0 ? root : root + 12);
            else if (d->bass == 1 && k % 2 == 0)
                s->bass[i] = (int16_t)(k == 14 ? fifth - 24 : root);
            if (d->arp == 1)
                s->arp[i] = (int16_t)tones[k % 4];
            else if (d->arp == 2 && k % 2 == 0)
                s->arp[i] = (int16_t)tones[(k / 2) % 3];
        }
    }
    for (int k = 0; k < s->nsteps; k++) {
        char c = d->drums[k % 16 % (int)strlen(d->drums)];
        s->drums[k] = (uint8_t)((c == 'K' || c == 'X') * DR_KICK | (c == 'S' || c == 'Y') * DR_SNARE |
                                (c == 'H' || c == 'X' || c == 'Y') * DR_HAT);
    }
}

static const int PENTA[5] = { 0, 2, 4, 7, 9 };

// Note 1..8 of his song's scale, as a MIDI note in the octave from middle C.
static int user_note(int scale, int n)
{
    if (scale == SCALE_DREAMY)
        return 60 + PENTA[(n - 1) % 5] + 12 * ((n - 1) / 5);
    const int *sc = scale == SCALE_SPOOKY ? MINOR : MAJOR;
    return (scale == SCALE_SPOOKY ? 57 : 60) + sc[(n - 1) % 7] + 12 * ((n - 1) / 7);
}

// Each step is two 16ths: the lead an octave up, the bass two down, a sparkle
// note followed by the note four up the scale, and the drums on the first.
static void expand_user(const UserSong *u, Song *s)
{
    s->nsteps = USER_STEPS * 2;
    s->bpm = u->bpm < 40 ? 40 : u->bpm > 240 ? 240 : u->bpm;
    s->lead_wave = u->lead_wave;
    for (int i = 0; i < s->nsteps; i++) {
        s->lead[i] = s->bass[i] = s->arp[i] = HOLD;
        s->drums[i] = 0;
    }
    for (int k = 0; k < USER_STEPS; k++) {
        int lead = u->notes[TRACK_LEAD][k], sp = u->notes[TRACK_SPARKLE][k], bass = u->notes[TRACK_BASS][k];
        if (lead >= 1 && lead <= 8)
            s->lead[2 * k] = (int16_t)(user_note(u->scale, lead) + 12);
        if (sp >= 1 && sp <= 8) {
            s->arp[2 * k] = (int16_t)(user_note(u->scale, sp) + 12);
            s->arp[2 * k + 1] = (int16_t)(user_note(u->scale, sp + 4 > 8 ? sp - 3 : sp + 4) + 12);
        }
        if (bass >= 1 && bass <= 8)
            s->bass[2 * k] = (int16_t)(user_note(u->scale, bass) - 24);
        s->drums[2 * k] = u->notes[TRACK_DRUMS][k] & 7;
    }
}

void synth_init(int r)
{
    rate = r;
    note_hz[69] = 440.0f;
    for (int i = 70; i < 128; i++)
        note_hz[i] = note_hz[i - 1] * 1.059463094f;
    for (int i = 68; i >= 0; i--)
        note_hz[i] = note_hz[i + 1] / 1.059463094f;
    for (int i = 0; i <= NUM_SONGS; i++)
        expand(&SONGS[i], &songs[i]);
    for (int i = 0; i < USER_SLOTS; i++) {
        user_seen[i] = __atomic_load_n(&user_seq[i], __ATOMIC_ACQUIRE);
        expand_user(&pending[i], &songs[SONG_USER + i]);
    }
    for (int i = 0; i < NUM_SECRET_SONGS; i++)
        expand(&SECRET_SONGS[i], &songs[SONG_SECRET + i]);
    Song *t = &songs[SONG_THEME_SHAPESHIFT];
    expand(&SHAPESHIFT_THEME, t);
    for (int i = t->nsteps / 2; i < t->nsteps; i++) {
        if (t->lead[i] >= 0)
            t->lead[i] += SHAPESHIFT_LIFT;
        if (t->bass[i] >= 0)
            t->bass[i] += SHAPESHIFT_LIFT;
        if (t->arp[i] >= 0)
            t->arp[i] += SHAPESHIFT_LIFT;
    }
}

int synth_bpm(int song)
{
    if (song == SONG_THEME_SHAPESHIFT)
        return SHAPESHIFT_THEME.bpm;
    if (song >= SONG_SECRET && song < SONG_SECRET + NUM_SECRET_SONGS)
        return SECRET_SONGS[song - SONG_SECRET].bpm;
    if (song >= SONG_USER && song < SONG_USER + USER_SLOTS) {
        int bpm = pending[song - SONG_USER].bpm;
        return bpm < 40 ? 40 : bpm > 240 ? 240 : bpm;
    }
    return song >= 0 && song <= NUM_SONGS ? SONGS[song].bpm : 120;
}

void synth_song_at(int song, int frames)
{
    __atomic_store_n(&req_song, song, __ATOMIC_RELAXED);
    __atomic_store_n(&req_start, frames, __ATOMIC_RELAXED);
    __atomic_add_fetch(&req_seq, 1, __ATOMIC_RELEASE);
}

void synth_song(int song)
{
    synth_song_at(song, 0);
}

void synth_user_song(int n, const UserSong *s)
{
    if (n < 0 || n >= USER_SLOTS)
        return;
    pending[n] = *s;
    __atomic_add_fetch(&user_seq[n], 1, __ATOMIC_RELEASE);
}

void synth_tone(const UserSong *s, int track, int value)
{
    int t = 0;
    if (track == TRACK_DRUMS)
        t = 1 << 12 | (value & 7);
    else if (value >= 1 && value <= 8)
        t = (track + 2) << 12 | (s->lead_wave & 3) << 10 | (track == TRACK_BASS ? user_note(s->scale, value) - 12 :
                                                             user_note(s->scale, value) + 12);
    __atomic_store_n(&req_tone, t, __ATOMIC_RELEASE);
}

void synth_pause(int p)
{
    __atomic_store_n(&req_pause, p, __ATOMIC_RELEASE);
}

void synth_sfx(int sound)
{
    __atomic_fetch_or(&req_sfx, 1u << sound, __ATOMIC_ACQ_REL);
}

void synth_sfx_pack(int pack)
{
    __atomic_store_n(&req_pack, pack, __ATOMIC_RELEASE);
}

void synth_volume(int music, int fx)
{
    __atomic_store_n(&req_music, music, __ATOMIC_RELEASE);
    __atomic_store_n(&req_fx, fx, __ATOMIC_RELEASE);
}

// tau: seconds for the sound to fade to about a third; hz1: where a sweep
// ends after `sweep` seconds; delay: seconds before it starts.
static void trigger(Voice *v, int wave, float hz0, float hz1, float sweep, float vol, float tau, float delay)
{
    v->wave = wave;
    v->inc = hz0 / (float)rate;
    v->sweep_left = (int)(sweep * (float)rate);
    v->dinc = v->sweep_left > 0 ? (hz1 - hz0) / (float)rate / (float)v->sweep_left : 0;
    v->amp = 1.0f;
    v->vol = vol;
    v->decay = 1.0f - 1.0f / (tau * (float)rate);
    v->delay = (int)(delay * (float)rate);
    v->phase = 0;
}

static float voice_sample(Voice *v)
{
    if (v->delay > 0) {
        v->delay--;
        return 0;
    }
    if (v->amp < 0.0005f)
        return 0;
    float p = v->phase, s;
    switch (v->wave) {
    case W_SQ50: s = p < 0.5f ? 1.0f : -1.0f; break;
    case W_SQ25: s = p < 0.25f ? 1.0f : -1.0f; break;
    case W_SQ12: s = p < 0.125f ? 1.0f : -1.0f; break;
    case W_TRI: s = p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p; break;
    default: s = v->noise; break;
    }
    v->phase += v->inc;
    if (v->phase >= 1.0f) {
        v->phase -= (float)(int)v->phase;
        if (v->wave == W_NOISE) {
            lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xB400u);
            v->noise = (lfsr & 1u) ? 1.0f : -1.0f;
        }
    }
    if (v->sweep_left > 0) {
        v->sweep_left--;
        v->inc += v->dinc;
        if (v->inc < 0)
            v->inc = 0;
    }
    float out = s * v->amp * v->vol;
    v->amp *= v->decay;
    return out;
}

static void song_step(void)
{
    const Song *s = &songs[cur_song];
    int i = step % s->nsteps;
    static const int LEAD_WAVE[3] = { W_SQ25, W_TRI, W_SQ50 };
    static const float LEAD_VOL[3] = { 0.15f, 0.26f, 0.11f };
    if (s->lead[i] >= 0)
        trigger(&voices[V_LEAD], LEAD_WAVE[s->lead_wave % 3], note_hz[s->lead[i]], 0, 0, LEAD_VOL[s->lead_wave % 3], 0.40f, 0);
    else if (s->lead[i] == OFF)
        voices[V_LEAD].decay = 1.0f - 1.0f / (0.02f * (float)rate);
    if (s->bass[i] >= 0)
        trigger(&voices[V_BASS], W_TRI, note_hz[s->bass[i]], 0, 0, 0.32f, 0.16f, 0);
    if (s->arp[i] >= 0)
        trigger(&voices[V_ARP], W_SQ12, note_hz[s->arp[i]], 0, 0, 0.06f, 0.07f, 0);
    int d = s->drums[i];
    if (d & DR_KICK)
        trigger(&voices[V_KICK], W_TRI, 160.0f, 45.0f, 0.09f, 0.55f, 0.10f, 0);
    if (d & DR_SNARE)
        trigger(&voices[V_NOISE], W_NOISE, 6000.0f, 0, 0, 0.18f, 0.09f, 0);
    else if (d & DR_HAT)
        trigger(&voices[V_NOISE], W_NOISE, 12000.0f, 0, 0, 0.06f, 0.02f, 0);
    step++;
}

static Voice *free_sfx_voice(void)
{
    Voice *best = &voices[V_SFX];
    for (int i = V_SFX; i < NVOICES; i++)
        if (voices[i].amp < best->amp && voices[i].delay == 0)
            best = &voices[i];
    return best;
}

// A sound pack's version of a sound; 0 if the pack leaves it as it is.
static int pack_sfx(int pack, int sound)
{
    Voice *v = free_sfx_voice();
    switch (pack * 32 + sound) {
    case SFXPACK_BOING * 32 + SND_JUMP: trigger(v, W_TRI, 180, 720, 0.18f, 0.20f, 0.16f, 0); return 1;
    case SFXPACK_BOING * 32 + SND_ORB: trigger(v, W_TRI, 260, 1100, 0.14f, 0.20f, 0.14f, 0); return 1;
    case SFXPACK_BOING * 32 + SND_PAD: trigger(v, W_TRI, 120, 900, 0.25f, 0.24f, 0.22f, 0); return 1;
    case SFXPACK_BOING * 32 + SND_DEATH:
        trigger(v, W_TRI, 700, 90, 0.45f, 0.26f, 0.30f, 0);
        trigger(free_sfx_voice(), W_TRI, 500, 70, 0.40f, 0.14f, 0.30f, 0.12f);
        return 1;
    case SFXPACK_QUACK * 32 + SND_JUMP:
    case SFXPACK_QUACK * 32 + SND_ORB:
    case SFXPACK_QUACK * 32 + SND_PAD:
        trigger(v, W_SQ12, sound == SND_JUMP ? 620 : 760, 330, 0.10f, 0.16f, 0.09f, 0);
        trigger(free_sfx_voice(), W_SQ25, sound == SND_JUMP ? 610 : 750, 320, 0.10f, 0.08f, 0.09f, 0.005f);
        return 1;
    case SFXPACK_QUACK * 32 + SND_DEATH:
        for (int k = 0; k < 3; k++)
            trigger(k ? free_sfx_voice() : v, W_SQ12, 640 - k * 60, 300, 0.10f, 0.16f, 0.08f, (float)k * 0.14f);
        return 1;
    case SFXPACK_CHIP * 32 + SND_JUMP:
        trigger(v, W_SQ25, note_hz[76], 0, 0, 0.09f, 0.03f, 0);
        trigger(free_sfx_voice(), W_SQ25, note_hz[83], 0, 0, 0.09f, 0.04f, 0.03f);
        return 1;
    case SFXPACK_CHIP * 32 + SND_ORB:
    case SFXPACK_CHIP * 32 + SND_PAD:
        for (int k = 0; k < 3; k++)
            trigger(k ? free_sfx_voice() : v, W_SQ12, note_hz[79 + k * 4 + (sound == SND_PAD) * 5], 0, 0, 0.09f, 0.04f,
                    (float)k * 0.03f);
        return 1;
    case SFXPACK_CHIP * 32 + SND_DEATH:
        for (int k = 0; k < 3; k++)
            trigger(k ? free_sfx_voice() : v, W_SQ50, note_hz[67 - k * 5], 0, 0, 0.10f, 0.08f, (float)k * 0.08f);
        return 1;
    case SFXPACK_ROBOT * 32 + SND_JUMP: trigger(v, W_SQ50, 140, 280, 0.06f, 0.12f, 0.07f, 0); return 1;
    case SFXPACK_ROBOT * 32 + SND_ORB:
    case SFXPACK_ROBOT * 32 + SND_PAD:
        trigger(v, W_SQ50, 200, 200, 0, 0.10f, 0.05f, 0);
        trigger(free_sfx_voice(), W_SQ50, 300, 300, 0, 0.10f, 0.05f, 0.06f);
        return 1;
    case SFXPACK_ROBOT * 32 + SND_DEATH:
        trigger(v, W_SQ50, 220, 40, 0.50f, 0.18f, 0.35f, 0);
        trigger(free_sfx_voice(), W_NOISE, 900, 200, 0.40f, 0.12f, 0.25f, 0);
        return 1;
    }
    return 0;
}

static void play_sfx(int sound)
{
    int pack = __atomic_load_n(&req_pack, __ATOMIC_ACQUIRE);
    if (pack > 0 && pack < SFXPACK_COUNT && pack_sfx(pack, sound))
        return;
    Voice *v = free_sfx_voice();
    switch (sound) {
    case SND_JUMP: trigger(v, W_TRI, 520, 780, 0.06f, 0.10f, 0.05f, 0); break;
    case SND_ORB: trigger(v, W_SQ25, 700, 1400, 0.08f, 0.10f, 0.08f, 0); break;
    case SND_PAD: trigger(v, W_TRI, 300, 1000, 0.12f, 0.18f, 0.12f, 0); break;
    case SND_PORTAL: trigger(v, W_SQ12, 1200, 500, 0.15f, 0.10f, 0.15f, 0); break;
    case SND_DEATH:
        trigger(v, W_NOISE, 3000, 400, 0.40f, 0.30f, 0.25f, 0);
        trigger(free_sfx_voice(), W_SQ50, 320, 60, 0.30f, 0.12f, 0.15f, 0);
        break;
    case SND_DONE:
        trigger(&voices[V_SFX], W_SQ25, note_hz[79], 0, 0, 0.12f, 0.30f, 0);
        trigger(&voices[V_SFX + 1], W_SQ25, note_hz[84], 0, 0, 0.12f, 0.30f, 0.10f);
        trigger(&voices[V_SFX + 2], W_SQ25, note_hz[88], 0, 0, 0.12f, 0.50f, 0.20f);
        break;
    case SND_CHECKPOINT: trigger(v, W_TRI, 900, 1300, 0.08f, 0.14f, 0.08f, 0); break;
    case SND_MOVE: trigger(v, W_SQ12, 880, 880, 0, 0.06f, 0.02f, 0); break;
    case SND_SELECT: trigger(v, W_SQ25, 660, 990, 0.07f, 0.08f, 0.06f, 0); break;
    case SND_BACK: trigger(v, W_SQ25, 660, 440, 0.07f, 0.08f, 0.06f, 0); break;
    case SND_PLACE: trigger(v, W_TRI, 600, 600, 0, 0.12f, 0.03f, 0); break;
    case SND_DELETE: trigger(v, W_NOISE, 3000, 1000, 0.05f, 0.08f, 0.03f, 0); break;
    case SND_BADGE:
        trigger(&voices[V_SFX], W_SQ50, note_hz[84], 0, 0, 0.10f, 0.20f, 0);
        trigger(&voices[V_SFX + 1], W_SQ50, note_hz[88], 0, 0, 0.10f, 0.20f, 0.09f);
        trigger(&voices[V_SFX + 2], W_SQ25, note_hz[91], note_hz[96], 0.20f, 0.12f, 0.45f, 0.18f);
        break;
    case SND_MORPH:
        trigger(v, W_SQ25, 300, 1500, 0.16f, 0.09f, 0.10f, 0);
        trigger(free_sfx_voice(), W_TRI, note_hz[88], note_hz[95], 0.08f, 0.07f, 0.10f, 0.10f);
        break;
    case SND_COIN:
        trigger(v, W_SQ25, note_hz[83], 0, 0, 0.11f, 0.05f, 0);
        trigger(free_sfx_voice(), W_SQ25, note_hz[88], 0, 0, 0.11f, 0.22f, 0.07f);
        break;
    // Mode pack B
    case SND_BLAST: trigger(v, W_SQ25, 1900, 260, 0.13f, 0.10f, 0.09f, 0); break;
    case SND_BREAK:
        trigger(v, W_NOISE, 2600, 500, 0.16f, 0.24f, 0.10f, 0);
        trigger(free_sfx_voice(), W_TRI, 220, 70, 0.10f, 0.16f, 0.08f, 0);
        break;
    case SND_BOOM:
        trigger(v, W_NOISE, 1100, 140, 0.30f, 0.34f, 0.20f, 0);
        trigger(free_sfx_voice(), W_TRI, 130, 40, 0.25f, 0.30f, 0.18f, 0);
        break;
    case SND_TICK: trigger(v, W_SQ12, 2400, 2400, 0, 0.07f, 0.012f, 0); break;
    case SND_SPLAT:
        trigger(v, W_TRI, 420, 110, 0.09f, 0.16f, 0.08f, 0);
        trigger(free_sfx_voice(), W_NOISE, 900, 300, 0.06f, 0.08f, 0.04f, 0);
        break;
    case SND_SWAP:
        trigger(v, W_SQ25, 520, 1040, 0.05f, 0.09f, 0.05f, 0);
        trigger(free_sfx_voice(), W_TRI, 1040, 780, 0.05f, 0.08f, 0.06f, 0.05f);
        break;
    case SND_WHOO: trigger(v, W_TRI, 330, 660, 0.22f, 0.09f, 0.16f, 0); break;
    case SND_EGG:
        trigger(v, W_NOISE, 5000, 2000, 0.04f, 0.16f, 0.03f, 0);
        trigger(free_sfx_voice(), W_NOISE, 4000, 1500, 0.04f, 0.14f, 0.03f, 0.09f);
        trigger(free_sfx_voice(), W_SQ25, note_hz[84], note_hz[96], 0.25f, 0.11f, 0.40f, 0.22f);
        break;
    }
}

// A song maker preview: (kind << 12) | detail, see synth_tone.
static void play_tone(int t)
{
    static const int LEAD_WAVE[3] = { W_SQ25, W_TRI, W_SQ50 };
    int kind = t >> 12, note = t & 0x3FF;
    Voice *v = &voices[V_SFX];
    if (kind == 1) {
        if (t & DR_KICK)
            trigger(free_sfx_voice(), W_TRI, 160.0f, 45.0f, 0.09f, 0.55f, 0.10f, 0);
        if (t & DR_SNARE)
            trigger(free_sfx_voice(), W_NOISE, 6000.0f, 0, 0, 0.18f, 0.09f, 0);
        if (t & DR_HAT)
            trigger(free_sfx_voice(), W_NOISE, 12000.0f, 0, 0, 0.06f, 0.02f, 0);
        return;
    }
    if (note < 0 || note > 127)
        return;
    if (kind == 2 + TRACK_LEAD)
        trigger(v, LEAD_WAVE[((t >> 10) & 3) % 3], note_hz[note], 0, 0, 0.16f, 0.40f, 0);
    else if (kind == 2 + TRACK_SPARKLE)
        trigger(v, W_SQ12, note_hz[note], 0, 0, 0.10f, 0.15f, 0);
    else
        trigger(v, W_TRI, note_hz[note], 0, 0, 0.32f, 0.25f, 0);
}

static float gain(int v)
{
    float g = (float)(v < 0 ? 0 : v > 10 ? 10 : v) / 10.0f;
    return g * g;
}

void synth_render(int16_t *out, int frames)
{
    if (!rate) {
        memset(out, 0, sizeof(int16_t) * 2 * (size_t)frames);
        return;
    }
    for (int i = 0; i < USER_SLOTS; i++) {
        int us = __atomic_load_n(&user_seq[i], __ATOMIC_ACQUIRE);
        if (us != user_seen[i]) {
            user_seen[i] = us;
            expand_user(&pending[i], &songs[SONG_USER + i]);
            if (cur_song == SONG_USER + i)
                step_len = (float)rate * 60.0f / ((float)songs[cur_song].bpm * 4.0f);
        }
    }
    int seq = __atomic_load_n(&req_seq, __ATOMIC_ACQUIRE);
    if (seq != seen_seq) {
        seen_seq = seq;
        int s = __atomic_load_n(&req_song, __ATOMIC_RELAXED);
        cur_song = (s >= 0 && s < ALL_SONGS) ? s : -1;
        step = 0;
        step_left = 0;
        for (int i = 0; i < V_SFX; i++)
            voices[i].amp = 0;
        if (cur_song >= 0) {
            step_len = (float)rate * 60.0f / ((float)songs[cur_song].bpm * 4.0f);
            // Part way in: whole 16ths from the frames asked for.
            int start = __atomic_load_n(&req_start, __ATOMIC_RELAXED);
            step = (int)((float)start / 60.0f * (float)songs[cur_song].bpm * 4.0f / 60.0f);
        }
    }
    int tone = __atomic_exchange_n(&req_tone, 0, __ATOMIC_ACQ_REL);
    if (tone)
        play_tone(tone);
    paused = __atomic_load_n(&req_pause, __ATOMIC_ACQUIRE);
    music_gain = gain(__atomic_load_n(&req_music, __ATOMIC_ACQUIRE));
    sfx_gain = gain(__atomic_load_n(&req_fx, __ATOMIC_ACQUIRE));
    uint32_t fx = __atomic_exchange_n(&req_sfx, 0u, __ATOMIC_ACQ_REL);
    for (int i = 0; i <= SND_LAST; i++)
        if (fx & (1u << i))
            play_sfx(i);

    for (int f = 0; f < frames; f++) {
        float l = 0, r = 0;
        if (cur_song >= 0 && !paused) {
            step_left -= 1.0f;
            if (step_left <= 0) {
                song_step();
                step_left += step_len;
            }
            float lead = voice_sample(&voices[V_LEAD]), arp = voice_sample(&voices[V_ARP]);
            float rest = voice_sample(&voices[V_BASS]) + voice_sample(&voices[V_KICK]) + voice_sample(&voices[V_NOISE]);
            l += (lead + arp * 1.3f + rest) * music_gain;
            r += (lead + arp * 0.7f + rest) * music_gain;
        }
        float e = 0;
        for (int i = V_SFX; i < NVOICES; i++)
            e += voice_sample(&voices[i]);
        l += e * sfx_gain;
        r += e * sfx_gain;
        int il = (int)(l * 26000.0f), ir = (int)(r * 26000.0f);
        out[2 * f] = (int16_t)(il > 32767 ? 32767 : il < -32768 ? -32768 : il);
        out[2 * f + 1] = (int16_t)(ir > 32767 ? 32767 : ir < -32768 ? -32768 : ir);
    }
}
