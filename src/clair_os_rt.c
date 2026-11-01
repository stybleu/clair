/*
 * V0.56: the module os (import os): programs, files, folders, time.
 *
 * Copied into the C of the programs that import os (tools/embed_file.cmake),
 * after the runtime of strings and lists, which it uses.  Its #include lines
 * are written by the compiler at the top of the C (see codegen_c.c).
 *
 * A program started by os.start() writes into two pipes; what it writes is
 * read whenever the module looks at it (read, running, wait), so it never
 * blocks on a full pipe.  os.run() is os.start() then wait().
 */

static int clair_os_argc = 0;
static char **clair_os_argv = NULL;
static char clair_os_err[512] = "";                  /* os.error(): what went wrong last */

static void clair_os_failed(const char *what, const char *path) {
    snprintf(clair_os_err, sizeof(clair_os_err), "%s%s%s: %s", what, path ? " " : "", path ? path : "", strerror(errno));
}

_Noreturn static void clair_os_fail(const char *what, const char *detail) {
    fprintf(stderr, "Clair runtime error: %s%s\n", what, detail ? detail : "");
    exit(EXIT_FAILURE);
}

static const char *clair_os_text(const char *s, size_t n) {
    char *d = clair_rt_arena_alloc(n + 1);
    if (n) memcpy(d, s, n);
    d[n] = '\0';
    return d;
}

static ClairList clair_os_strings(const char **items, size_t n) {
    return clair_rt_list_make(items, n, sizeof(const char *));
}

/* ------------------------------------------------------------------ */
/* Arguments, environment, time                                       */
/* ------------------------------------------------------------------ */

/* os.args(): what follows the name of the program on the command line */
static ClairList clair_os_args(void) {
    size_t n = clair_os_argc > 1 ? (size_t)clair_os_argc - 1 : 0;
    const char **items = n ? malloc(n * sizeof(const char *)) : NULL;
    if (n && !items) clair_os_fail("out of memory", NULL);
    for (size_t i = 0; i < n; i++) items[i] = clair_os_text(clair_os_argv[i + 1], strlen(clair_os_argv[i + 1]));
    ClairList l = clair_os_strings(items, n);
    free(items);
    return l;
}

static const char *clair_os_error(void) { return clair_os_text(clair_os_err, strlen(clair_os_err)); }

static const char *clair_os_env(const char *name, const char *otherwise) {
    const char *v = getenv(name);
    return v ? clair_os_text(v, strlen(v)) : otherwise;
}

static bool clair_os_set_env(const char *name, const char *value) {
    if (setenv(name, value, 1) != 0) { clair_os_failed("cannot set", name); return false; }
    return true;
}

