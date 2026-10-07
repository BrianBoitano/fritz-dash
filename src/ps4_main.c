// Fritz Dash - PS4 entry point: video out, pad input, audio thread, timing,
// and the files and directory listing the game asks for (platform.h).
// Video and pad setup follow the OpenOrbis graphics and input samples.
#include <dirent.h>
#include <stdio.h>
#include <stdint.h>

#include <orbis/libkernel.h>
#include <orbis/AudioOut.h>
#include <orbis/VideoOut.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/SystemService.h>

#include "draw.h"
#include "game.h"
#include "platform.h"
#include "synth.h"

static int video = -1;
static OrbisKernelEqueue flip_queue;
static uint32_t *framebuffers[2];

static int video_init(void)
{
    video = sceVideoOutOpen(ORBIS_VIDEO_USER_MAIN, ORBIS_VIDEO_OUT_BUS_MAIN, 0, 0);
    if (video < 0)
        return -1;
    if (sceKernelCreateEqueue(&flip_queue, "fritzdash flip queue") < 0)
        return -2;
    sceVideoOutAddFlipEvent(flip_queue, video, 0);

    // Two framebuffers, each rounded up to a 2 MB boundary.
    const size_t align = 0x200000;
    size_t fb_size = (size_t)SCREEN_W * SCREEN_H * 4;
    size_t fb_stride = (fb_size + align - 1) / align * align;
    size_t total = fb_stride * 2;
    off_t phys = 0;
    void *mem = NULL;
    if (sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), total, align, 3, &phys) < 0)
        return -3;
    if (sceKernelMapDirectMemory(&mem, total, 0x33, 0, phys, align) < 0)
        return -4;
    framebuffers[0] = (uint32_t *)mem;
    framebuffers[1] = (uint32_t *)((char *)mem + fb_stride);

    // A8R8G8B8 sRGB, linear tiling, pitch = width (as in the sample).
    OrbisVideoOutBufferAttribute attr;
    sceVideoOutSetBufferAttribute(&attr, 0x80000000, 1, 0, SCREEN_W, SCREEN_H, SCREEN_W);
    if (sceVideoOutRegisterBuffers(video, 0, (void **)framebuffers, 2, &attr) != 0)
        return -5;
    sceVideoOutSetFlipRate(video, 0);
    return 0;
}

static void wait_for_flip(int64_t frame_id)
{
    for (;;) {
        OrbisVideoOutFlipStatus status;
        OrbisKernelEvent evt;
        int count;
        sceVideoOutGetFlipStatus(video, &status);
        if (status.flipArg == frame_id)
            break;
        if (sceKernelWaitEqueue(flip_queue, &evt, 1, &count, 0) != 0)
            break;
    }
}

// --- Controller ------------------------------------------------------------
// User IDs are opaque 32-bit values (any bit pattern but -1 is valid), so
// "have we got one" is tracked with flags, never with the sign.

#define MAX_PADS 4

static int pads[MAX_PADS];
static int32_t pad_users[MAX_PADS];
static int npads;

static int32_t initial_user;
static int have_initial_user;

