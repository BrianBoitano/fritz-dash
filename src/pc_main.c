// Fritz Dash - Windows entry point (SDL2): the window, pads and keyboard, the
// audio callback, 60 game steps a second on any monitor, and the files and
// directory listing the game asks for (platform.h), kept in %APPDATA%\FritzDash.
// It mirrors ps4_main.c; README.md "Windows (PC)" has the controls and the
// --selftest switch.
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <windows.h>
#include <shlobj.h>
#undef RGB      // wingdi.h's; draw.h has the game's own

#include <SDL.h>

#include "draw.h"
#include "game.h"
#include "platform.h"
#include "synth.h"

#ifndef FRITZ_VERSION
#define FRITZ_VERSION "03.20"
#endif

#define WPATH 1024
#define WINDOW_W 1280
#define WINDOW_H 720

// --- Files: DATA_DIR is %APPDATA%\FritzDash --------------------------------
// Paths are UTF-8 in the game and UTF-16 for Windows, so a user folder with
// spaces or accents works.

static wchar_t data_root[WPATH];
static char data_root_utf8[WPATH * 3];
static wchar_t log_path[WPATH];
static int log_lines;
static const char *test_dir;     // --selftest output folder, or NULL

static int to_wide(const char *s, wchar_t *out, int n)
{
    return n > 0 && MultiByteToWideChar(CP_UTF8, 0, s, -1, out, n) > 0;
}

static int to_utf8(const wchar_t *s, char *out, int n)
{
    return n > 0 && WideCharToMultiByte(CP_UTF8, 0, s, -1, out, n, NULL, NULL) > 0;
}

// A game path as a Windows one: DATA_DIR becomes data_root and / becomes \.
static int map_path(const char *path, wchar_t *out, int n)
{
    size_t k = strlen(DATA_DIR);
    int used = 0;
    if (!strncmp(path, DATA_DIR, k) && (path[k] == 0 || path[k] == '/')) {
        used = (int)wcslen(data_root);
        if (used >= n)
            return 0;
        memcpy(out, data_root, sizeof(wchar_t) * (size_t)used);
        path += k;
    }
    if (!to_wide(path, out + used, n - used))
        return 0;
    for (wchar_t *p = out; *p; p++)
        if (*p == L'/')
            *p = L'\\';
    return 1;
}

// %APPDATA% (C:\Users\<name>\AppData\Roaming), else the known folder, else
// the exe's own folder.
static void find_data_root(void)
{
    const wchar_t *env = _wgetenv(L"APPDATA");
    PWSTR known = NULL;
    wchar_t base[WPATH] = L".";
    if (env && env[0] && wcslen(env) < WPATH - 16) {
        wcscpy(base, env);
    } else if (SHGetKnownFolderPath(&FOLDERID_RoamingAppData, 0, NULL, &known) == S_OK && wcslen(known) < WPATH - 16) {
        wcscpy(base, known);
    } else {
        DWORD n = GetModuleFileNameW(NULL, base, WPATH - 16);
        while (n > 0 && base[n - 1] != L'\\')
            n--;
        if (n > 1)
            base[n - 1] = 0;
        else
            wcscpy(base, L".");
    }
    CoTaskMemFree(known);
    wcscpy(data_root, base);
    wcscat(data_root, L"\\FritzDash");
    to_utf8(data_root, data_root_utf8, (int)sizeof(data_root_utf8));
}

static void make_dir(const char *dir)
{
    wchar_t p[WPATH];
    if (map_path(dir, p, WPATH))
        CreateDirectoryW(p, NULL);  // fine if it already exists
}

int plat_read_file(const char *path, char *buf, int max)
{
    wchar_t p[WPATH];
    if (!map_path(path, p, WPATH))
        return -1;
    FILE *f = _wfopen(p, L"rb");
    if (!f)
        return -1;
    size_t n = fread(buf, 1, (size_t)max, f);
    fclose(f);
    return (int)n;
}

int plat_write_file(const char *path, const char *buf, int len)
{
    wchar_t p[WPATH];
    if (!map_path(path, p, WPATH))
        return -1;
    FILE *f = _wfopen(p, L"wb");
    if (!f)
        return -1;
    size_t n = fwrite(buf, 1, (size_t)len, f);
    // fclose flushes the buffer, so a full disk can first show up here.
    int closed = fclose(f);
    return n == (size_t)len && closed == 0 ? 0 : -1;
}