/* 0: seconds since 1970 (the clock of the phone), 1: seconds of a clock that only goes forward */
static double clair_os_time(int64_t kind) {
    struct timespec t;
    clock_gettime(kind == 0 ? CLOCK_REALTIME : CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

static int64_t clair_os_sleep(double seconds) {
    if (!(seconds > 0)) return 0;
    struct timespec t;
    t.tv_sec = (time_t)seconds;
    t.tv_nsec = (long)((seconds - (double)t.tv_sec) * 1e9);
    while (nanosleep(&t, &t) != 0 && errno == EINTR) {}
    return 0;
}

static int64_t clair_os_exit(int64_t code) {
    fflush(NULL);
    (exit)((int)code);                                     /* even inside a route of the module web */
}

/* ------------------------------------------------------------------ */
/* Files and folders                                                  */
/* ------------------------------------------------------------------ */

static int clair_os_name_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* os.listdir(path): the names in a folder, sorted, without . and .. */
static ClairList clair_os_listdir(const char *path) {
    DIR *d = opendir(path);
    if (!d) {
        clair_os_failed("cannot open the folder", path);
        return clair_os_strings(NULL, 0);
    }
    size_t n = 0, cap = 0;
    const char **items = NULL;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        if (n == cap) {
            cap = cap ? cap * 2 : 32;
            const char **grown = realloc(items, cap * sizeof(const char *));
            if (!grown) { closedir(d); free(items); clair_os_fail("out of memory", NULL); }
            items = grown;
        }
        items[n++] = clair_os_text(e->d_name, strlen(e->d_name));
    }
    closedir(d);
    if (n > 1) qsort(items, n, sizeof(const char *), clair_os_name_cmp);
    ClairList l = clair_os_strings(items, n);
    free(items);
    return l;
}

/* 0 exists, 1 is a folder, 2 is a file, 3 size in bytes (-1: none), 4 changed at (seconds since 1970, -1: none) */
static int64_t clair_os_stat(const char *path, int64_t what) {
    struct stat st;
    if (stat(path, &st) != 0) return what >= 3 ? -1 : 0;
    switch (what) {
        case 0: return 1;
        case 1: return S_ISDIR(st.st_mode) ? 1 : 0;
        case 2: return S_ISREG(st.st_mode) ? 1 : 0;
        case 3: return (int64_t)st.st_size;
        default: return (int64_t)st.st_mtime;
    }
}

/* os.mkdir(path): the folder and the folders above it (nothing to do if it exists) */
static bool clair_os_mkdir(const char *path) {
    size_t n = strlen(path);
    if (n == 0) { errno = ENOENT; clair_os_failed("cannot make the folder", path); return false; }
    char *p = malloc(n + 1);
    if (!p) clair_os_fail("out of memory", NULL);
    memcpy(p, path, n + 1);
    for (size_t i = 1; i <= n; i++) {
        if (p[i] != '/' && p[i] != '\0') continue;
        char c = p[i];
        p[i] = '\0';
        if (mkdir(p, 0755) != 0 && errno != EEXIST) {
            clair_os_failed("cannot make the folder", p);
            free(p);
            return false;
        }
        p[i] = c;
    }
    free(p);
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        clair_os_failed("cannot make the folder", path);
        return false;
    }
    return true;
}

/* a file, or an empty folder */
static bool clair_os_remove(const char *path) {
    if (remove(path) != 0) { clair_os_failed("cannot remove", path); return false; }
    return true;
}

static bool clair_os_rename(const char *from, const char *to) {
    if (rename(from, to) != 0) { clair_os_failed("cannot rename", from); return false; }
    return true;
}

static bool clair_os_copy_file(const char *from, const char *to) {
    int in = open(from, O_RDONLY);
    if (in < 0) { clair_os_failed("cannot read", from); return false; }
    struct stat st;
    fstat(in, &st);
    int out = open(to, O_WRONLY | O_CREAT | O_TRUNC, (st.st_mode & 0777) | 0600);
    if (out < 0) { clair_os_failed("cannot write", to); close(in); return false; }
    char buf[65536];
    bool ok = true;
    for (;;) {
        ssize_t r = read(in, buf, sizeof(buf));
        if (r < 0 && errno == EINTR) continue;
        if (r < 0) { clair_os_failed("cannot read", from); ok = false; break; }
        if (r == 0) break;
        for (ssize_t w = 0; w < r;) {
            ssize_t k = write(out, buf + w, (size_t)(r - w));
            if (k < 0 && errno == EINTR) continue;
            if (k <= 0) { clair_os_failed("cannot write", to); ok = false; break; }
            w += k;
        }
        if (!ok) break;
    }
    close(in);
    if (close(out) != 0 && ok) { clair_os_failed("cannot write", to); ok = false; }
    return ok;
}

static const char *clair_os_cwd(void) {
    char buf[4096];
    if (!getcwd(buf, sizeof(buf))) { clair_os_failed("cannot read the current folder", NULL); return ""; }
    return clair_os_text(buf, strlen(buf));
}

static bool clair_os_chdir(const char *path) {
    if (chdir(path) != 0) { clair_os_failed("cannot go to", path); return false; }
    return true;
}

/* where a program of PATH is ("" when there is none) */
static const char *clair_os_which(const char *name) {
    if (strchr(name, '/')) return access(name, X_OK) == 0 ? clair_os_text(name, strlen(name)) : "";
    const char *path = getenv("PATH");
    if (!path) path = "/usr/bin:/bin";
    char full[4096];
    while (*path) {
        size_t n = strcspn(path, ":");
        if (n && n + strlen(name) + 2 < sizeof(full)) {
            snprintf(full, sizeof(full), "%.*s/%s", (int)n, path, name);
            struct stat st;
            if (stat(full, &st) == 0 && S_ISREG(st.st_mode) && access(full, X_OK) == 0) return clair_os_text(full, strlen(full));
        }
        path += n;
        if (*path == ':') path++;
    }
    return "";
}

