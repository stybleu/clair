/*
 * V0.66: the module gui (import gui): a window to draw in, in real time, like pygame:
 * shapes, text, images, the keyboard, the mouse (or a finger).
 *
 * Copied into the C of the programs that import gui (tools/embed_file.cmake), after the
 * runtime of strings and lists.  SDL3 is not linked: it is loaded (dlopen) when the first
 * window opens, so a program that imports gui, or nbody, without opening a window needs
 * nothing more.  The part of SDL3 used is declared below (SDL keeps it the same in every
 * 3.x version); tests/gui_abi.c checks it against the headers of SDL.
 *
 * One window at a time.  Each turn of the loop:  while w.running() { draw...  w.present() }
 * running() reads what happened since the last turn, present() shows the frame and waits
 * to keep 60 frames a second.
 *
 * In Termux the window goes to the app Termux:X11: without DISPLAY, the runtime uses :0,
 * starts the X server termux-x11 when it is not running, and brings the app to the front.
 *
 * For the tests: CLAIR_GUI_FRAMES=n closes the window after n frames, CLAIR_GUI_SHOT=file.png
 * keeps the last frame, CLAIR_GUI_EVENTS="frame:kind:x:y[:more];..." adds mouse and key events
 * (kind: down, up, move, wheel, keydown, keyup), CLAIR_SDL3=none acts as if SDL3 were missing
 * (CLAIR_SDL3=path: the SDL3 to load).
 */

/* ---- the part of SDL3 that is used ---- */

typedef struct { float x, y, w, h; } ClairSdlFRect;
typedef struct { int x, y, w, h; } ClairSdlRect;
typedef struct { float x, y; } ClairSdlFPoint;
typedef struct { float r, g, b, a; } ClairSdlFColor;
typedef struct { ClairSdlFPoint position; ClairSdlFColor color; ClairSdlFPoint tex_coord; } ClairSdlVertex;
typedef struct { uint32_t flags, format; int w, h, pitch; void *pixels; } ClairSdlSurface;   /* the beginning */
typedef union {
    uint32_t type;
    struct { uint32_t type, reserved; uint64_t timestamp; uint32_t window, which, scancode, key; uint16_t mod, raw; bool down, repeat; } key;
    struct { uint32_t type, reserved; uint64_t timestamp; uint32_t window; const char *text; } text;
    struct { uint32_t type, reserved; uint64_t timestamp; uint32_t window, which, state; float x, y, xrel, yrel; } motion;
    struct { uint32_t type, reserved; uint64_t timestamp; uint32_t window, which; uint8_t button; bool down; uint8_t clicks, padding; float x, y; } button;
    struct { uint32_t type, reserved; uint64_t timestamp; uint32_t window, which; float x, y; uint32_t direction; float mouse_x, mouse_y; } wheel;
    struct { uint32_t type, reserved; uint64_t timestamp; uint32_t window; int32_t data1, data2; } window;
    uint8_t padding[128];
} ClairSdlEvent;

enum {
    CLAIR_SDL_INIT_VIDEO = 0x20, CLAIR_SDL_WINDOW_BORDERLESS = 0x10, CLAIR_SDL_WINDOW_RESIZABLE = 0x20,
    CLAIR_SDL_BLENDMODE_BLEND = 1,
    CLAIR_SDL_EVENT_QUIT = 0x100, CLAIR_SDL_EVENT_WINDOW_RESIZED = 0x206, CLAIR_SDL_EVENT_WINDOW_CLOSE_REQUESTED = 0x210,
    CLAIR_SDL_EVENT_KEY_DOWN = 0x300, CLAIR_SDL_EVENT_KEY_UP = 0x301, CLAIR_SDL_EVENT_TEXT_INPUT = 0x303,
    CLAIR_SDL_EVENT_MOUSE_MOTION = 0x400, CLAIR_SDL_EVENT_MOUSE_BUTTON_DOWN = 0x401, CLAIR_SDL_EVENT_MOUSE_BUTTON_UP = 0x402,
    CLAIR_SDL_EVENT_MOUSE_WHEEL = 0x403, CLAIR_SDL_MOUSEWHEEL_FLIPPED = 1, CLAIR_SDL_SCANCODES = 512
};

static struct {
    void *lib;
    bool (*Init)(uint32_t);
    void (*Quit)(void);
    const char *(*GetError)(void);
    uint32_t (*GetPrimaryDisplay)(void);
    bool (*GetDisplayBounds)(uint32_t, ClairSdlRect *);
    bool (*CreateWindowAndRenderer)(const char *, int, int, uint64_t, void **, void **);
    void (*DestroyRenderer)(void *);
    void (*DestroyWindow)(void *);
    bool (*SetRenderDrawBlendMode)(void *, uint32_t);
    bool (*StartTextInput)(void *);
    bool (*SetRenderDrawColor)(void *, uint8_t, uint8_t, uint8_t, uint8_t);
    bool (*RenderClear)(void *);
    bool (*PollEvent)(ClairSdlEvent *);
    const char *(*GetKeyName)(uint32_t);
    int (*GetScancodeFromName)(const char *);
    uint32_t (*GetKeyFromName)(const char *);
    int (*GetScancodeFromKey)(uint32_t, uint16_t *);
    bool (*GetCurrentRenderOutputSize)(void *, int *, int *);
    bool (*RenderFillRect)(void *, const ClairSdlFRect *);
    bool (*RenderFillRects)(void *, const ClairSdlFRect *, int);
    bool (*RenderRect)(void *, const ClairSdlFRect *);
    bool (*RenderLine)(void *, float, float, float, float);
    bool (*RenderLines)(void *, const ClairSdlFPoint *, int);
    bool (*RenderPoint)(void *, float, float);
    bool (*RenderGeometry)(void *, void *, const ClairSdlVertex *, int, const int *, int);
    ClairSdlSurface *(*LoadSurface)(const char *);            /* SDL 3.4 (PNG and BMP) */
    ClairSdlSurface *(*LoadBMP)(const char *);
    void *(*CreateTextureFromSurface)(void *, ClairSdlSurface *);
    void (*DestroySurface)(ClairSdlSurface *);
    void (*DestroyTexture)(void *);
    bool (*RenderTexture)(void *, void *, const ClairSdlFRect *, const ClairSdlFRect *);
    bool (*RenderTextureRotated)(void *, void *, const ClairSdlFRect *, const ClairSdlFRect *, double, const ClairSdlFPoint *, int);
    bool (*RenderPresent)(void *);
    uint64_t (*GetTicksNS)(void);
    void (*DelayNS)(uint64_t);
    bool (*SetWindowTitle)(void *, const char *);
    bool (*HideWindow)(void *);
    ClairSdlSurface *(*RenderReadPixels)(void *, const ClairSdlRect *);
    bool (*SaveBMP)(ClairSdlSurface *, const char *);
    bool (*SavePNG)(ClairSdlSurface *, const char *);          /* SDL 3.4 */
    bool (*ReadSurfacePixel)(ClairSdlSurface *, int, int, uint8_t *, uint8_t *, uint8_t *, uint8_t *);
} clair_sdl;