int plat_remove_file(const char *path)
{
    wchar_t p[WPATH];
    if (!map_path(path, p, WPATH))
        return -1;
    return _wremove(p) == 0 ? 0 : -1;
}

// Every entry, "." and ".." included, as readdir gives them on the PS4.
int plat_list_dir(const char *dir, void (*fn)(const char *name, void *ctx), void *ctx)
{
    wchar_t p[WPATH];
    if (!map_path(dir, p, WPATH - 2))
        return -1;
    wcscat(p, L"\\*");
    WIN32_FIND_DATAW e;
    HANDLE h = FindFirstFileW(p, &e);
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    do {
        char name[MAX_PATH * 3];
        if (to_utf8(e.cFileName, name, (int)sizeof(name)))
            fn(name, ctx);
    } while (FindNextFileW(h, &e));
    FindClose(h);
    return 0;
}

// --- Log: %APPDATA%\FritzDash\log.txt --------------------------------------
// How start-up went, to read when something is wrong on a PC far away. Starts
// over once it passes 64 KB, and takes at most 200 lines a session.

static void log_line(const char *fmt, ...)
{
    if (!log_path[0] || log_lines >= 200)
        return;
    FILE *f = _wfopen(log_path, L"ab");
    if (!f)
        return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d  ", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputs("\r\n", f);
    fclose(f);
    log_lines++;
}

static void log_open(void)
{
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!map_path(DATA_DIR "/log.txt", log_path, WPATH)) {
        log_path[0] = 0;
        return;
    }
    if (GetFileAttributesExW(log_path, GetFileExInfoStandard, &a) && (a.nFileSizeHigh || a.nFileSizeLow > 64 * 1024))
        _wremove(log_path);
}

// Logs what failed and says so in a message box (not in a selftest, which
// runs with nobody to click it away).
static int fail(const char *what)
{
    char msg[512];
    snprintf(msg, sizeof(msg), "%s: %s", what, SDL_GetError());
    log_line("FAILED %s", msg);
    if (!test_dir)
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fritz Dash", msg, NULL);
    SDL_Quit();
    return 1;
}

// --- Video -----------------------------------------------------------------
// The game draws 1920x1080 into framebuffer; one streaming texture shows it,
// scaled to the window with black bars where the shape differs.

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_RendererInfo renderer_info;
static int fullscreen = 1, vsync;
static uint32_t framebuffer[SCREEN_W * SCREEN_H];

static void texture_make(void)
{
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
    // The game leaves the top byte 0, so the texture is copied, not blended.
    if (texture)
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
}

static int renderer_try(Uint32 flags)
{
    renderer = SDL_CreateRenderer(window, -1, flags);
    if (renderer)
        texture_make();
    if (!texture) {
        log_line("renderer with flags %u failed: %s", (unsigned)flags, SDL_GetError());
        if (renderer)
            SDL_DestroyRenderer(renderer);
        renderer = NULL;
        return -1;
    }
    SDL_RenderSetLogicalSize(renderer, SCREEN_W, SCREEN_H);
    SDL_GetRendererInfo(renderer, &renderer_info);
    vsync = (renderer_info.flags & SDL_RENDERER_PRESENTVSYNC) != 0;
    return 0;
}

static int display_hz(void)
{
    SDL_DisplayMode m;
    if (SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window), &m) != 0)
        return 0;
    return m.refresh_rate;
}

static int video_init(void)
{
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    window = SDL_CreateWindow("Fritz Dash", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!window)
        return -1;
    SDL_SetWindowMinimumSize(window, 320, 180);
    SDL_ShowCursor(SDL_DISABLE);
    if (renderer_try(SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) != 0 &&
        renderer_try(SDL_RENDERER_ACCELERATED) != 0 && renderer_try(SDL_RENDERER_SOFTWARE) != 0)
        return -2;
    int w = 0, h = 0;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    log_line("video: driver %s, renderer %s, vsync %s, output %dx%d, display %d Hz", SDL_GetCurrentVideoDriver(),
             renderer_info.name, vsync ? "on" : "off", w, h, display_hz());
    return 0;
}