/* 0 the name (a/b.cl: b.cl), 1 the folder (a/b.cl: a), 2 the extension (a/b.cl: .cl) */
static const char *clair_os_path(int64_t what, const char *p) {
    size_t n = strlen(p);
    while (n > 1 && p[n - 1] == '/') n--;                  /* a/b/ is a/b */
    size_t slash = n;
    while (slash > 0 && p[slash - 1] != '/') slash--;
    if (what == 0) return clair_os_text(p + slash, n - slash);
    if (what == 1) {
        if (slash == 0) return ".";
        size_t end = slash - 1;
        while (end > 0 && p[end - 1] == '/') end--;
        return end == 0 ? "/" : clair_os_text(p, end);
    }
    const char *name = p + slash;
    size_t nl = n - slash;
    for (size_t i = nl; i > 1; i--) {
        if (name[i - 1] == '.') return clair_os_text(name + i - 1, nl - i + 1);   /* .bashrc has none */
    }
    return "";
}

static const char *clair_os_join(const char *a, const char *b) {
    if (b[0] == '/' || a[0] == '\0') return clair_os_text(b, strlen(b));
    size_t na = strlen(a), nb = strlen(b);
    int slash = a[na - 1] != '/';
    char *d = clair_rt_arena_alloc(na + nb + 2);
    memcpy(d, a, na);
    if (slash) d[na] = '/';
    memcpy(d + na + slash, b, nb + 1);
    return d;
}

/* ------------------------------------------------------------------ */
/* Programs                                                           */
/* ------------------------------------------------------------------ */

#define CLAIR_OS_PROCS 64

typedef struct {
    int64_t id;                        /* 0: a free slot */
    pid_t pid;
    int in, out, err;                  /* pipes, -1 when closed */
    char *got[2];                      /* what it wrote and nobody read yet: 0 output, 1 errors */
    size_t got_len[2], got_cap[2];
    int done;                          /* it stopped; code is known */
    int64_t code;                      /* its exit code, or 128 + the signal that stopped it */
} ClairOsProc;

static ClairOsProc clair_os_procs[CLAIR_OS_PROCS];
static int64_t clair_os_next_id = 0;
static int64_t clair_os_ended_id[CLAIR_OS_PROCS], clair_os_ended_code[CLAIR_OS_PROCS];   /* programs forgotten, with their code */
static int clair_os_ended_at = 0;

static int clair_os_ended(int64_t id, int64_t *code) {
    for (int i = 0; i < CLAIR_OS_PROCS; i++) {
        if (id > 0 && clair_os_ended_id[i] == id) { *code = clair_os_ended_code[i]; return 1; }
    }
    return 0;
}

static ClairOsProc *clair_os_proc(int64_t id) {
    for (int i = 0; i < CLAIR_OS_PROCS; i++) if (id > 0 && clair_os_procs[i].id == id) return &clair_os_procs[i];
    return NULL;
}

static void clair_os_keep(ClairOsProc *p, int which, const char *s, size_t n) {
    if (p->got_len[which] + n + 1 > p->got_cap[which]) {
        size_t cap = p->got_cap[which] ? p->got_cap[which] : 4096;
        while (cap < p->got_len[which] + n + 1) cap *= 2;
        char *g = realloc(p->got[which], cap);
        if (!g) clair_os_fail("out of memory", NULL);
        p->got[which] = g;
        p->got_cap[which] = cap;
    }
    memcpy(p->got[which] + p->got_len[which], s, n);
    p->got_len[which] += n;
}