// Shown on the title screen so a failure can be read off the TV.
static int32_t last_rc;          // last error from user service / pad calls
// The local month, day and hour for the easter eggs' holidays: the kernel's
// UTC clock turned into local time (Pacific time if that fails).
static void update_clock(void)
{
    OrbisKernelTimeval tv;
    if (sceKernelGettimeofday(&tv) != 0)
        return;
    int64_t utc = (int64_t)tv.tv_sec, local = utc - 8 * 3600, dst = 0;
    char tz[16];
    int (*to_local)(int64_t, int64_t *, void *, int64_t *) =
        (int (*)(int64_t, int64_t *, void *, int64_t *))(void *)sceKernelConvertUtcToLocaltime;
    if (to_local(utc, &local, tz, &dst) != 0)
        local = utc - 8 * 3600;
    int64_t days = local / 86400, rem = local % 86400;
    if (rem < 0)
        rem += 86400, days--;
    days += 719468;
    int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    unsigned doe = (unsigned)(days - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    eggs_set_clock((int)(mp < 10 ? mp + 3 : mp - 9), (int)(doy - (153 * mp + 2) / 5 + 1), (int)(rem / 3600));
}

static uint32_t last_buttons;    // raw buttons, OR of every pad read this frame
static int last_connected;       // OR of the connected flags
static uint32_t good_reads;      // successful scePadReadState calls
static int stick_bits;           // IN_ bits from both analog sticks
static uint32_t pad_buttons[MAX_PADS];
static int pad_connected[MAX_PADS];
static int32_t audio = -1;       // audio port, or the error from opening it

static void note_rc(int32_t rc)
{
    if (rc < 0)
        last_rc = rc;
}

static void input_init(void)
{
    // Same parameters as the OpenOrbis samples; user service first, then the
    // pad library (the order Apollo Save Tool uses on retail consoles).
    OrbisUserServiceInitializeParams param;
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    int32_t rc = sceUserServiceInitialize(&param);
    if (rc != (int32_t)ORBIS_USER_SERVICE_ERROR_ALREADY_INITIALIZED)
        note_rc(rc);
    note_rc(scePadInit());
}

static void open_pad_for(int32_t user)
{
    if (user == ORBIS_USER_SERVICE_USER_ID_INVALID || npads >= MAX_PADS)
        return;
    for (int i = 0; i < npads; i++)
        if (pad_users[i] == user)
            return;
    int h = scePadOpen(user, ORBIS_PAD_PORT_TYPE_STANDARD, 0, NULL);
    if (h < 0) {
        note_rc(h);
        h = scePadGetHandle(user, ORBIS_PAD_PORT_TYPE_STANDARD, 0);  // already opened?
        if (h < 0) {
            note_rc(h);
            return;
        }
    }
    pads[npads] = h;
    pad_users[npads] = user;
    npads++;
}

// Opens the initial user's pad and the pad of every logged-in user. Safe to
// call again: users that already have a handle are skipped.
static void pads_open(void)
{
    if (!have_initial_user) {
        int32_t user = ORBIS_USER_SERVICE_USER_ID_INVALID;
        int32_t rc = sceUserServiceGetInitialUser(&user);
        if (rc == 0 && user != ORBIS_USER_SERVICE_USER_ID_INVALID) {
            initial_user = user;
            have_initial_user = 1;
        } else {
            note_rc(rc);
        }
    }
    if (have_initial_user)
        open_pad_for(initial_user);

    OrbisUserServiceLoginUserIdList list;
    for (int i = 0; i < ORBIS_USER_SERVICE_MAX_LOGIN_USERS; i++)
        list.userId[i] = ORBIS_USER_SERVICE_USER_ID_INVALID;
    int32_t rc = sceUserServiceGetLoginUserIdList(&list);
    if (rc == 0) {
        for (int i = 0; i < ORBIS_USER_SERVICE_MAX_LOGIN_USERS; i++)
            open_pad_for(list.userId[i]);
    } else {
        note_rc(rc);
    }
}

// Reads every open pad. Returns the OR of all their buttons.
static uint32_t read_buttons(void)
{
    uint32_t buttons = 0;
    int connected = 0, sticks = 0;
    for (int i = 0; i < npads; i++) {
        OrbisPadData data;
        int32_t rc = scePadReadState(pads[i], &data);
        if (rc != 0) {
            note_rc(rc);
            continue;
        }
        good_reads++;
        buttons |= data.buttons;
        connected |= data.connected ? 1 : 0;
        pad_buttons[i] = data.buttons;
        pad_connected[i] = data.connected ? 1 : 0;
        // Sticks rest near 128; past a third of the way counts as a press.
        if (data.leftStick.x < 70) sticks |= IN_LEFT;
        if (data.leftStick.x > 186) sticks |= IN_RIGHT;
        if (data.leftStick.y < 70) sticks |= IN_UP;
        if (data.leftStick.y > 186) sticks |= IN_DOWN;
        if (data.rightStick.x < 70) sticks |= IN_RS_LEFT;
        if (data.rightStick.x > 186) sticks |= IN_RS_RIGHT;
        if (data.rightStick.y < 70) sticks |= IN_RS_UP;
        if (data.rightStick.y > 186) sticks |= IN_RS_DOWN;
    }
    stick_bits = sticks;
    last_buttons = buttons;
    last_connected = connected;
    return buttons;
}

static void hex8(char *out, uint32_t v)
{
    static const char digits[] = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) {
        out[i] = digits[v & 0xF];
        v >>= 4;
    }
    out[8] = 0;
}

static void update_diag(void)
{
    // PADS:<count> H:<first handle> USER:<initial user> RC:<last error>
    // BTN:<raw buttons> CON:<connected> RD:<good reads> AUD:<audio port or error>
    char h[9], u[9], rc[9], b[9], rd[9], au[9], line[160];
    hex8(h, npads ? (uint32_t)pads[0] : 0xFFFFFFFFu);
    hex8(u, have_initial_user ? (uint32_t)initial_user : 0xFFFFFFFFu);
    hex8(rc, (uint32_t)last_rc);
    hex8(b, last_buttons);
    hex8(rd, good_reads);
    hex8(au, (uint32_t)audio);
    snprintf(line, sizeof(line), "PADS:%d H:%s USER:%s RC:%s BTN:%s CON:%d RD:%s AUD:%s",
             npads, h, u, rc, b, last_connected, rd, au);
    game_set_diag(line);
}

int plat_read_file(const char *path, char *buf, int max)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    size_t n = fread(buf, 1, (size_t)max, f);
    fclose(f);
    return (int)n;
}

int plat_write_file(const char *path, const char *buf, int len)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return -1;
    size_t n = fwrite(buf, 1, (size_t)len, f);
    fclose(f);
    return n == (size_t)len ? 0 : -1;
}

int plat_remove_file(const char *path)
{
    return sceKernelUnlink(path) == 0 ? 0 : -1;
}