// F11 and Alt+Enter: windowed is 1280x720 and can be resized; fullscreen
// covers the desktop and hides the mouse.
static void set_fullscreen(int on)
{
    fullscreen = on;
    SDL_SetWindowFullscreen(window, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    if (!on) {
        SDL_SetWindowSize(window, WINDOW_W, WINDOW_H);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    SDL_ShowCursor(on ? SDL_DISABLE : SDL_ENABLE);
}

// --- Controllers and keyboard ----------------------------------------------
// PS4 and Xbox pads both come through SDL_GameController, in Xbox names:
// A is Cross, B Circle, X Square, Y Triangle. A pad keeps its slot while it is
// plugged in; the race players are in race_bits.

#define MAX_PADS 4
// The PS4 sticks count past 58 of 128 from the middle; the same here.
#define STICK_PRESS 14848
#define TRIGGER_PRESS 10923      // a third of the way down

static SDL_GameController *pads[MAX_PADS];
static SDL_JoystickID pad_ids[MAX_PADS];
static int npads;

static void pad_added(int device)
{
    SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(device);
    for (int i = 0; i < MAX_PADS; i++)
        if (pads[i] && pad_ids[i] == id)
            return;
    for (int i = 0; i < MAX_PADS; i++) {
        if (pads[i])
            continue;
        pads[i] = SDL_GameControllerOpen(device);
        if (!pads[i]) {
            log_line("pad %d did not open: %s", device, SDL_GetError());
            return;
        }
        pad_ids[i] = id;
        npads++;
        log_line("pad %d connected: %s", i + 1, SDL_GameControllerName(pads[i]));
        return;
    }
}

static void pad_removed(SDL_JoystickID id)
{
    for (int i = 0; i < MAX_PADS; i++) {
        if (!pads[i] || pad_ids[i] != id)
            continue;
        SDL_GameControllerClose(pads[i]);
        pads[i] = NULL;
        npads--;
        log_line("pad %d disconnected", i + 1);
    }
}

static int pad_bits(SDL_GameController *c)
{
    static const struct { SDL_GameControllerButton button; int bit; } BUTTONS[] = {
        { SDL_CONTROLLER_BUTTON_A, IN_CROSS },          { SDL_CONTROLLER_BUTTON_B, IN_CIRCLE },
        { SDL_CONTROLLER_BUTTON_X, IN_SQUARE },         { SDL_CONTROLLER_BUTTON_Y, IN_TRIANGLE },
        { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, IN_L1 },  { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, IN_R1 },
        { SDL_CONTROLLER_BUTTON_START, IN_START },      { SDL_CONTROLLER_BUTTON_BACK, IN_TOUCH },
        { SDL_CONTROLLER_BUTTON_TOUCHPAD, IN_TOUCH },   { SDL_CONTROLLER_BUTTON_LEFTSTICK, IN_L3 },
        { SDL_CONTROLLER_BUTTON_DPAD_UP, IN_UP },       { SDL_CONTROLLER_BUTTON_DPAD_DOWN, IN_DOWN },
        { SDL_CONTROLLER_BUTTON_DPAD_LEFT, IN_LEFT },   { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, IN_RIGHT },
    };
    int bits = 0;
    for (size_t i = 0; i < sizeof(BUTTONS) / sizeof(BUTTONS[0]); i++)
        if (SDL_GameControllerGetButton(c, BUTTONS[i].button))
            bits |= BUTTONS[i].bit;
    if (SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > TRIGGER_PRESS) bits |= IN_L2;
    if (SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > TRIGGER_PRESS) bits |= IN_R2;
    int lx = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX);
    int ly = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
    int rx = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTX);
    int ry = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTY);
    if (lx < -STICK_PRESS) bits |= IN_LEFT;
    if (lx > STICK_PRESS) bits |= IN_RIGHT;
    if (ly < -STICK_PRESS) bits |= IN_UP;
    if (ly > STICK_PRESS) bits |= IN_DOWN;
    if (rx < -STICK_PRESS) bits |= IN_RS_LEFT;
    if (rx > STICK_PRESS) bits |= IN_RS_RIGHT;
    if (ry < -STICK_PRESS) bits |= IN_RS_UP;
    if (ry > STICK_PRESS) bits |= IN_RS_DOWN;
    return bits;
}