/* reads what the program has written, waiting at most ms milliseconds for something (0: not at all) */
static void clair_os_pump(ClairOsProc *p, int ms) {
    for (int turn = 0; turn < 64; turn++) {              /* a program that writes without end: 4 Mo at a time */
        struct pollfd fds[2];
        int map[2], nf = 0;
        if (p->out >= 0) { fds[nf].fd = p->out; fds[nf].events = POLLIN; fds[nf].revents = 0; map[nf++] = 0; }
        if (p->err >= 0) { fds[nf].fd = p->err; fds[nf].events = POLLIN; fds[nf].revents = 0; map[nf++] = 1; }
        if (nf == 0) return;
        int r = poll(fds, (nfds_t)nf, ms);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return;
        for (int k = 0; k < nf; k++) {
            if (!(fds[k].revents & (POLLIN | POLLHUP | POLLERR))) continue;
            char buf[65536];
            ssize_t n = read(fds[k].fd, buf, sizeof(buf));
            if (n > 0) {
                clair_os_keep(p, map[k], buf, (size_t)n);
            } else if (n == 0 || (errno != EINTR && errno != EAGAIN)) {
                close(fds[k].fd);
                if (map[k] == 0) p->out = -1; else p->err = -1;
            }
        }
        ms = 0;                                            /* then only what is already there */
    }
}

static void clair_os_reap(ClairOsProc *p, int block) {
    if (p->done) return;
    int status = 0;
    pid_t r;
    while ((r = waitpid(p->pid, &status, block ? 0 : WNOHANG)) < 0 && errno == EINTR) {}
    if (r == p->pid) {
        p->done = 1;
        p->code = WIFEXITED(status) ? WEXITSTATUS(status) : WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1;
    } else if (r < 0) {
        p->done = 1;
        p->code = 1;
    }
    if (p->done && p->in >= 0) {                           /* nobody reads its input any more */
        close(p->in);
        p->in = -1;
    }
}

/* os.start(command): starts it and gives its number (0 when it cannot start); merge: errors with the output */
static int64_t clair_os_start(ClairList command, bool merge) {
    if (!command || command->len == 0) clair_os_fail("os.start() needs the program to run: os.start([\"ls\", \"-l\"])", NULL);
    ClairOsProc *p = NULL;
    for (int i = 0; i < CLAIR_OS_PROCS && !p; i++) if (clair_os_procs[i].id == 0) p = &clair_os_procs[i];
    if (!p) {
        errno = EAGAIN;
        clair_os_failed("more than 64 programs started and not waited for", NULL);
        return 0;
    }
    int pin[2], pout[2], perr[2];
    if (pipe(pin) != 0) { clair_os_failed("cannot start", NULL); return 0; }
    if (pipe(pout) != 0) { close(pin[0]); close(pin[1]); clair_os_failed("cannot start", NULL); return 0; }
    if (pipe(perr) != 0) { close(pin[0]); close(pin[1]); close(pout[0]); close(pout[1]); clair_os_failed("cannot start", NULL); return 0; }
    size_t argc = command->len;
    char **argv = malloc((argc + 1) * sizeof(char *));
    if (!argv) clair_os_fail("out of memory", NULL);
    for (size_t i = 0; i < argc; i++) argv[i] = (char *)((const char **)command->data)[i];
    argv[argc] = NULL;
    signal(SIGPIPE, SIG_IGN);
    fflush(NULL);                                          /* nothing written twice by the two processes */
    pid_t pid = fork();
    if (pid < 0) {
        clair_os_failed("cannot start", argv[0]);
        free(argv);
        close(pin[0]); close(pin[1]); close(pout[0]); close(pout[1]); close(perr[0]); close(perr[1]);
        return 0;
    }
    if (pid == 0) {
        setpgid(0, 0);                                     /* os.stop() stops what it starts too */
        signal(SIGPIPE, SIG_DFL);
        dup2(pin[0], 0);
        dup2(pout[1], 1);
        dup2(merge ? pout[1] : perr[1], 2);
        struct rlimit rl;
        int top = getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur < 65536 ? (int)rl.rlim_cur : 4096;
        for (int fd = 3; fd < top; fd++) close(fd);          /* not the sockets of a web server, not other pipes */
        execvp(argv[0], argv);
        char msg[600];
        int k = snprintf(msg, sizeof(msg), "cannot run %s: %s\n", argv[0], strerror(errno));
        if (k > 0) { ssize_t w = write(2, msg, (size_t)k); (void)w; }
        _exit(127);
    }
    free(argv);
    close(pin[0]);
    close(pout[1]);
    close(perr[1]);
    memset(p, 0, sizeof(*p));
    p->id = ++clair_os_next_id;
    p->pid = pid;
    p->in = pin[1];
    p->out = pout[0];
    p->err = perr[0];
    fcntl(p->out, F_SETFL, O_NONBLOCK);
    fcntl(p->err, F_SETFL, O_NONBLOCK);
    return p->id;
}

