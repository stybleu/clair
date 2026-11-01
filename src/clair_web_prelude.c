/*
 * V0.55: what a program that imports web needs before everything else.
 *
 * A runtime error inside a route (a list index out of bounds, a missing
 * dictionary key...) must not stop the server: the request gets a page
 * "500 Internal Server Error" with the message, and the server goes on.
 * Every runtime error of Clair writes its message on stderr and calls exit():
 * here, while a route runs, stderr is written into memory and exit() jumps
 * back to the server loop.  Outside a route, both work as usual.
 *
 * Copied right after the #include lines of the C of the program
 * (tools/embed_file.cmake), before any runtime code.
 */
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <poll.h>
#include <setjmp.h>
#include <strings.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int clair_web_in_handler = 0;                 /* a route is running (on the main thread) */
static jmp_buf clair_web_jump;
static FILE *clair_web_capture = NULL;               /* stderr while a route runs */

static FILE *clair_web_real_stderr(void) { return stderr; }

/* the routes run on the main thread; the threads of a big matrix product keep the usual stderr and exit() */
static int clair_web_routing(void) { return clair_web_in_handler && gettid() == getpid(); }

static FILE *clair_web_errf(void) {
    return clair_web_routing() && clair_web_capture ? clair_web_capture : clair_web_real_stderr();
}

_Noreturn static void clair_web_exit(int code) {
    if (clair_web_routing()) {
        clair_web_in_handler = 0;
        longjmp(clair_web_jump, 1);
    }
    (exit)(code);
}

#undef stderr
#define stderr (clair_web_errf())
#define exit(code) clair_web_exit(code)

static int64_t clair_web_handler(void);              /* the routes, written by the compiler */