// The keyboard is player 1's controller (README.md has the table).
static int key_bits(void)
{
    static const struct { SDL_Scancode key; int bit; } KEYS[] = {
        { SDL_SCANCODE_UP, IN_UP },          { SDL_SCANCODE_DOWN, IN_DOWN },
        { SDL_SCANCODE_LEFT, IN_LEFT },      { SDL_SCANCODE_RIGHT, IN_RIGHT },
        { SDL_SCANCODE_SPACE, IN_CROSS },    { SDL_SCANCODE_Z, IN_CROSS },
        { SDL_SCANCODE_X, IN_CIRCLE },       { SDL_SCANCODE_BACKSPACE, IN_CIRCLE },
        { SDL_SCANCODE_C, IN_SQUARE },       { SDL_SCANCODE_V, IN_TRIANGLE },
        { SDL_SCANCODE_Q, IN_L1 },           { SDL_SCANCODE_E, IN_R1 },
        { SDL_SCANCODE_1, IN_L2 },           { SDL_SCANCODE_3, IN_R2 },
        { SDL_SCANCODE_TAB, IN_TOUCH },      { SDL_SCANCODE_ESCAPE, IN_START },
        { SDL_SCANCODE_RETURN, IN_START },   { SDL_SCANCODE_F, IN_L3 },
        { SDL_SCANCODE_I, IN_RS_UP },        { SDL_SCANCODE_J, IN_RS_LEFT },
        { SDL_SCANCODE_K, IN_RS_DOWN },      { SDL_SCANCODE_L, IN_RS_RIGHT },
    };
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    // Alt+Enter is fullscreen and Alt+Tab another window, not Options or Touchpad.
    int alt = k[SDL_SCANCODE_LALT] || k[SDL_SCANCODE_RALT];
    int bits = 0;
    for (size_t i = 0; i < sizeof(KEYS) / sizeof(KEYS[0]); i++)
        if (k[KEYS[i].key] && !(alt && (KEYS[i].key == SDL_SCANCODE_RETURN || KEYS[i].key == SDL_SCANCODE_TAB)))
            bits |= KEYS[i].bit;
    return bits;
}

// The 2 player race needs each player's X on its own. With two controllers
// they are players 1 and 2 (the keyboard joins player 1); with one controller
// the keyboard is player 2; with none the keyboard is player 1 alone.
static int race_bits(int keys, int npad, int first, int second)
{
    int input = 0;
    if (npad >= 2) {
        if ((keys | first) & IN_CROSS)
            input |= IN_P1_CROSS;
        if (second & IN_CROSS)
            input |= IN_P2_CROSS;
        input |= IN_P2_HERE;
    } else if (npad == 1) {
        if (first & IN_CROSS)
            input |= IN_P1_CROSS;
        if (keys & IN_CROSS)
            input |= IN_P2_CROSS;
        input |= IN_P2_HERE;
    } else if (keys & IN_CROSS) {
        input |= IN_P1_CROSS;
    }
    return input;
}

// See game.h for what each input bit does.
static int read_input(void)
{
    int keys = key_bits(), input = keys, n = 0, first = 0, second = 0;
    for (int i = 0; i < MAX_PADS; i++) {
        if (!pads[i])
            continue;
        int bits = pad_bits(pads[i]);
        input |= bits;
        if (n == 0)
            first = bits;
        else if (n == 1)
            second = bits;
        n++;
    }
    return input | race_bits(keys, n, first, second);
}

// --- Audio -----------------------------------------------------------------
// SDL pulls 48 kHz stereo from the synth on its own thread, as the PS4 audio
// thread does. Without a sound device the game runs silently.

static SDL_AudioDeviceID audio;
static char audio_name[16] = "OFF";
static volatile long audio_frames;   // frames rendered, for the selftest

static void SDLCALL audio_callback(void *user, Uint8 *stream, int len)
{
    (void)user;
    synth_render((int16_t *)stream, len / 4);
    InterlockedExchangeAdd(&audio_frames, len / 4);
}

