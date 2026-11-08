/*
 * V0.66 : le module gui declare lui-meme la partie de SDL3 qu'il emploie (src/clair_gui_rt.c) et charge
 * SDL3 par dlopen.  Ce programme compare ces declarations aux en-tetes de SDL3 (tests/run_tests.sh le
 * compile quand <SDL3/SDL.h> est la) : tailles, places des champs, valeurs des constantes.
 */
#define _GNU_SOURCE
#include <SDL3/SDL.h>
#include <ctype.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct ClairListObj { void *data; size_t len; size_t capacity; size_t elem_size; } ClairListObj;
typedef ClairListObj *ClairList;
static void *clair_rt_arena_alloc(size_t n) { return malloc(n); }

#include "../src/clair_gui_rt.c"

#define SAME(a, b) _Static_assert((long long)(a) == (long long)(b), #a " != " #b)
#define FIELD(T1, f1, T2, f2) SAME(offsetof(T1, f1), offsetof(T2, f2)); SAME(sizeof(((T1 *)0)->f1), sizeof(((T2 *)0)->f2))

SAME(sizeof(ClairSdlEvent), sizeof(SDL_Event));
SAME(sizeof(ClairSdlFRect), sizeof(SDL_FRect));
SAME(sizeof(ClairSdlRect), sizeof(SDL_Rect));
SAME(sizeof(ClairSdlFPoint), sizeof(SDL_FPoint));
SAME(sizeof(ClairSdlVertex), sizeof(SDL_Vertex));
FIELD(ClairSdlVertex, color, SDL_Vertex, color);
FIELD(ClairSdlVertex, tex_coord, SDL_Vertex, tex_coord);
FIELD(ClairSdlSurface, w, SDL_Surface, w);
FIELD(ClairSdlSurface, h, SDL_Surface, h);
FIELD(ClairSdlEvent, key.scancode, SDL_Event, key.scancode);
FIELD(ClairSdlEvent, key.key, SDL_Event, key.key);
FIELD(ClairSdlEvent, key.down, SDL_Event, key.down);
FIELD(ClairSdlEvent, text.text, SDL_Event, text.text);
FIELD(ClairSdlEvent, motion.x, SDL_Event, motion.x);
FIELD(ClairSdlEvent, motion.y, SDL_Event, motion.y);
FIELD(ClairSdlEvent, button.button, SDL_Event, button.button);
FIELD(ClairSdlEvent, button.x, SDL_Event, button.x);
FIELD(ClairSdlEvent, button.y, SDL_Event, button.y);
FIELD(ClairSdlEvent, wheel.y, SDL_Event, wheel.y);
FIELD(ClairSdlEvent, wheel.direction, SDL_Event, wheel.direction);
FIELD(ClairSdlEvent, window.data1, SDL_Event, window.data1);
FIELD(ClairSdlEvent, window.data2, SDL_Event, window.data2);
SAME(CLAIR_SDL_INIT_VIDEO, SDL_INIT_VIDEO);
SAME(CLAIR_SDL_WINDOW_BORDERLESS, SDL_WINDOW_BORDERLESS);
SAME(CLAIR_SDL_WINDOW_RESIZABLE, SDL_WINDOW_RESIZABLE);
SAME(CLAIR_SDL_BLENDMODE_BLEND, SDL_BLENDMODE_BLEND);
SAME(CLAIR_SDL_EVENT_QUIT, SDL_EVENT_QUIT);
SAME(CLAIR_SDL_EVENT_WINDOW_RESIZED, SDL_EVENT_WINDOW_RESIZED);
SAME(CLAIR_SDL_EVENT_WINDOW_CLOSE_REQUESTED, SDL_EVENT_WINDOW_CLOSE_REQUESTED);
SAME(CLAIR_SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_DOWN);
SAME(CLAIR_SDL_EVENT_KEY_UP, SDL_EVENT_KEY_UP);
SAME(CLAIR_SDL_EVENT_TEXT_INPUT, SDL_EVENT_TEXT_INPUT);
SAME(CLAIR_SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_MOTION);
SAME(CLAIR_SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_EVENT_MOUSE_BUTTON_DOWN);
SAME(CLAIR_SDL_EVENT_MOUSE_BUTTON_UP, SDL_EVENT_MOUSE_BUTTON_UP);
SAME(CLAIR_SDL_EVENT_MOUSE_WHEEL, SDL_EVENT_MOUSE_WHEEL);
SAME(CLAIR_SDL_MOUSEWHEEL_FLIPPED, SDL_MOUSEWHEEL_FLIPPED);
SAME(CLAIR_SDL_SCANCODES, SDL_SCANCODE_COUNT);
SAME(44, SDL_SCANCODE_SPACE);
SAME(41, SDL_SCANCODE_ESCAPE);
SAME(20, SDL_SCANCODE_Q);
SAME(6, SDL_SCANCODE_C);
SAME(23, SDL_SCANCODE_T);
SAME(45, SDL_SCANCODE_MINUS);
SAME(46, SDL_SCANCODE_EQUALS);
SAME(86, SDL_SCANCODE_KP_MINUS);
SAME(87, SDL_SCANCODE_KP_PLUS);
SAME(79, SDL_SCANCODE_RIGHT);
SAME(80, SDL_SCANCODE_LEFT);

/* the functions themselves are all called by tests/gui_module.cl */

int main(void) {
    (void)clair_gui_available;
    puts("gui_abi : les declarations de SDL3 sont les bonnes");
    return 0;
}