/* loads SDL3 (once); false when it is not there */
static bool clair_sdl_load(void) {
    if (clair_sdl.lib) return true;
    const char *forced = getenv("CLAIR_SDL3");
    if (forced && strcmp(forced, "none") == 0) return false;   /* the tests: as if SDL3 were not there */
    const char *prefix = getenv("PREFIX");
    const char *names[6] = { getenv("CLAIR_SDL3"), NULL, NULL, "libSDL3.so.0", "libSDL3.so", "libSDL3.0.dylib" };
    static char p1[600], p2[600];
    if (prefix) {
        snprintf(p1, sizeof(p1), "%s/lib/libSDL3.so.0", prefix);
        snprintf(p2, sizeof(p2), "%s/lib/libSDL3.so", prefix);
        names[1] = p1;
        names[2] = p2;
    }
    void *lib = NULL;
    for (int i = 0; i < 6 && !lib; i++) if (names[i] && names[i][0]) lib = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
    if (!lib) return false;
#define CLAIR_SDL_SYM(field, name, needed) \
    do { *(void **)&clair_sdl.field = dlsym(lib, name); if (!clair_sdl.field && (needed)) { dlclose(lib); return false; } } while (0)
    CLAIR_SDL_SYM(Init, "SDL_Init", 1);
    CLAIR_SDL_SYM(Quit, "SDL_Quit", 1);
    CLAIR_SDL_SYM(GetError, "SDL_GetError", 1);
    CLAIR_SDL_SYM(GetPrimaryDisplay, "SDL_GetPrimaryDisplay", 1);
    CLAIR_SDL_SYM(GetDisplayBounds, "SDL_GetDisplayBounds", 1);
    CLAIR_SDL_SYM(CreateWindowAndRenderer, "SDL_CreateWindowAndRenderer", 1);
    CLAIR_SDL_SYM(DestroyRenderer, "SDL_DestroyRenderer", 1);
    CLAIR_SDL_SYM(DestroyWindow, "SDL_DestroyWindow", 1);
    CLAIR_SDL_SYM(SetRenderDrawBlendMode, "SDL_SetRenderDrawBlendMode", 1);
    CLAIR_SDL_SYM(StartTextInput, "SDL_StartTextInput", 1);
    CLAIR_SDL_SYM(SetRenderDrawColor, "SDL_SetRenderDrawColor", 1);
    CLAIR_SDL_SYM(RenderClear, "SDL_RenderClear", 1);
    CLAIR_SDL_SYM(PollEvent, "SDL_PollEvent", 1);
    CLAIR_SDL_SYM(GetKeyName, "SDL_GetKeyName", 1);
    CLAIR_SDL_SYM(GetScancodeFromName, "SDL_GetScancodeFromName", 1);
    CLAIR_SDL_SYM(GetKeyFromName, "SDL_GetKeyFromName", 1);
    CLAIR_SDL_SYM(GetScancodeFromKey, "SDL_GetScancodeFromKey", 1);
    CLAIR_SDL_SYM(GetCurrentRenderOutputSize, "SDL_GetCurrentRenderOutputSize", 1);
    CLAIR_SDL_SYM(RenderFillRect, "SDL_RenderFillRect", 1);
    CLAIR_SDL_SYM(RenderFillRects, "SDL_RenderFillRects", 1);
    CLAIR_SDL_SYM(RenderRect, "SDL_RenderRect", 1);
    CLAIR_SDL_SYM(RenderLine, "SDL_RenderLine", 1);
    CLAIR_SDL_SYM(RenderLines, "SDL_RenderLines", 1);
    CLAIR_SDL_SYM(RenderPoint, "SDL_RenderPoint", 1);
    CLAIR_SDL_SYM(RenderGeometry, "SDL_RenderGeometry", 1);
    CLAIR_SDL_SYM(LoadSurface, "SDL_LoadSurface", 0);
    CLAIR_SDL_SYM(LoadBMP, "SDL_LoadBMP", 1);
    CLAIR_SDL_SYM(CreateTextureFromSurface, "SDL_CreateTextureFromSurface", 1);
    CLAIR_SDL_SYM(DestroySurface, "SDL_DestroySurface", 1);
    CLAIR_SDL_SYM(DestroyTexture, "SDL_DestroyTexture", 1);
    CLAIR_SDL_SYM(RenderTexture, "SDL_RenderTexture", 1);
    CLAIR_SDL_SYM(RenderTextureRotated, "SDL_RenderTextureRotated", 1);
    CLAIR_SDL_SYM(RenderPresent, "SDL_RenderPresent", 1);
    CLAIR_SDL_SYM(GetTicksNS, "SDL_GetTicksNS", 1);
    CLAIR_SDL_SYM(DelayNS, "SDL_DelayNS", 1);
    CLAIR_SDL_SYM(SetWindowTitle, "SDL_SetWindowTitle", 1);
    CLAIR_SDL_SYM(HideWindow, "SDL_HideWindow", 1);
    CLAIR_SDL_SYM(RenderReadPixels, "SDL_RenderReadPixels", 1);
    CLAIR_SDL_SYM(SaveBMP, "SDL_SaveBMP", 1);
    CLAIR_SDL_SYM(SavePNG, "SDL_SavePNG", 0);
    CLAIR_SDL_SYM(ReadSurfacePixel, "SDL_ReadSurfacePixel", 1);
#undef CLAIR_SDL_SYM
    clair_sdl.lib = lib;
    return true;
}

/* ---- the letters: 8 x 8 pixels, U+0020 to U+007E and U+00A0 to U+00FF (accents), public domain
   (Daniel Hepper, font8x8, after the IBM VGA fonts); bit 0 is the pixel on the left ---- */