static void audio_init(void)
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        log_line("audio: no audio system (%s), playing silently", SDL_GetError());
        return;
    }
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = audio_callback;
    audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!audio) {
        log_line("audio: driver %s has no device (%s), playing silently", SDL_GetCurrentAudioDriver(), SDL_GetError());
        return;
    }
    synth_init(48000);
    SDL_PauseAudioDevice(audio, 0);
    snprintf(audio_name, sizeof(audio_name), "%s", SDL_GetCurrentAudioDriver());
    char *dev = NULL;
    SDL_AudioSpec spec;
    if (SDL_GetDefaultAudioInfo(&dev, &spec, 0) != 0)
        dev = NULL;
    log_line("audio: driver %s, device %s, %d Hz, %d channels, %d samples", audio_name, dev ? dev : "(default)",
             have.freq, have.channels, have.samples);
    SDL_free(dev);
}

// --- Clock and diagnostic line ---------------------------------------------

// Local month, day and hour for the easter eggs' holidays.
static void update_clock(void)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    eggs_set_clock(t.wMonth, t.wDay, t.wHour);
}

// Shown on the title while L1+R1 (Q+E) are held.
// PADS:<count> BTN:<input bits> AUD:<audio driver or OFF> REN:<renderer>
// VS:<vsync> HZ:<display refresh> SPS:<game steps last second, 60 when right>
static void update_diag(int input, int hz, int sps)
{
    char line[128];
    snprintf(line, sizeof(line), "PADS:%d BTN:%08X AUD:%s REN:%s VS:%d HZ:%d SPS:%d", npads, (unsigned)input,
             audio_name, renderer_info.name, vsync, hz, sps);
    game_set_diag(line);
}

// --- Selftest --------------------------------------------------------------
// FritzDash.exe --selftest <folder>: scripted presses take the title into
// WORLD 1's first level and hop through it for a few seconds; screenshots of
// the game's own framebuffer go to the folder as BMP, with selftest.txt. The
// exit code is 0 when every screen was the one expected and every file was
// written.

#define TEST_STEPS 600

static const struct { int step; const char *name; int state; } TEST_SHOTS[] = {
    { 90, "1_title", ST_TITLE },   { 140, "2_worlds", ST_LEVELS }, { 190, "3_levels", ST_LEVELS },
    { 330, "4_play", ST_PLAY },    { 560, "5_play", ST_PLAY },
};
#define TEST_NSHOTS ((int)(sizeof(TEST_SHOTS) / sizeof(TEST_SHOTS[0])))

static int test_input(int step)
{
    // X on LEVELS, on the WORLD 1 card and on its first level, then hops.
    if (step == 100 || step == 150 || step == 200)
        return IN_CROSS;
    return step > 200 && step % 45 < 12 ? IN_CROSS : 0;
}

static int test_shot(const char *name)
{
    char path[WPATH * 3];
    snprintf(path, sizeof(path), "%s\\%s.bmp", test_dir, name);
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(framebuffer, SCREEN_W, SCREEN_H, 32, SCREEN_W * 4,
                                                        SDL_PIXELFORMAT_RGB888);
    int rc = s ? SDL_SaveBMP(s, path) : -1;
    SDL_FreeSurface(s);
    return rc;
}

typedef struct {
    FILE *f;
    int count;
    int found_probe;
} TestList;

static void test_list_entry(const char *name, void *ctx)
{
    TestList *t = ctx;
    char path[512], buf[1 << 16];
    t->count++;
    if (!strcmp(name, "probe \xc3\xbc.txt"))
        t->found_probe = 1;
    snprintf(path, sizeof(path), "%s/%s", DATA_DIR, name);
    fprintf(t->f, "  %-16s read %d bytes\n", name, plat_read_file(path, buf, (int)sizeof(buf)));
}