static void *audio_thread(void *arg)
{
    static int16_t buf[256 * 2];
    (void)arg;
    for (;;) {
        synth_render(buf, 256);
        sceAudioOutOutput(audio, buf);
    }
    return NULL;
}

// Same port settings as the OpenOrbis audio sample. Without a port the game
// still runs, silently, and the diagnostic line shows the error.
static void audio_init(void)
{
    sceAudioOutInit();
    audio = sceAudioOutOpen(ORBIS_USER_SERVICE_USER_ID_SYSTEM, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0, 256, 48000,
                            ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    if (audio <= 0)
        return;
    synth_init(48000);
    OrbisPthread thread;
    scePthreadCreate(&thread, NULL, audio_thread, NULL, "fritzdash audio");
}

int plat_list_dir(const char *dir, void (*fn)(const char *name, void *ctx), void *ctx)
{
    DIR *d = opendir(dir);
    if (!d)
        return -1;
    struct dirent *e;
    while ((e = readdir(d)) != NULL)
        fn(e->d_name, ctx);
    closedir(d);
    return 0;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (video_init() != 0) {
        printf("fritzdash: video init failed\n");
        for (;;)
            sceKernelUsleep(1000000);
    }

    // Without this the system launch splash (our sce_sys art) stays on top of
    // the video output for the whole session.
    note_rc(sceSystemServiceHideSplashScreen());

    input_init();
    pads_open();
    audio_init();

    sceKernelMkdir(DATA_DIR, 0777);  // fine if they already exist
    sceKernelMkdir(LEVELS_DIR, 0777);
    sceKernelMkdir(GHOSTS_DIR, 0777);
    sceKernelMkdir(SONGS_DIR, 0777);

    static Game game;
    uint64_t now = sceKernelGetProcessTime();
    game_init(&game, (uint32_t)(now * 2654435761u) | 1u);

    int64_t frame_id = 0;
    int cur = 0;
    for (;;) {
        // Keep trying to open a pad every second while none is open, and look
        // for newly logged-in users every five seconds.
        if ((npads == 0 && frame_id % 60 == 0) || frame_id % 300 == 299)
            pads_open();

        // See game.h for what each input bit does.
        uint32_t buttons = read_buttons();
        int input = 0;
        if (buttons & ORBIS_PAD_BUTTON_CROSS)
            input |= IN_CROSS;
        if (buttons & ORBIS_PAD_BUTTON_OPTIONS)
            input |= IN_START;
        if (buttons & ORBIS_PAD_BUTTON_CIRCLE)
            input |= IN_CIRCLE;
        if (buttons & ORBIS_PAD_BUTTON_UP)
            input |= IN_UP;
        if (buttons & ORBIS_PAD_BUTTON_DOWN)
            input |= IN_DOWN;
        if (buttons & ORBIS_PAD_BUTTON_L1)
            input |= IN_L1;
        if (buttons & ORBIS_PAD_BUTTON_R1)
            input |= IN_R1;
        if (buttons & ORBIS_PAD_BUTTON_LEFT)
            input |= IN_LEFT;
        if (buttons & ORBIS_PAD_BUTTON_RIGHT)
            input |= IN_RIGHT;
        if (buttons & ORBIS_PAD_BUTTON_SQUARE)
            input |= IN_SQUARE;
        if (buttons & ORBIS_PAD_BUTTON_TRIANGLE)
            input |= IN_TRIANGLE;
        if (buttons & ORBIS_PAD_BUTTON_L2)
            input |= IN_L2;
        if (buttons & ORBIS_PAD_BUTTON_R2)
            input |= IN_R2;
        if (buttons & ORBIS_PAD_BUTTON_TOUCH_PAD)
            input |= IN_TOUCH;
        if (buttons & ORBIS_PAD_BUTTON_L3)
            input |= IN_L3;
        input |= stick_bits;
        // The 2 player race needs each controller on its own.
        if (npads > 0 && (pad_buttons[0] & ORBIS_PAD_BUTTON_CROSS))
            input |= IN_P1_CROSS;
        if (npads > 1 && (pad_buttons[1] & ORBIS_PAD_BUTTON_CROSS))
            input |= IN_P2_CROSS;
        if (npads > 1 && pad_connected[1])
            input |= IN_P2_HERE;
        update_diag();
        if (frame_id % 600 == 0)
            update_clock();

        // One game step per displayed frame; the vsync flip paces us at 60 fps.
        game_step(&game, input);

        draw_set_target(framebuffers[cur]);
        game_draw(&game);

        // First second: a flashing band at the top, so it is obvious at a
        // glance that our framebuffer (not the system splash) is on screen.
        if (frame_id < 60) {
            draw_set_offset(0, 0);
            draw_rect(0, 0, SCREEN_W, 40, (frame_id / 8) % 2 ? RGB(255, 0, 255) : RGB(0, 255, 255));
        }

        sceVideoOutSubmitFlip(video, cur, ORBIS_VIDEO_OUT_FLIP_VSYNC, frame_id);
        wait_for_flip(frame_id);
        cur ^= 1;
        frame_id++;
    }
    return 0;
}