static const char clair_gui_font_hex[] =
    "0000000000000000183c3c1818001800363600000000000036367f367f3636000c3e031e301f0c00006333180c6663001c361c6e3b336e000606030000000000"
    "180c0606060c1800060c1818180c060000663cff3c660000000c0c3f0c0c000000000000000c0c060000003f0000000000000000000c0c006030180c06030100"
    "3e63737b6f673e000c0e0c0c0c0c3f001e33301c06333f001e33301c30331e00383c36337f3078003f031f3030331e001c06031f33331e003f3330180c0c0c00"
    "1e33331e33331e001e33333e30180e00000c0c00000c0c00000c0c00000c0c06180c0603060c180000003f00003f0000060c1830180c06001e3330180c000c00"
    "3e637b7b7b031e000c1e33333f3333003f66663e66663f003c66030303663c001f36666666361f007f46161e16467f007f46161e16060f003c66030373667c00"
    "3333333f333333001e0c0c0c0c0c1e007830303033331e006766361e366667000f06060646667f0063777f7f6b63630063676f7b736363001c36636363361c00"
    "3f66663e06060f001e3333333b1e38003f66663e366667001e33070e38331e003f2d0c0c0c0c1e003333333333333f0033333333331e0c006363636b7f776300"
    "6363361c1c3663003333331e0c0c1e007f6331184c667f001e06060606061e0003060c18306040001e18181818181e00081c36630000000000000000000000ff"
    "0c0c18000000000000001e303e336e000706063e66663b0000001e3303331e003830303e33336e0000001e333f031e001c36060f06060f0000006e33333e301f"
    "0706366e666667000c000e0c0c0c1e00300030303033331e070666361e3667000e0c0c0c0c0c1e000000337f7f6b630000001f333333330000001e3333331e00"
    "00003b66663e060f00006e33333e307800003b6e66060f0000003e031e301f00080c3e0c0c2c18000000333333336e0000003333331e0c000000636b7f7f3600"
    "000063361c36630000003333333e301f00003f190c263f00380c0c070c0c38001818180018181800070c0c380c0c07006e3b0000000000000000000000000000"
    "0000000000000000181800181818180018187e03037e18181c36260f06673f000000633e363e630033331e3f0c3f0c0c18181800181818007cc61c36361c331e"
    "33000000000000003c4299858599423c3c36367c0000000000cc663366cc00000000003f3030000000000000000000003c429da59da5423c7e00000000000000"
    "1c36361c0000000018187e1818007e001c30180c3c0000001c3018301c000000180c00000000000000006666663e0603fedbdbded8d8d8000000001818000000"
    "000000000018301e080c081c000000001c36361c00000000003366cc66330000c36333bdecf6f303c363337bcc6633f003c463b4dbace6800c000c0603331e00"
    "07001c36637f630070001c36637f63001c36003e637f63006e3b003e637f6300631c36637f6363000c0c001e333f33007c36337f333373001e3303331e18301e"
    "07003f061e063f0038003f061e063f000c123f061e063f0036003f061e063f0007001e0c0c0c1e0038001e0c0c0c1e000c12001e0c0c1e0033001e0c0c0c1e00"
    "3f666f6f66663f003f0033373f3b33000e00183c663c18007000183c663c18003c66183c663c18006e3b003e63633e00c3183c66663c180000361c081c360000"
    "5c36737b6f361d000e00666666663c007000666666663c003c66006666663c003300333333331e00700066663c1818000f063e66663e060f001e331f331f0303"
    "07001e303e337e0038001e303e337e007ec33c607c66fc006e3b1e303e337e0033001e303e337e000c0c1e303e337e000000fe30fe33fe0000001e03031e301c"
    "07001e333f031e0038001e333f031e007ec33c667e063c0033001e333f031e0007000e0c0c0c1e001c000e0c0c0c1e003e631c1818183c0033000e0c0c0c1e00"
    "1b0e1b303e331e00001f001f333333000007001e33331e000038001e33331e001e33001e33331e006e3b001e33331e000033001e33331e001818007e00181800"
    "00603c767e6e3c060007003333337e000038003333337e001e33003333337e000033003333337e0000380033333e301f0000063e663e060000330033333e301f";

static unsigned char clair_gui_font[192][8];

static void clair_gui_font_read(void) {               /* once: 16 hexadecimal digits per letter */
    static int done = 0;
    if (done) return;
    for (int i = 0; i < 192 * 8; i++) {
        char a = clair_gui_font_hex[2 * i], b = clair_gui_font_hex[2 * i + 1];
        clair_gui_font[i / 8][i % 8] = (unsigned char)(((a <= '9' ? a - '0' : a - 'a' + 10) << 4) | (b <= '9' ? b - '0' : b - 'a' + 10));
    }
    done = 1;
}

/* ---- the window ---- */

#define CLAIR_GUI_EVENTS 256
#define CLAIR_GUI_IMAGES 1024

typedef struct {
    char kind[16];
    char key[32];
    char text[32];
    float x, y;
    int button;
} ClairGuiEvent;

static struct {
    void *window, *renderer;
    int open, quit, inited, closing;
    ClairGuiEvent events[CLAIR_GUI_EVENTS];
    int nevents;
    bool keys_before[CLAIR_SDL_SCANCODES], keys_now[CLAIR_SDL_SCANCODES];
    uint32_t mouse_before, mouse_now;            /* bit b-1: button b held */
    float mx, my, wheel;
    uint64_t frame_ns, last_ns, start_ns, fps_ns;
    double dt, fps;
    int fps_frames;
    long frames, stop_after;                     /* CLAIR_GUI_FRAMES */
    void *images[CLAIR_GUI_IMAGES];
    int image_w[CLAIR_GUI_IMAGES], image_h[CLAIR_GUI_IMAGES];
    int nimages;
    uint8_t cr, cg, cb, ca;                      /* the color of the next drawings */
    uint64_t active_button;                      /* the button held down (its hash), 0: none */
    ClairSdlFRect *rects;                        /* the pixels of a text */
    size_t nrects, caprects;
} clair_gui;

_Noreturn static void clair_gui_fail(const char *what, const char *detail) {
    fprintf(stderr, "Clair runtime error: %s%s\n", what, detail ? detail : "");
    exit(EXIT_FAILURE);
}

static void clair_gui_need(void) {
    if (!clair_gui.open) clair_gui_fail("gui: open a window first: let w: Window = gui.open(\"title\", 800, 600)", NULL);
}

static bool clair_gui_available(void) { return clair_sdl_load(); }

/* is an X server listening on this socket? */
static bool clair_gui_x_alive(const char *path) {
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0) return false;
    struct sockaddr_un a;
    memset(&a, 0, sizeof(a));
    a.sun_family = AF_UNIX;
    snprintf(a.sun_path, sizeof(a.sun_path), "%s", path);
    bool ok = connect(s, (struct sockaddr *)&a, sizeof(a)) == 0;
    close(s);
    return ok;
}

/* In Termux: the window goes to the app Termux:X11 (display :0); starts its X server if needed,
   and brings the app to the front. */