static int test_report(int steps, double secs, const int *states, const int *saved)
{
    char path[WPATH * 3];
    wchar_t wpath[WPATH];
    snprintf(path, sizeof(path), "%s\\selftest.txt", test_dir);
    if (!to_wide(path, wpath, WPATH))
        return 1;
    FILE *f = _wfopen(wpath, L"wb");
    if (!f)
        return 1;
    int ok = 1;
    fprintf(f, "Fritz Dash %s selftest\n", FRITZ_VERSION);
    fprintf(f, "steps %d in %.2f s (%.1f steps/s)\n", steps, secs, secs > 0 ? steps / secs : 0.0);
    fprintf(f, "renderer %s, vsync %d, display %d Hz\n", renderer_info.name, vsync, display_hz());
    fprintf(f, "audio %s, %ld frames rendered by the synth\n", audio_name, (long)audio_frames);
    fprintf(f, "pads %d\n", npads);
    fprintf(f, "data dir %s\n", data_root_utf8);
    for (int i = 0; i < TEST_NSHOTS; i++) {
        int good = saved[i] && states[i] == TEST_SHOTS[i].state;
        ok &= good;
        fprintf(f, "%s.bmp at step %d: state %d (want %d), %s -> %s\n", TEST_SHOTS[i].name, TEST_SHOTS[i].step,
                states[i], TEST_SHOTS[i].state, saved[i] ? "written" : "NOT WRITTEN", good ? "ok" : "FAIL");
    }
    // The platform layer round trip: write, list, read back and remove a file
    // whose name is not plain ASCII.
    static const char probe[] = "fritz dash probe\n";
    char back[64];
    int wrote = plat_write_file(DATA_DIR "/probe \xc3\xbc.txt", probe, (int)sizeof(probe) - 1) == 0;
    TestList t = { f, 0, 0 };
    fprintf(f, "%s via plat_list_dir:\n", DATA_DIR);
    int listed = plat_list_dir(DATA_DIR, test_list_entry, &t) == 0;
    int n = plat_read_file(DATA_DIR "/probe \xc3\xbc.txt", back, (int)sizeof(back));
    int same = n == (int)sizeof(probe) - 1 && !memcmp(back, probe, (size_t)n);
    int removed = plat_remove_file(DATA_DIR "/probe \xc3\xbc.txt") == 0;
    int gone = plat_read_file(DATA_DIR "/probe \xc3\xbc.txt", back, (int)sizeof(back)) < 0;
    int round = wrote && listed && t.found_probe && same && removed && gone;
    ok &= round;
    fprintf(f, "file round trip: write %d, listed %d, read back %d, remove %d -> %s\n", wrote, t.found_probe, same,
            removed && gone, round ? "ok" : "FAIL");
    // Who is which race player: keyboard alone, keyboard + one pad, two pads.
    int race = race_bits(IN_CROSS, 0, 0, 0) == IN_P1_CROSS &&
               race_bits(IN_CROSS, 1, 0, 0) == (IN_P2_CROSS | IN_P2_HERE) &&
               race_bits(0, 1, IN_CROSS, 0) == (IN_P1_CROSS | IN_P2_HERE) &&
               race_bits(IN_CROSS, 2, 0, 0) == (IN_P1_CROSS | IN_P2_HERE) &&
               race_bits(0, 2, 0, IN_CROSS) == (IN_P2_CROSS | IN_P2_HERE) &&
               race_bits(0, 0, 0, 0) == 0;
    ok &= race;
    fprintf(f, "race players (keyboard, keyboard + 1 pad, 2 pads): %s\n", race ? "ok" : "FAIL");
    fprintf(f, "RESULT %s\n", ok ? "PASS" : "FAIL");
    fclose(f);
    log_line("selftest %s: %d steps in %.2f s", ok ? "PASS" : "FAIL", steps, secs);
    return ok ? 0 : 1;
}

// --- Main loop -------------------------------------------------------------