static void clair_os_forget(ClairOsProc *p) {
    clair_os_ended_id[clair_os_ended_at] = p->id;
    clair_os_ended_code[clair_os_ended_at] = p->code;
    clair_os_ended_at = (clair_os_ended_at + 1) % CLAIR_OS_PROCS;
    if (p->in >= 0) close(p->in);
    if (p->out >= 0) close(p->out);
    if (p->err >= 0) close(p->err);
    free(p->got[0]);
    free(p->got[1]);
    memset(p, 0, sizeof(*p));
}

/* what it wrote since the last read: 0 its output, 1 its errors ("" when nothing) */
static const char *clair_os_read(int64_t id, int64_t which) {
    ClairOsProc *p = clair_os_proc(id);
    if (!p) return "";
    clair_os_pump(p, 0);
    int w = which ? 1 : 0;
    const char *r = clair_os_text(p->got[w] ? p->got[w] : "", p->got_len[w]);
    p->got_len[w] = 0;
    /* stopped, waited for, and everything read: the slot is free again */
    clair_os_reap(p, 0);
    if (p->done && p->out < 0 && p->err < 0 && p->got_len[0] == 0 && p->got_len[1] == 0 && p->in < 0) clair_os_forget(p);
    return r;
}

static bool clair_os_write(int64_t id, const char *text) {
    ClairOsProc *p = clair_os_proc(id);
    if (!p || p->in < 0) { errno = EPIPE; clair_os_failed("cannot write to the program", NULL); return false; }
    size_t n = strlen(text);
    while (n) {
        /* while writing, its output is read too: a program that answers line by line never blocks */
        struct pollfd f = { p->in, POLLOUT, 0 };
        int r = poll(&f, 1, 50);
        if (r < 0 && errno != EINTR) break;
        if (r <= 0) { clair_os_pump(p, 0); continue; }
        ssize_t w = write(p->in, text, n);
        if (w < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        if (w <= 0) { clair_os_failed("cannot write to the program", NULL); return false; }
        text += w;
        n -= (size_t)w;
    }
    return true;
}

/* no more input: the program reads the end of its input */
static bool clair_os_close_input(int64_t id) {
    ClairOsProc *p = clair_os_proc(id);
    if (!p || p->in < 0) return false;
    close(p->in);
    p->in = -1;
    return true;
}

static bool clair_os_running(int64_t id) {
    ClairOsProc *p = clair_os_proc(id);
    if (!p) return false;                                  /* unknown, or stopped and forgotten */
    clair_os_pump(p, 0);
    clair_os_reap(p, 0);
    return !p->done;
}

/* stops it, and what it started */
static bool clair_os_stop(int64_t id) {
    ClairOsProc *p = clair_os_proc(id);
    if (!p) return false;
    clair_os_reap(p, 0);
    if (p->done) return false;
    kill(-p->pid, SIGKILL);
    kill(p->pid, SIGKILL);
    return true;
}

/* waits until it stops (at most seconds, < 0: for ever) and gives its code; -1 when it still runs */
static int64_t clair_os_wait(int64_t id, double seconds) {
    ClairOsProc *p = clair_os_proc(id);
    int64_t code;
    if (!p) return clair_os_ended(id, &code) ? code : -1;
    if (p->in >= 0 && seconds < 0) {                       /* nothing more to give it */
        close(p->in);
        p->in = -1;
    }
    double end = clair_os_time(1) + (seconds < 0 ? 1e18 : seconds);
    for (;;) {
        clair_os_pump(p, 20);
        clair_os_reap(p, 0);
        if (p->done) {
            clair_os_pump(p, 0);
            /* what it started may keep the pipes open: what is there is kept, then they are closed */
            if (p->out >= 0) { close(p->out); p->out = -1; }
            if (p->err >= 0) { close(p->err); p->err = -1; }
            return p->code;
        }
        if (clair_os_time(1) >= end) return -1;
    }
}