static void clair_gui_termux_x11(void) {
    const char *prefix = getenv("PREFIX");
    if (getenv("DISPLAY") || getenv("WAYLAND_DISPLAY") || getenv("SDL_VIDEO_DRIVER") || !prefix || !strstr(prefix, "com.termux")) return;
    setenv("DISPLAY", ":0", 1);
    char sock[600], server[600];
    snprintf(sock, sizeof(sock), "%s/tmp/.X11-unix/X0", prefix);
    snprintf(server, sizeof(server), "%s/bin/termux-x11", prefix);
    if (!clair_gui_x_alive(sock)) {
        if (access(server, X_OK) != 0) return;      /* SDL says why there is no window */
        pid_t child = fork();                       /* the server outlives the program, in its own session */
        if (child == 0) {
            setsid();
            if (fork() == 0) {
                int null = open("/dev/null", O_RDWR);
                if (null >= 0) { dup2(null, 0); dup2(null, 1); dup2(null, 2); if (null > 2) close(null); }
                execl(server, "termux-x11", ":0", (char *)NULL);
                _exit(127);
            }
            _exit(0);
        }
        if (child < 0) return;
        waitpid(child, NULL, 0);
        for (int i = 0; i < 100 && !clair_gui_x_alive(sock); i++) {
            struct timespec t = { 0, 50000000 };
            nanosleep(&t, NULL);
        }
    }
    FILE *p = popen("am start --user 0 -n com.termux.x11/com.termux.x11.MainActivity 2>&1", "r");
    if (!p) return;
    char out[512];
    size_t n = fread(out, 1, sizeof(out) - 1, p);
    out[n] = '\0';
    pclose(p);
    if (strstr(out, "Error") || strstr(out, "rror:"))
        fprintf(stderr, "gui: the window is shown by the app Termux:X11: install it from\n"
                        "     https://github.com/termux/termux-x11/releases (termux-x11-universal-debug.apk)\n");
}

static void clair_gui_destroy(void) {
    for (int i = 0; i < clair_gui.nimages; i++) if (clair_gui.images[i]) clair_sdl.DestroyTexture(clair_gui.images[i]);
    clair_gui.nimages = 0;
    if (clair_gui.renderer) clair_sdl.DestroyRenderer(clair_gui.renderer);
    if (clair_gui.window) clair_sdl.DestroyWindow(clair_gui.window);
    clair_gui.renderer = clair_gui.window = NULL;
    clair_gui.open = clair_gui.closing = 0;
}

static void clair_gui_at_exit(void) {
    clair_gui_destroy();
    free(clair_gui.rects);
    if (clair_gui.inited) clair_sdl.Quit();
}

/* gui.open(title, width, height); 0, 0: the whole screen */
static int64_t clair_gui_open(const char *title, int64_t w, int64_t h) {
    if (clair_gui.closing) clair_gui_destroy();
    if (clair_gui.open) clair_gui_fail("gui: one window at a time (w.close() closes the other one)", NULL);
    if (w < 0 || h < 0 || w > 16384 || h > 16384) clair_gui_fail("gui.open(): the size is 1 to 16384 pixels (0, 0: the whole screen)", NULL);
    if (!clair_sdl_load())
        clair_gui_fail("gui needs SDL3 (libSDL3.so.0 not found).\n"
                       "In Termux: pkg install x11-repo && pkg install sdl3 termux-x11-nightly, and the app Termux:X11", NULL);
    if (!clair_gui.inited) {
        clair_gui_termux_x11();
        if (!clair_sdl.Init(CLAIR_SDL_INIT_VIDEO)) {
            fprintf(stderr, "Clair runtime error: gui: no screen to open a window on (%s).\n"
                            "In Termux: pkg install x11-repo && pkg install termux-x11-nightly, and the app Termux:X11\n"
                            "(https://github.com/termux/termux-x11/releases).\n", clair_sdl.GetError());
            exit(EXIT_FAILURE);
        }
        clair_gui.inited = 1;
        atexit(clair_gui_at_exit);
        const char *n = getenv("CLAIR_GUI_FRAMES");
        clair_gui.stop_after = n ? atol(n) : 0;
    }
    uint64_t flags = CLAIR_SDL_WINDOW_RESIZABLE;
    if (w == 0 || h == 0) {
        ClairSdlRect r = { 0, 0, 0, 0 };
        if (!clair_sdl.GetDisplayBounds(clair_sdl.GetPrimaryDisplay(), &r) || r.w < 1 || r.h < 1) { r.w = 800; r.h = 600; }
        if (w == 0) w = r.w;
        if (h == 0) h = r.h;
        flags |= CLAIR_SDL_WINDOW_BORDERLESS;
    }
    if (!clair_sdl.CreateWindowAndRenderer(title, (int)w, (int)h, flags, &clair_gui.window, &clair_gui.renderer)) {
        fprintf(stderr, "Clair runtime error: gui: cannot open the window (%s).\n", clair_sdl.GetError());
        exit(EXIT_FAILURE);
    }
    clair_sdl.SetRenderDrawBlendMode(clair_gui.renderer, CLAIR_SDL_BLENDMODE_BLEND);
    clair_sdl.StartTextInput(clair_gui.window);
    clair_gui.open = 1;
    clair_gui.quit = 0;
    clair_gui.frames = 0;
    clair_gui.frame_ns = 1000000000ull / 60;
    clair_gui.start_ns = clair_gui.last_ns = clair_gui.fps_ns = clair_sdl.GetTicksNS();
    clair_gui.dt = 1.0 / 60;
    clair_gui.fps = 0;
    clair_gui.fps_frames = 0;
    memset(clair_gui.keys_now, 0, sizeof(clair_gui.keys_now));
    clair_gui.mouse_now = 0;
    clair_gui.cr = clair_gui.cg = clair_gui.cb = clair_gui.ca = 255;
    clair_sdl.SetRenderDrawColor(clair_gui.renderer, 0, 0, 0, 255);
    clair_sdl.RenderClear(clair_gui.renderer);
    return 1;
}