static void poll_events(int *running, int *redraw)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:      // window closed or Alt+F4; Esc is pause, never quit
            *running = 0;
            break;
        case SDL_CONTROLLERDEVICEADDED:
            pad_added(e.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            pad_removed(e.cdevice.which);
            break;
        case SDL_KEYDOWN:
            if (!e.key.repeat && (e.key.keysym.scancode == SDL_SCANCODE_F11 ||
                                  (e.key.keysym.scancode == SDL_SCANCODE_RETURN && (e.key.keysym.mod & KMOD_ALT))))
                set_fullscreen(!fullscreen);
            break;
        case SDL_RENDER_DEVICE_RESET:   // the GPU went away and came back: textures are gone
            log_line("render device reset");
            if (texture)
                SDL_DestroyTexture(texture);
            texture_make();
            *redraw = 1;
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--selftest"))
            test_dir = i + 1 < argc ? argv[i + 1] : "";

    find_data_root();
    CreateDirectoryW(data_root, NULL);
    make_dir(LEVELS_DIR);
    make_dir(GHOSTS_DIR);
    make_dir(SONGS_DIR);
    log_open();

    static char test_default[WPATH * 3];
    if (test_dir && !test_dir[0]) {
        snprintf(test_default, sizeof(test_default), "%s\\selftest", data_root_utf8);
        test_dir = test_default;
    }
    if (test_dir) {
        wchar_t w[WPATH], full[WPATH];
        if (to_wide(test_dir, w, WPATH) && GetFullPathNameW(w, WPATH, full, NULL) > 0)
            SHCreateDirectoryExW(NULL, full, NULL);
    }

    SDL_version built, linked;
    SDL_VERSION(&built);
    SDL_GetVersion(&linked);
    log_line("Fritz Dash %s starting%s; SDL %d.%d.%d (built with %d.%d.%d); data %s", FRITZ_VERSION,
             test_dir ? " a selftest" : "", linked.major, linked.minor, linked.patch, built.major, built.minor,
             built.patch, data_root_utf8);

    // Sharp on scaled (high DPI) displays instead of stretched by Windows.
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return fail("SDL video init");
    if (video_init() != 0)
        return fail("window and renderer");
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0)
        log_line("controllers unavailable: %s", SDL_GetError());
    else
        log_line("controllers: %d found at start", SDL_NumJoysticks());
    audio_init();

    static Game game;
    game_init(&game, (uint32_t)(SDL_GetPerformanceCounter() * 2654435761u) | 1u);

    // Fixed 60 game steps a second, whatever the monitor: the counter's time
    // is banked (times 60, so a step is exactly freq) and spent a step at a
    // time, at most 4 per frame; the frame is drawn once after them.
    const Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 last = SDL_GetPerformanceCounter(), start = last, bank = 0, sec_start = last;
    int running = 1, redraw = 1, steps = 0, sec_steps = 0, sps = 0, hz = display_hz(), input = 0, shot = 0;
    int states[TEST_NSHOTS] = { 0 }, saved[TEST_NSHOTS] = { 0 };
    while (running) {
        Uint64 frame_start = SDL_GetPerformanceCounter();
        poll_events(&running, &redraw);
        input = read_input();

        Uint64 now = SDL_GetPerformanceCounter();
        Uint64 d = (now - last) * 60;
        last = now;
        // A 60 Hz vsync frame counts as exactly one step, not 0 or 2 from jitter.
        if (d > freq - freq / 50 && d < freq + freq / 50)
            d = freq;
        bank += d;
        int n = 0;
        while (bank >= freq && n < 4 && !(test_dir && steps >= TEST_STEPS)) {
            if (steps % 600 == 0)
                update_clock();
            game_step(&game, test_dir ? test_input(steps) : input);
            bank -= freq;
            steps++;
            n++;
        }
        if (bank >= freq)
            bank %= freq;   // too far behind (a stall): drop the backlog

        if (now - sec_start >= freq) {
            sps = steps - sec_steps;
            sec_steps = steps;
            sec_start = now;
            hz = display_hz();
        }
        if (n > 0 || redraw) {
            update_diag(input, hz, sps);
            draw_set_target(framebuffer);
            game_draw(&game);
            if (texture)
                SDL_UpdateTexture(texture, NULL, framebuffer, SCREEN_W * 4);
            redraw = 0;
        }
        while (test_dir && shot < TEST_NSHOTS && steps >= TEST_SHOTS[shot].step) {
            states[shot] = game.state;
            saved[shot] = test_shot(TEST_SHOTS[shot].name) == 0;
            shot++;
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        if (texture)
            SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        if (test_dir && steps >= TEST_STEPS)
            break;
        // Without vsync, or when it does not wait (a minimised window), sleep
        // until the next step is due.
        Uint64 after = SDL_GetPerformanceCounter();
        if (!vsync || (after - frame_start) * 1000 < freq) {
            Uint64 due = (freq - bank) / 60, spent = after - now;
            if (due > spent && (due - spent) * 1000 / freq > 0)
                SDL_Delay((Uint32)((due - spent) * 1000 / freq));
        }
    }

    int rc = 0;
    if (test_dir)
        rc = test_report(steps, (double)(SDL_GetPerformanceCounter() - start) / (double)freq, states, saved);
    else
        log_line("quit after %d steps", steps);
    if (audio)
        SDL_CloseAudioDevice(audio);
    for (int i = 0; i < MAX_PADS; i++)
        if (pads[i])
            SDL_GameControllerClose(pads[i]);
    SDL_Quit();
    return rc;
}