static void clair_gui_lower(char *dst, size_t n, const char *src) {
    size_t i = 0;
    for (; src && src[i] && i + 1 < n; i++) dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

static ClairGuiEvent *clair_gui_add_event(const char *kind) {
    if (clair_gui.nevents >= CLAIR_GUI_EVENTS) return NULL;
    ClairGuiEvent *e = &clair_gui.events[clair_gui.nevents++];
    memset(e, 0, sizeof(*e));
    snprintf(e->kind, sizeof(e->kind), "%s", kind);
    return e;
}

static void clair_gui_mouse_event(bool down, int button, float x, float y) {
    ClairGuiEvent *e = clair_gui_add_event(down ? "mouse_down" : "mouse_up");
    if (e) { e->x = x; e->y = y; e->button = button; }
    clair_gui.mx = x;
    clair_gui.my = y;
    if (button >= 1 && button <= 5) {
        if (down) clair_gui.mouse_now |= 1u << (button - 1);
        else clair_gui.mouse_now &= ~(1u << (button - 1));
    }
}

static void clair_gui_key_event(bool down, int scancode, uint32_t key) {
    ClairGuiEvent *e = clair_gui_add_event(down ? "key_down" : "key_up");
    if (e) clair_gui_lower(e->key, sizeof(e->key), clair_sdl.GetKeyName(key));
    if (scancode > 0 && scancode < CLAIR_SDL_SCANCODES) clair_gui.keys_now[scancode] = down;
}

static int clair_gui_scancode(const char *name, bool fail);

/* CLAIR_GUI_EVENTS (the tests): the events of this frame */
static void clair_gui_test_events(long frame) {
    const char *spec = getenv("CLAIR_GUI_EVENTS");
    if (!spec) return;
    char item[160];
    while (*spec) {
        size_t n = strcspn(spec, ";");
        if (n >= sizeof(item)) n = sizeof(item) - 1;
        memcpy(item, spec, n);
        item[n] = '\0';
        spec += n + (spec[n] == ';');
        long f = 0;
        char kind[16] = "", more[40] = "";
        double x = 0, y = 0;
        if (sscanf(item, "%ld:%15[a-z]:%lf:%lf:%39s", &f, kind, &x, &y, more) < 2 || f != frame) continue;
        if (strcmp(kind, "down") == 0 || strcmp(kind, "up") == 0)
            clair_gui_mouse_event(kind[0] == 'd', more[0] ? atoi(more) : 1, (float)x, (float)y);
        else if (strcmp(kind, "move") == 0) {
            ClairGuiEvent *e = clair_gui_add_event("mouse_move");
            if (e) { e->x = (float)x; e->y = (float)y; }
            clair_gui.mx = (float)x;
            clair_gui.my = (float)y;
        } else if (strcmp(kind, "wheel") == 0) {
            ClairGuiEvent *e = clair_gui_add_event("wheel");
            if (e) { e->x = 0; e->y = (float)y; }
            clair_gui.wheel += (float)y;
        } else if (strcmp(kind, "keydown") == 0 || strcmp(kind, "keyup") == 0) {
            int s = clair_gui_scancode(more, false);
            ClairGuiEvent *e = clair_gui_add_event(kind[3] == 'd' ? "key_down" : "key_up");
            if (e) clair_gui_lower(e->key, sizeof(e->key), more);
            if (s > 0) clair_gui.keys_now[s] = kind[3] == 'd';
        }
    }
}

static int64_t clair_gui_screenshot_to(const char *path);

/* w.running(): reads what happened since the last frame; false once the window is closed */
static bool clair_gui_frame(void) {
    if (clair_gui.closing) clair_gui_destroy();
    if (!clair_gui.open) return false;
    clair_gui.nevents = 0;
    clair_gui.wheel = 0;
    memcpy(clair_gui.keys_before, clair_gui.keys_now, sizeof(clair_gui.keys_now));
    clair_gui.mouse_before = clair_gui.mouse_now;
    ClairSdlEvent ev;
    while (clair_sdl.PollEvent(&ev)) {
        ClairGuiEvent *e = NULL;
        switch (ev.type) {
            case CLAIR_SDL_EVENT_QUIT:
            case CLAIR_SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                clair_gui.quit = 1;
                clair_gui_add_event("quit");
                break;
            case CLAIR_SDL_EVENT_KEY_DOWN:
            case CLAIR_SDL_EVENT_KEY_UP:
                clair_gui_key_event(ev.type == CLAIR_SDL_EVENT_KEY_DOWN, (int)ev.key.scancode, ev.key.key);
                break;
            case CLAIR_SDL_EVENT_TEXT_INPUT:
                e = clair_gui_add_event("text");
                if (e) snprintf(e->text, sizeof(e->text), "%s", ev.text.text ? ev.text.text : "");
                break;
            case CLAIR_SDL_EVENT_MOUSE_BUTTON_DOWN:
            case CLAIR_SDL_EVENT_MOUSE_BUTTON_UP:
                clair_gui_mouse_event(ev.type == CLAIR_SDL_EVENT_MOUSE_BUTTON_DOWN, ev.button.button, ev.button.x, ev.button.y);
                break;
            case CLAIR_SDL_EVENT_MOUSE_MOTION:
                e = clair_gui_add_event("mouse_move");
                if (e) { e->x = ev.motion.x; e->y = ev.motion.y; }
                clair_gui.mx = ev.motion.x;
                clair_gui.my = ev.motion.y;
                break;
            case CLAIR_SDL_EVENT_MOUSE_WHEEL: {
                float y = ev.wheel.direction == CLAIR_SDL_MOUSEWHEEL_FLIPPED ? -ev.wheel.y : ev.wheel.y;
                clair_gui.wheel += y;
                e = clair_gui_add_event("wheel");
                if (e) { e->x = ev.wheel.x; e->y = y; }
                break;
            }
            case CLAIR_SDL_EVENT_WINDOW_RESIZED:
                e = clair_gui_add_event("resize");
                if (e) { e->x = (float)ev.window.data1; e->y = (float)ev.window.data2; }
                break;
            default:
                break;
        }
    }
    clair_gui_test_events(clair_gui.frames);
    if (clair_gui.stop_after > 0 && clair_gui.frames >= clair_gui.stop_after) {
        const char *shot = getenv("CLAIR_GUI_SHOT");
        if (shot && shot[0]) clair_gui_screenshot_to(shot);
        clair_gui.quit = 1;
    }
    clair_gui.frames++;
    return !clair_gui.quit;
}

static int64_t clair_gui_event_count(void) { return clair_gui.nevents; }

/* 0 its kind, 1 its key, 2 the text typed */
static const char *clair_gui_event_text(int64_t i, int64_t what) {
    if (i < 0 || i >= clair_gui.nevents) return "";
    const ClairGuiEvent *e = &clair_gui.events[i];
    const char *s = what == 0 ? e->kind : what == 1 ? e->key : e->text;
    char *d = clair_rt_arena_alloc(strlen(s) + 1);
    strcpy(d, s);
    return d;
}

/* 0 x, 1 y, 2 the mouse button */
static double clair_gui_event_num(int64_t i, int64_t what) {
    if (i < 0 || i >= clair_gui.nevents) return 0;
    const ClairGuiEvent *e = &clair_gui.events[i];
    return what == 0 ? e->x : what == 1 ? e->y : e->button;
}

/* "left", "a", "space", "return"... and in French: gauche, droite, haut, bas, espace, entree, echap */
static int clair_gui_scancode(const char *name, bool fail) {
    static const char *const fr[][2] = {
        { "gauche", "Left" }, { "droite", "Right" }, { "haut", "Up" }, { "bas", "Down" }, { "espace", "Space" },
        { "entree", "Return" }, { "entrée", "Return" }, { "enter", "Return" }, { "echap", "Escape" },
        { "échap", "Escape" }, { "esc", "Escape" }, { "maj", "Left Shift" }, { "shift", "Left Shift" },
        { "ctrl", "Left Ctrl" }, { "alt", "Left Alt" }, { "tab", "Tab" }, { "retour", "Backspace" }
    };
    for (size_t i = 0; i < sizeof(fr) / sizeof(fr[0]); i++) if (strcasecmp(name, fr[i][0]) == 0) name = fr[i][1];
    int s = clair_sdl.GetScancodeFromName(name);
    if (s <= 0) {
        uint32_t k = clair_sdl.GetKeyFromName(name);
        if (k != 0) s = clair_sdl.GetScancodeFromKey(k, NULL);
    }
    if ((s <= 0 || s >= CLAIR_SDL_SCANCODES) && fail) clair_gui_fail("gui: unknown key ", name);
    return s > 0 && s < CLAIR_SDL_SCANCODES ? s : 0;
}

/* 0 held, 1 pressed at this frame, 2 released at this frame */
static bool clair_gui_key(const char *name, int64_t what) {
    clair_gui_need();
    int s = clair_gui_scancode(name, true);
    bool now = clair_gui.keys_now[s], before = clair_gui.keys_before[s];
    return what == 0 ? now : what == 1 ? (now && !before) : (!now && before);
}

/* 0 x, 1 y, 2 the wheel at this frame */
static double clair_gui_mouse(int64_t what) {
    clair_gui_need();
    return what == 0 ? clair_gui.mx : what == 1 ? clair_gui.my : clair_gui.wheel;
}

/* button 1 left, 2 middle, 3 right; 0 held, 1 pressed at this frame, 2 released at this frame */
static bool clair_gui_mouse_button(int64_t button, int64_t what) {
    clair_gui_need();
    if (button < 1 || button > 5) return false;
    uint32_t m = 1u << (button - 1);
    bool now = (clair_gui.mouse_now & m) != 0, before = (clair_gui.mouse_before & m) != 0;
    return what == 0 ? now : what == 1 ? (now && !before) : (!now && before);
}

/* 0 the width, 1 the height of the window */
static int64_t clair_gui_size(int64_t what) {
    clair_gui_need();
    int w = 0, h = 0;
    clair_sdl.GetCurrentRenderOutputSize(clair_gui.renderer, &w, &h);
    return what == 0 ? w : h;
}

static uint8_t clair_gui_byte(int64_t v) { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }

static int64_t clair_gui_set_color(int64_t r, int64_t g, int64_t b, int64_t a) {
    clair_gui.cr = clair_gui_byte(r);
    clair_gui.cg = clair_gui_byte(g);
    clair_gui.cb = clair_gui_byte(b);
    clair_gui.ca = clair_gui_byte(a);
    if (clair_gui.renderer) clair_sdl.SetRenderDrawColor(clair_gui.renderer, clair_gui.cr, clair_gui.cg, clair_gui.cb, clair_gui.ca);
    return 0;
}

static int64_t clair_gui_clear(void) {
    clair_gui_need();
    clair_sdl.RenderClear(clair_gui.renderer);
    return 0;
}

static int64_t clair_gui_rect(double x, double y, double w, double h, bool fill) {
    clair_gui_need();
    ClairSdlFRect r = { (float)x, (float)y, (float)w, (float)h };
    if (fill) clair_sdl.RenderFillRect(clair_gui.renderer, &r);
    else clair_sdl.RenderRect(clair_gui.renderer, &r);
    return 0;
}

static int64_t clair_gui_line(double x1, double y1, double x2, double y2) {
    clair_gui_need();
    clair_sdl.RenderLine(clair_gui.renderer, (float)x1, (float)y1, (float)x2, (float)y2);
    return 0;
}

static int64_t clair_gui_point(double x, double y) {
    clair_gui_need();
    clair_sdl.RenderPoint(clair_gui.renderer, (float)x, (float)y);
    return 0;
}

/* filled triangles: n points around (a fan from the first), in the current color */
static void clair_gui_fan(const float *xs, const float *ys, int n) {
    if (n < 3) return;
    ClairSdlVertex *v = malloc((size_t)n * sizeof(ClairSdlVertex));
    int *idx = malloc((size_t)(n - 2) * 3 * sizeof(int));
    if (!v || !idx) { free(v); free(idx); clair_gui_fail("out of memory", NULL); }
    ClairSdlFColor c = { clair_gui.cr / 255.0f, clair_gui.cg / 255.0f, clair_gui.cb / 255.0f, clair_gui.ca / 255.0f };
    for (int i = 0; i < n; i++) {
        v[i].position.x = xs[i];
        v[i].position.y = ys[i];
        v[i].color = c;
        v[i].tex_coord.x = v[i].tex_coord.y = 0;
    }
    for (int i = 0; i < n - 2; i++) { idx[3 * i] = 0; idx[3 * i + 1] = i + 1; idx[3 * i + 2] = i + 2; }
    clair_sdl.RenderGeometry(clair_gui.renderer, NULL, v, n, idx, (n - 2) * 3);
    free(v);
    free(idx);
}

/* a line through n points (closed: back to the first) */
static void clair_gui_path(const float *xs, const float *ys, int n, bool closed) {
    if (n < 2) return;
    ClairSdlFPoint *p = malloc((size_t)(n + 1) * sizeof(ClairSdlFPoint));
    if (!p) clair_gui_fail("out of memory", NULL);
    for (int i = 0; i < n; i++) { p[i].x = xs[i]; p[i].y = ys[i]; }
    if (closed) p[n] = p[0];
    clair_sdl.RenderLines(clair_gui.renderer, p, n + (closed ? 1 : 0));
    free(p);
}

static int64_t clair_gui_circle(double cx, double cy, double r, bool fill) {
    clair_gui_need();
    if (!(r > 0)) return 0;
    int n = r > 2000 ? 720 : (int)(r * 0.75) + 16;
    float *xs = malloc((size_t)n * sizeof(float)), *ys = malloc((size_t)n * sizeof(float));
    if (!xs || !ys) { free(xs); free(ys); clair_gui_fail("out of memory", NULL); }
    for (int i = 0; i < n; i++) {
        double a = 2 * 3.14159265358979323846 * i / n;
        xs[i] = (float)(cx + r * cos(a));
        ys[i] = (float)(cy + r * sin(a));
    }
    if (fill) clair_gui_fan(xs, ys, n);
    else clair_gui_path(xs, ys, n, true);
    free(xs);
    free(ys);
    return 0;
}

/* mode 0: the outline of a polygon, 1: a filled convex polygon, 2: a line through the points */
static int64_t clair_gui_polygon(ClairList xs, ClairList ys, int64_t mode) {
    clair_gui_need();
    if (!xs || !ys || xs->len != ys->len) clair_gui_fail("gui: as many x as y are needed", NULL);
    int n = (int)xs->len;
    if (n < 2) return 0;
    float *fx = malloc((size_t)n * sizeof(float)), *fy = malloc((size_t)n * sizeof(float));
    if (!fx || !fy) { free(fx); free(fy); clair_gui_fail("out of memory", NULL); }
    for (int i = 0; i < n; i++) { fx[i] = (float)((double *)xs->data)[i]; fy[i] = (float)((double *)ys->data)[i]; }
    if (mode == 1) clair_gui_fan(fx, fy, n);
    else clair_gui_path(fx, fy, n, mode == 0);
    free(fx);
    free(fy);
    return 0;
}

static int64_t clair_gui_triangle(double x1, double y1, double x2, double y2, double x3, double y3) {
    clair_gui_need();
    float xs[3] = { (float)x1, (float)x2, (float)x3 }, ys[3] = { (float)y1, (float)y2, (float)y3 };
    clair_gui_fan(xs, ys, 3);
    return 0;
}

/* the next character of a UTF-8 text, as its glyph (? when there is none) */
static int clair_gui_glyph(const unsigned char **p) {
    const unsigned char *s = *p;
    unsigned c = *s++;
    if (c >= 0xC0) {
        int more = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
        c &= (unsigned)(0x3F >> more);
        for (int i = 0; i < more && (*s & 0xC0) == 0x80; i++) c = (c << 6) | (*s++ & 0x3F);
    } else if (c >= 0x80) {
        c = '?';
    }
    *p = s;
    if (c >= 0x20 && c < 0x7F) return (int)c - 0x20;
    if (c >= 0xA0 && c <= 0xFF) return 96 + (int)c - 0xA0;
    if (c == 0x2019) return '\'' - 0x20;
    if (c == 0x0153) return -1;                   /* oe, written as two letters */
    return '?' - 0x20;
}

static void clair_gui_add_rect(float x, float y, float w, float h) {
    if (clair_gui.nrects == clair_gui.caprects) {
        size_t cap = clair_gui.caprects ? clair_gui.caprects * 2 : 1024;
        ClairSdlFRect *r = realloc(clair_gui.rects, cap * sizeof(ClairSdlFRect));
        if (!r) clair_gui_fail("out of memory", NULL);
        clair_gui.rects = r;
        clair_gui.caprects = cap;
    }
    ClairSdlFRect *r = &clair_gui.rects[clair_gui.nrects++];
    r->x = x;
    r->y = y;
    r->w = w;
    r->h = h;
}

static void clair_gui_draw_glyph(int g, float x, float y, float k) {
    for (int row = 0; row < 8; row++) {
        unsigned bits = clair_gui_font[g][row];
        float top = roundf(y + row * k), bottom = roundf(y + (row + 1) * k);     /* whole pixels: no gaps */
        int col = 0;
        while (bits) {                            /* runs of pixels side by side: one rectangle each */
            if (!(bits & 1)) { bits >>= 1; col++; continue; }
            int start = col;
            while (bits & 1) { bits >>= 1; col++; }
            float left = roundf(x + start * k), right = roundf(x + col * k);
            clair_gui_add_rect(left, top, right - left, bottom - top);
        }
    }
}

/* text with the 8 x 8 letters (accents included); size: the height of a letter in pixels */
static int64_t clair_gui_text(const char *s, double x, double y, double size) {
    clair_gui_need();
    float k = (float)(size / 8);
    if (!(k > 0) || !s) return 0;
    clair_gui_font_read();
    clair_gui.nrects = 0;
    float cx = (float)x;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        int g = clair_gui_glyph(&p);
        if (g < 0) {
            clair_gui_draw_glyph('o' - 0x20, cx, (float)y, k);
            cx += 8 * k;
            g = 'e' - 0x20;
        }
        clair_gui_draw_glyph(g, cx, (float)y, k);
        cx += 8 * k;
    }
    if (clair_gui.nrects) clair_sdl.RenderFillRects(clair_gui.renderer, clair_gui.rects, (int)clair_gui.nrects);
    return 0;
}

static double clair_gui_text_width(const char *s, double size) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; p && *p; p++) n += (*p & 0xC0) != 0x80;   /* characters, not bytes */
    for (const char *q = s; q && (q = strstr(q, "\xc5\x93")); q += 2) n++;                            /* oe: two letters */
    return (double)n * size;
}

/* gui.image(path): a PNG or BMP image, kept by the graphics card */
static int64_t clair_gui_image_load(const char *path) {
    clair_gui_need();
    if (clair_gui.nimages >= CLAIR_GUI_IMAGES) clair_gui_fail("gui: more than 1024 images", NULL);
    ClairSdlSurface *s = clair_sdl.LoadSurface ? clair_sdl.LoadSurface(path) : clair_sdl.LoadBMP(path);
    if (!s) {
        char msg[300];
        snprintf(msg, sizeof(msg), "%s (%s)", path, clair_sdl.GetError());
        clair_gui_fail("gui: cannot read the image ", msg);
    }
    void *t = clair_sdl.CreateTextureFromSurface(clair_gui.renderer, s);
    int i = clair_gui.nimages++;
    clair_gui.images[i] = t;
    clair_gui.image_w[i] = s->w;
    clair_gui.image_h[i] = s->h;
    clair_sdl.DestroySurface(s);
    if (!t) clair_gui_fail("gui: cannot keep the image ", path);
    return i + 1;
}

static int64_t clair_gui_image_size(int64_t id, int64_t what) {
    if (id < 1 || id > clair_gui.nimages) return 0;
    return what == 0 ? clair_gui.image_w[id - 1] : clair_gui.image_h[id - 1];
}

/* draws the image at x, y with the size w, h, turned by angle degrees around its center */
static int64_t clair_gui_image_draw(int64_t id, double x, double y, double w, double h, double angle) {
    clair_gui_need();
    if (id < 1 || id > clair_gui.nimages) clair_gui_fail("gui: not an image", NULL);
    ClairSdlFRect d = { (float)x, (float)y, (float)w, (float)h };
    if (angle == 0) clair_sdl.RenderTexture(clair_gui.renderer, clair_gui.images[id - 1], NULL, &d);
    else clair_sdl.RenderTextureRotated(clair_gui.renderer, clair_gui.images[id - 1], NULL, &d, angle, NULL, 0);
    return 0;
}

/* w.present(): shows the frame, waits to keep the frame rate */
static int64_t clair_gui_present(void) {
    clair_gui_need();
    clair_sdl.RenderPresent(clair_gui.renderer);
    uint64_t now = clair_sdl.GetTicksNS();
    if (clair_gui.frame_ns && now - clair_gui.last_ns < clair_gui.frame_ns && !clair_gui.stop_after) {
        clair_sdl.DelayNS(clair_gui.frame_ns - (now - clair_gui.last_ns));
        now = clair_sdl.GetTicksNS();
    }
    clair_gui.dt = clair_gui.stop_after ? 1.0 / 60 : (double)(now - clair_gui.last_ns) / 1e9;   /* the tests: always 1/60 s */
    clair_gui.last_ns = now;
    clair_gui.fps_frames++;
    if (now - clair_gui.fps_ns >= 500000000ull) {
        clair_gui.fps = clair_gui.fps_frames * 1e9 / (double)(now - clair_gui.fps_ns);
        clair_gui.fps_frames = 0;
        clair_gui.fps_ns = now;
    }
    return 0;
}

/* frames per second wanted (0: as fast as possible) */
static int64_t clair_gui_set_fps(int64_t n) {
    clair_gui.frame_ns = n > 0 ? 1000000000ull / (uint64_t)n : 0;
    return 0;
}

/* 0 the seconds of the last frame, 1 the seconds since the window opened, 2 the frames per second measured */
static double clair_gui_time(int64_t what) {
    clair_gui_need();
    if (what == 0) return clair_gui.dt;
    if (what == 1) return (double)(clair_sdl.GetTicksNS() - clair_gui.start_ns) / 1e9;
    return clair_gui.fps;
}

static int64_t clair_gui_title(const char *t) {
    clair_gui_need();
    clair_sdl.SetWindowTitle(clair_gui.window, t);
    return 0;
}

/* what is drawn so far, as a PNG (or a BMP when the name ends with .bmp) */
static int64_t clair_gui_screenshot_to(const char *path) {
    ClairSdlSurface *s = clair_sdl.RenderReadPixels(clair_gui.renderer, NULL);
    if (!s) return 0;
    size_t n = strlen(path);
    bool bmp = (n > 4 && strcasecmp(path + n - 4, ".bmp") == 0) || !clair_sdl.SavePNG;
    bool ok = bmp ? clair_sdl.SaveBMP(s, path) : clair_sdl.SavePNG(s, path);
    clair_sdl.DestroySurface(s);
    return ok;
}

static bool clair_gui_screenshot(const char *path) {
    clair_gui_need();
    return clair_gui_screenshot_to(path) != 0;
}

/* the color of a pixel drawn so far: 0 red, 1 green, 2 blue, 3 alpha (-1 outside the window) */
static int64_t clair_gui_pixel(int64_t x, int64_t y, int64_t what) {
    clair_gui_need();
    ClairSdlRect r = { (int)x, (int)y, 1, 1 };
    ClairSdlSurface *s = clair_sdl.RenderReadPixels(clair_gui.renderer, &r);
    if (!s) return -1;
    uint8_t c[4] = { 0, 0, 0, 0 };
    bool ok = clair_sdl.ReadSurfacePixel(s, 0, 0, &c[0], &c[1], &c[2], &c[3]);
    clair_sdl.DestroySurface(s);
    return ok && what >= 0 && what < 4 ? c[what] : -1;
}

/* a button: drawn, and true at the frame where it is clicked (pressed then let go on it) */
static bool clair_gui_button(const char *label, double x, double y, double w, double h) {
    clair_gui_need();
    uint64_t id = 1469598103934665603ULL;                  /* FNV-1a of the label and the place */
    for (const unsigned char *p = (const unsigned char *)label; *p; p++) id = (id ^ *p) * 1099511628211ULL;
    double place[4] = { x, y, w, h };
    const unsigned char *pb = (const unsigned char *)place;
    for (size_t i = 0; i < sizeof(place); i++) id = (id ^ pb[i]) * 1099511628211ULL;
    if (!id) id = 1;
    bool inside = clair_gui.mx >= x && clair_gui.mx < x + w && clair_gui.my >= y && clair_gui.my < y + h;
    bool down = (clair_gui.mouse_now & 1) != 0, was = (clair_gui.mouse_before & 1) != 0;
    bool clicked = false;
    if (inside && down && !was) clair_gui.active_button = id;
    if (!down && was) {
        clicked = inside && clair_gui.active_button == id;
        if (clair_gui.active_button == id) clair_gui.active_button = 0;
    }
    bool held = down && clair_gui.active_button == id;
    uint8_t r = clair_gui.cr, g = clair_gui.cg, b = clair_gui.cb, a = clair_gui.ca;
    int shade = held ? 70 : 52;
    clair_sdl.SetRenderDrawColor(clair_gui.renderer, (uint8_t)shade, (uint8_t)(shade + 6), (uint8_t)(shade + 20), 230);
    ClairSdlFRect box = { (float)x, (float)y, (float)w, (float)h };
    clair_sdl.RenderFillRect(clair_gui.renderer, &box);
    clair_sdl.SetRenderDrawColor(clair_gui.renderer, 120, 130, 155, 255);
    clair_sdl.RenderRect(clair_gui.renderer, &box);
    double size = h * 0.42 < 8 ? 8 : h * 0.42;
    double tw = clair_gui_text_width(label, size);
    if (tw > w - 8) { size = size * (w - 8) / tw; tw = clair_gui_text_width(label, size); }
    clair_sdl.SetRenderDrawColor(clair_gui.renderer, 240, 242, 248, 255);
    clair_gui_text(label, x + (w - tw) / 2, y + (h - size) / 2 + (held ? 1 : 0), size);
    clair_gui_set_color(r, g, b, a);
    return clicked;
}

/* the named colors: English and French; 0 red, 1 green, 2 blue */
static int64_t clair_gui_color_name(const char *name, int64_t what) {
    static const struct { const char *en, *fr; int r, g, b; } colors[] = {
        { "black", "noir", 0, 0, 0 }, { "white", "blanc", 255, 255, 255 }, { "red", "rouge", 230, 41, 55 },
        { "green", "vert", 0, 160, 70 }, { "blue", "bleu", 30, 100, 230 }, { "yellow", "jaune", 253, 220, 40 },
        { "orange", "orange", 255, 150, 0 }, { "purple", "violet", 140, 60, 200 }, { "pink", "rose", 255, 120, 180 },
        { "brown", "marron", 130, 85, 50 }, { "gray", "gris", 128, 128, 128 }, { "grey", "gris", 128, 128, 128 },
        { "cyan", "cyan", 0, 200, 220 }, { "magenta", "magenta", 220, 0, 200 },
        { "dark", "sombre", 16, 18, 24 }, { "light", "clair", 235, 237, 242 }, { "sky", "ciel", 110, 180, 245 }
    };
    for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
        if (strcasecmp(name, colors[i].en) == 0 || strcasecmp(name, colors[i].fr) == 0)
            return what == 0 ? colors[i].r : what == 1 ? colors[i].g : colors[i].b;
    }
    clair_gui_fail("gui.color(): unknown color (black/noir, white/blanc, red/rouge, green/vert, blue/bleu, "
                   "yellow/jaune, orange, purple/violet, pink/rose, brown/marron, gray/gris, cyan, magenta, "
                   "dark/sombre, light/clair, sky/ciel): ", name);
}

/* "#ffcc00" or "ffcc00": 0 red, 1 green, 2 blue */
static int64_t clair_gui_hex(const char *text, int64_t what) {
    const char *p = text[0] == '#' ? text + 1 : text;
    int v[6];
    if (strlen(p) != 6) clair_gui_fail("gui.hex() needs 6 hexadecimal digits, like \"#ffcc00\": ", text);
    for (int i = 0; i < 6; i++) {
        char c = (char)tolower((unsigned char)p[i]);
        v[i] = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        if (v[i] < 0) clair_gui_fail("gui.hex() needs 6 hexadecimal digits, like \"#ffcc00\": ", text);
    }
    int k = (int)(what < 0 ? 0 : what > 2 ? 2 : what);
    return v[2 * k] * 16 + v[2 * k + 1];
}

/* w.close(): the window goes away; running() gives false, and gui.open can open another one */
static int64_t clair_gui_close(void) {
    if (clair_gui.open && !clair_gui.closing) {
        clair_gui.closing = 1;                     /* what is drawn after close() still has a window */
        clair_sdl.HideWindow(clair_gui.window);
    }
    clair_gui.quit = 1;
    return 0;
}
