/*
 * V0.55: the web server of `import web`.
 *
 * Copied into the C of the programs that import web (tools/embed_file.cmake),
 * after the runtime of strings, lists, dictionaries and the arena, which it
 * uses, and after clair_web_prelude.c.
 *
 * One request at a time: accept a connection, read the request, call the
 * routes the compiler wrote (clair_web_handler), send the answer, then give
 * back everything the request made: its strings and objects (the arena goes
 * back to where it was), its lists and dictionaries.  Nothing a route makes
 * can outlive its request (Clair has no global variables), so this is safe.
 *
 * Also here: the files of static/, PHP pages (php-cgi, or php with a small
 * shim), the templates of templates/ ({{ x }}, {% if %}, {% for %},
 * {% include %}, {% extends %} and {% block %}), the files of data/, and the
 * files sent by forms.
 */

/* ------------------------------------------------------------------ */
/* Small tools                                                        */
/* ------------------------------------------------------------------ */

typedef struct { char *p; size_t len, cap; } ClairWebBuf;

_Noreturn static void clair_web_oom(void) {
    fputs("Clair web: out of memory\n", clair_web_real_stderr());
    (exit)(EXIT_FAILURE);
}

static void clair_web_buf_add(ClairWebBuf *b, const char *s, size_t n) {
    if (n > SIZE_MAX / 4 - b->len) clair_web_oom();
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 256;
        while (cap < b->len + n + 1) cap *= 2;
        char *p = realloc(b->p, cap);
        if (!p) clair_web_oom();
        b->p = p;
        b->cap = cap;
    }
    if (n) memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

static void clair_web_buf_str(ClairWebBuf *b, const char *s) { clair_web_buf_add(b, s, strlen(s)); }
static void clair_web_buf_ch(ClairWebBuf *b, char c) { clair_web_buf_add(b, &c, 1); }

static char *clair_web_dup(const char *s, size_t n) {
    char *d = malloc(n + 1);
    if (!d) clair_web_oom();
    if (n) memcpy(d, s, n);
    d[n] = '\0';
    return d;
}

/* a string of the program: in the arena, given back at the end of the request */
static const char *clair_web_text_of(const char *s, size_t n) {
    char *d = clair_rt_arena_alloc(n + 1);
    if (n) memcpy(d, s, n);
    d[n] = '\0';
    return d;
}

_Noreturn static void clair_web_fail(const char *what, const char *detail) {
    fprintf(stderr, "Clair runtime error: %s%s\n", what, detail ? detail : "");
    exit(EXIT_FAILURE);
}

static int clair_web_hex(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* %41 is A (a % not followed by two hex digits stays); with plus, + is a space: in place */
static size_t clair_web_decode(char *s, size_t n, int plus) {
    size_t w = 0;
    for (size_t i = 0; i < n; i++) {
        int h1, h2;
        if (s[i] == '%' && i + 2 < n && (h1 = clair_web_hex((unsigned char)s[i + 1])) >= 0 &&
            (h2 = clair_web_hex((unsigned char)s[i + 2])) >= 0) {
            s[w++] = (char)(h1 * 16 + h2);
            i += 2;
        } else if (plus && s[i] == '+') {
            s[w++] = ' ';
        } else {
            s[w++] = s[i];
        }
    }
    s[w] = '\0';
    return w;
}

static int clair_web_ends_with(const char *s, const char *end) {
    size_t a = strlen(s), b = strlen(end);
    return a >= b && strcmp(s + a - b, end) == 0;
}

static double clair_web_ms_since(const struct timespec *t0) {
    struct timespec t1;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return (double)(t1.tv_sec - t0->tv_sec) * 1e3 + (double)(t1.tv_nsec - t0->tv_nsec) / 1e6;
}

/* ------------------------------------------------------------------ */
/* The request and the response being made                            */
/* ------------------------------------------------------------------ */

#define CLAIR_WEB_HEAD_MAX 65536
#define CLAIR_WEB_BODY_MAX ((size_t)32 << 20)
#define CLAIR_WEB_HEADERS_MAX 128
#define CLAIR_WEB_PARTS_MAX 256

static struct {
    char *head;                                   /* request line and headers, cut in place */
    char *body;
    size_t body_len;
    const char *method, *target, *query;
    char *path;                                   /* decoded */
    const char *hname[CLAIR_WEB_HEADERS_MAX], *hvalue[CLAIR_WEB_HEADERS_MAX];
    size_t nh;
    char ip[64];
    int parts_done;                               /* the form sent as multipart/form-data, read once */
    char *pname[CLAIR_WEB_PARTS_MAX], *pvalue[CLAIR_WEB_PARTS_MAX];
    int pfile[CLAIR_WEB_PARTS_MAX];
    size_t np;
} clair_web_req;

static struct {
    int set;
    int status;
    const char *type, *body, *headers;
    size_t body_len;
    char *owned;                                  /* a body made here (malloc) */
    char *file;                                   /* a file to send instead of a body */
} clair_web_resp;

static int clair_web_public = 0;                  /* listening for the network: no detail in error pages */
static int64_t clair_web_port = 0;
static char clair_web_host[64] = "127.0.0.1";
static unsigned long clair_web_upload_count = 0;

static const char *clair_web_header(const char *lower_name) {
    for (size_t i = 0; i < clair_web_req.nh; i++)
        if (strcmp(clair_web_req.hname[i], lower_name) == 0) return clair_web_req.hvalue[i];
    return NULL;
}

static void clair_web_request_free(void) {
    free(clair_web_req.head);
    free(clair_web_req.body);
    free(clair_web_req.path);
    for (size_t i = 0; i < clair_web_req.np; i++) {
        if (clair_web_req.pfile[i] && clair_web_req.pvalue[i][0]) unlink(clair_web_req.pvalue[i]);
        free(clair_web_req.pname[i]);
        free(clair_web_req.pvalue[i]);
    }
    memset(&clair_web_req, 0, sizeof(clair_web_req));
}

static void clair_web_response_free(void) {
    free(clair_web_resp.owned);
    free(clair_web_resp.file);
    memset(&clair_web_resp, 0, sizeof(clair_web_resp));
}

static const char *clair_web_reason(int status) {
    switch (status) {
        case 200: return "OK"; case 201: return "Created"; case 202: return "Accepted"; case 204: return "No Content";
        case 301: return "Moved Permanently"; case 302: return "Found"; case 303: return "See Other";
        case 304: return "Not Modified"; case 307: return "Temporary Redirect"; case 308: return "Permanent Redirect";
        case 400: return "Bad Request"; case 401: return "Unauthorized"; case 403: return "Forbidden";
        case 404: return "Not Found"; case 405: return "Method Not Allowed"; case 408: return "Request Timeout";
        case 409: return "Conflict"; case 410: return "Gone"; case 413: return "Content Too Large";
        case 415: return "Unsupported Media Type"; case 418: return "I'm a teapot"; case 422: return "Unprocessable Content";
        case 429: return "Too Many Requests"; case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error"; case 501: return "Not Implemented"; case 502: return "Bad Gateway";
        case 503: return "Service Unavailable"; case 504: return "Gateway Timeout"; case 505: return "HTTP Version Not Supported";
        default: return status < 300 ? "OK" : status < 400 ? "Redirect" : status < 500 ? "Client Error" : "Server Error";
    }
}

static void clair_web_escape_into(ClairWebBuf *b, const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        switch (s[i]) {
            case '&': clair_web_buf_str(b, "&amp;"); break;
            case '<': clair_web_buf_str(b, "&lt;"); break;
            case '>': clair_web_buf_str(b, "&gt;"); break;
            case '"': clair_web_buf_str(b, "&quot;"); break;
            case '\'': clair_web_buf_str(b, "&#39;"); break;
            default: clair_web_buf_ch(b, s[i]);
        }
    }
}

/* the answer is a small page made here: 404, 500... (detail: shown under the title, may be NULL) */
static void clair_web_page(int status, const char *detail) {
    ClairWebBuf b = { 0 };
    char title[96];
    snprintf(title, sizeof(title), "%d %s", status, clair_web_reason(status));
    clair_web_buf_str(&b, "<!doctype html>\n<meta charset=\"utf-8\">\n<meta name=\"viewport\" content=\"width=device-width\">\n<title>");
    clair_web_buf_str(&b, title);
    clair_web_buf_str(&b, "</title>\n<h1>");
    clair_web_buf_str(&b, title);
    clair_web_buf_str(&b, "</h1>\n");
    if (detail && detail[0]) {
        clair_web_buf_str(&b, "<pre>");
        clair_web_escape_into(&b, detail, strlen(detail));
        clair_web_buf_str(&b, "</pre>\n");
    }
    free(clair_web_resp.owned);
    free(clair_web_resp.file);
    memset(&clair_web_resp, 0, sizeof(clair_web_resp));
    clair_web_resp.set = 1;
    clair_web_resp.status = status;
    clair_web_resp.type = "text/html; charset=utf-8";
    clair_web_resp.owned = b.p;
    clair_web_resp.body = b.p;
    clair_web_resp.body_len = b.len;
    clair_web_resp.headers = "";
}

static const char *clair_web_mime(const char *path) {
    static const char *const types[][2] = {
        { ".html", "text/html; charset=utf-8" }, { ".htm", "text/html; charset=utf-8" },
        { ".css", "text/css; charset=utf-8" }, { ".js", "text/javascript; charset=utf-8" },
        { ".mjs", "text/javascript; charset=utf-8" }, { ".json", "application/json" },
        { ".txt", "text/plain; charset=utf-8" }, { ".md", "text/plain; charset=utf-8" },
        { ".csv", "text/csv; charset=utf-8" }, { ".xml", "application/xml" },
        { ".svg", "image/svg+xml" }, { ".png", "image/png" }, { ".jpg", "image/jpeg" },
        { ".jpeg", "image/jpeg" }, { ".gif", "image/gif" }, { ".webp", "image/webp" },
        { ".ico", "image/x-icon" }, { ".avif", "image/avif" }, { ".pdf", "application/pdf" },
        { ".wasm", "application/wasm" }, { ".mp3", "audio/mpeg" }, { ".ogg", "audio/ogg" },
        { ".wav", "audio/wav" }, { ".mp4", "video/mp4" }, { ".webm", "video/webm" },
        { ".woff", "font/woff" }, { ".woff2", "font/woff2" }, { ".ttf", "font/ttf" },
        { ".otf", "font/otf" }, { ".zip", "application/zip" }, { ".cl", "text/plain; charset=utf-8" },
        { ".webmanifest", "application/manifest+json" }
    };
    const char *dot = strrchr(path, '.');
    if (dot && !strchr(dot, '/')) {
        for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
            const char *a = dot, *b = types[i][0];
            while (*a && *b && tolower((unsigned char)*a) == *b) { a++; b++; }
            if (!*a && !*b) return types[i][1];
        }
    }
    return "application/octet-stream";
}

/* ------------------------------------------------------------------ */
/* Reading a request                                                  */
/* ------------------------------------------------------------------ */

/* 0: a request was read; -1: nothing to answer (connection closed, too slow); else the status to answer */
static int clair_web_read(int fd) {
    size_t cap = 4096, len = 0, end = 0, head_len = 0;
    char *buf = malloc(cap);
    if (!buf) clair_web_oom();
    for (;;) {
        if (len + 1 >= cap) {
            if (cap >= CLAIR_WEB_HEAD_MAX) { free(buf); return 431; }
            cap *= 2;
            char *p = realloc(buf, cap);
            if (!p) clair_web_oom();
            buf = p;
        }
        ssize_t r = recv(fd, buf + len, cap - len - 1, 0);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) { free(buf); return len ? 400 : -1; }
        size_t from = len > 2 ? len - 2 : 0;
        len += (size_t)r;
        buf[len] = '\0';
        for (size_t i = from; i < len && !end; i++) {
            if (buf[i] != '\n') continue;
            if (i + 1 < len && buf[i + 1] == '\n') { head_len = i; end = i + 2; }
            else if (i + 2 < len && buf[i + 1] == '\r' && buf[i + 2] == '\n') { head_len = i; end = i + 3; }
        }
        if (end) break;
    }
    clair_web_req.head = buf;

    /* the bytes of the body already read are copied before the head is cut */
    size_t have = len - end;
    char *early = have ? clair_web_dup(buf + end, have) : NULL;
    buf[head_len] = '\0';

    char *line = buf, *next = strchr(line, '\n');
    if (next) *next++ = '\0';
    size_t ll = strlen(line);
    if (ll && line[ll - 1] == '\r') line[ll - 1] = '\0';
    char *sp1 = strchr(line, ' ');
    if (!sp1) { free(early); return 400; }
    *sp1 = '\0';
    char *target = sp1 + 1, *sp2 = strchr(target, ' ');
    if (!sp2) { free(early); return 400; }
    *sp2 = '\0';
    if (strncmp(sp2 + 1, "HTTP/1.", 7) != 0) { free(early); return strncmp(sp2 + 1, "HTTP/", 5) == 0 ? 505 : 400; }
    size_t ml = strlen(line);
    if (ml == 0 || ml > 15) { free(early); return 400; }
    for (size_t i = 0; i < ml; i++) if (line[i] < 'A' || line[i] > 'Z') { free(early); return 400; }
    clair_web_req.method = line;

    while (next && *next) {
        char *h = next;
        next = strchr(h, '\n');
        if (next) *next++ = '\0';
        size_t hl = strlen(h);
        if (hl && h[hl - 1] == '\r') h[--hl] = '\0';
        if (!hl) break;
        if (h[0] == ' ' || h[0] == '\t') continue;          /* an old folded line: ignored */
        char *colon = strchr(h, ':');
        if (!colon || colon == h) { free(early); return 400; }
        *colon = '\0';
        for (char *q = h; *q; q++) {
            unsigned char c = (unsigned char)*q;
            if (c <= ' ' || c >= 127) { free(early); return 400; }
            *q = (char)tolower(c);
        }
        char *v = colon + 1;
        while (*v == ' ' || *v == '\t') v++;
        size_t vl = strlen(v);
        while (vl && (v[vl - 1] == ' ' || v[vl - 1] == '\t')) v[--vl] = '\0';
        if (clair_web_req.nh >= CLAIR_WEB_HEADERS_MAX) { free(early); return 431; }
        clair_web_req.hname[clair_web_req.nh] = h;
        clair_web_req.hvalue[clair_web_req.nh++] = v;
    }

    /* http://host/path?x is /path?x */
    if (strncmp(target, "http://", 7) == 0 || strncmp(target, "https://", 8) == 0) {
        char *slash = strchr(strstr(target, "//") + 2, '/');
        target = slash ? slash : (char *)"/";
    }
    if (target[0] != '/') { free(early); return 400; }
    clair_web_req.target = target;
    char *qm = strchr(target, '?');
    size_t path_len = qm ? (size_t)(qm - target) : strlen(target);
    clair_web_req.query = qm ? qm + 1 : "";
    clair_web_req.path = clair_web_dup(target, path_len);
    size_t dl = clair_web_decode(clair_web_req.path, path_len, 0);
    if (strlen(clair_web_req.path) != dl) { free(early); return 400; }    /* %00 */

    const char *te = clair_web_header("transfer-encoding");
    if (te && strcmp(te, "identity") != 0) { free(early); return 501; }
    size_t cl = 0;
    const char *cls = clair_web_header("content-length");
    if (cls) {
        if (!*cls) { free(early); return 400; }
        for (const char *q = cls; *q; q++) {
            if (*q < '0' || *q > '9') { free(early); return 400; }
            if (cl > CLAIR_WEB_BODY_MAX) break;
            cl = cl * 10 + (size_t)(*q - '0');
        }
        if (cl > CLAIR_WEB_BODY_MAX) { free(early); return 413; }
    }
    clair_web_req.body = malloc(cl + 1);
    if (!clair_web_req.body) clair_web_oom();
    size_t got = have < cl ? have : cl;
    if (got) memcpy(clair_web_req.body, early, got);
    free(early);
    while (got < cl) {
        ssize_t r = recv(fd, clair_web_req.body + got, cl - got, 0);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return -1;
        got += (size_t)r;
    }
    clair_web_req.body[cl] = '\0';
    clair_web_req.body_len = cl;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Forms: a=1&b=2, cookies, multipart/form-data                       */
/* ------------------------------------------------------------------ */

static void clair_web_dict_put(ClairDict d, const char *k, size_t kn, const char *v, size_t vn) {
    char *key = clair_web_dup(k, kn);
    for (size_t i = 0; i < d->len; i++) {
        if (strcmp(d->keys[i], key) == 0) { free(key); return; }       /* the first one wins, like Flask */
    }
    const char **slot = clair_rt_dict_set_slot(d, key);
    *slot = clair_web_text_of(v, vn);
    free(key);
}

/* a=1&b=x+y%21 (sep '&'), or a cookie header: a=1; b=2 (sep ';') */
static void clair_web_fill_pairs(ClairDict d, const char *s, size_t n, char sep) {
    size_t i = 0;
    while (i < n) {
        size_t j = i;
        while (j < n && s[j] != sep) j++;
        size_t a = i, b = j;
        while (a < b && (s[a] == ' ' || s[a] == '\t')) a++;
        if (a < b) {
            size_t eq = a;
            while (eq < b && s[eq] != '=') eq++;
            char *k = clair_web_dup(s + a, eq - a);
            char *v = eq < b ? clair_web_dup(s + eq + 1, b - eq - 1) : clair_web_dup("", 0);
            size_t kn = clair_web_decode(k, strlen(k), sep == '&');
            size_t vn = clair_web_decode(v, strlen(v), sep == '&');
            if (kn) clair_web_dict_put(d, k, kn, v, vn);
            free(k);
            free(v);
        }
        i = j + 1;
    }
}

static const char *clair_web_tmpdir(void) {
    const char *t = getenv("TMPDIR");
    return t && *t ? t : "/tmp";
}

/* the value of name="..." in a Content-Disposition header (malloc), or NULL */
static char *clair_web_disposition(const char *h, const char *key) {
    size_t kl = strlen(key);
    for (const char *p = h; (p = strstr(p, key)) != NULL; p += kl) {
        if (p != h && p[-1] != ' ' && p[-1] != ';') continue;
        const char *v = p + kl;
        if (*v != '=') continue;
        v++;
        if (*v == '"') {
            const char *e = strchr(v + 1, '"');
            if (!e) return NULL;
            return clair_web_dup(v + 1, (size_t)(e - v - 1));
        }
        size_t n = strcspn(v, "; ");
        return clair_web_dup(v, n);
    }
    return NULL;
}

static void clair_web_read_parts(void) {
    if (clair_web_req.parts_done) return;
    clair_web_req.parts_done = 1;
    const char *ct = clair_web_header("content-type");
    if (!ct || strncasecmp(ct, "multipart/form-data", 19) != 0) return;
    char *boundary = clair_web_disposition(ct, "boundary");
    if (!boundary || !*boundary || strlen(boundary) > 200) { free(boundary); return; }
    char delim[210];
    snprintf(delim, sizeof(delim), "--%s", boundary);
    free(boundary);
    size_t dl = strlen(delim);
    const char *body = clair_web_req.body, *endb = body + clair_web_req.body_len;
    const char *p = memmem(body, clair_web_req.body_len, delim, dl);
    while (p && clair_web_req.np < CLAIR_WEB_PARTS_MAX) {
        p += dl;
        if (endb - p >= 2 && p[0] == '-' && p[1] == '-') break;           /* --boundary-- : the end */
        if (endb - p >= 2 && p[0] == '\r' && p[1] == '\n') p += 2;
        else if (p < endb && p[0] == '\n') p += 1;
        const char *hend = memmem(p, (size_t)(endb - p), "\r\n\r\n", 4);
        size_t hsep = 4;
        if (!hend) break;
        const char *data = hend + hsep;
        const char *next = memmem(data, (size_t)(endb - data), delim, dl);
        if (!next) break;
        const char *dend = next;
        if (dend - data >= 2 && dend[-2] == '\r' && dend[-1] == '\n') dend -= 2;
        char *heads = clair_web_dup(p, (size_t)(hend - p));
        char *name = NULL, *filename = NULL;
        for (char *h = heads; h && *h;) {
            char *nl = strstr(h, "\r\n");
            if (nl) *nl = '\0';
            if (strncasecmp(h, "content-disposition:", 20) == 0) {
                name = clair_web_disposition(h + 20, "name");
                filename = clair_web_disposition(h + 20, "filename");
            }
            h = nl ? nl + 2 : NULL;
        }
        free(heads);
        if (name) {
            size_t k = clair_web_req.np;
            clair_web_req.pname[k] = name;
            if (filename) {
                /* the file is written to a temporary file; web.keep() keeps it */
                char path[512];
                snprintf(path, sizeof(path), "%s/clair_upload_%ld_%lu", clair_web_tmpdir(), (long)getpid(),
                         ++clair_web_upload_count);
                int ok = 0;
                if (filename[0]) {
                    int out = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
                    if (out >= 0) {
                        size_t left = (size_t)(dend - data);
                        const char *w = data;
                        ok = 1;
                        while (left) {
                            ssize_t r = write(out, w, left);
                            if (r < 0 && errno == EINTR) continue;
                            if (r <= 0) { ok = 0; break; }
                            w += r;
                            left -= (size_t)r;
                        }
                        close(out);
                        if (!ok) unlink(path);
                    }
                }
                clair_web_req.pvalue[k] = ok ? clair_web_dup(path, strlen(path)) : clair_web_dup("", 0);
                clair_web_req.pfile[k] = 1;
                /* the form gets the name of the file, files gets where it is */
                if (clair_web_req.np + 1 < CLAIR_WEB_PARTS_MAX) {
                    clair_web_req.np++;
                    clair_web_req.pname[k + 1] = clair_web_dup(name, strlen(name));
                    clair_web_req.pvalue[k + 1] = filename;
                    clair_web_req.pfile[k + 1] = 0;
                    filename = NULL;
                }
            } else {
                clair_web_req.pvalue[k] = clair_web_dup(data, (size_t)(dend - data));
                clair_web_req.pfile[k] = 0;
            }
            clair_web_req.np++;
        }
        free(filename);
        p = next;
    }
}

/* ------------------------------------------------------------------ */
/* What the module web asks for                                        */
/* ------------------------------------------------------------------ */

/* 0 the method, 1 the path, 2 the body, 3 the address of the client, 4 the query string, 5 the URL asked */
static const char *clair_web_text(int64_t what) {
    const char *s = "";
    switch (what) {
        case 0: s = clair_web_req.method; break;
        case 1: s = clair_web_req.path; break;
        case 2: return clair_web_req.body ? clair_web_text_of(clair_web_req.body, strlen(clair_web_req.body)) : "";
        case 3: s = clair_web_req.ip; break;
        case 4: s = clair_web_req.query; break;
        case 5: s = clair_web_req.target; break;
        default: break;
    }
    return s ? clair_web_text_of(s, strlen(s)) : "";
}

static void clair_web_read_session(ClairDict d);

/* 0 the arguments of the URL, 1 the form, 2 the headers (names in lower case), 3 the cookies, 4 the files sent,
   5 the session (V0.57) */
static ClairDict clair_web_pairs(int64_t what) {
    ClairDict d = clair_rt_dict_make(NULL, NULL, 0, sizeof(const char *));
    if (!clair_web_req.method) return d;
    if (what == 0) {
        clair_web_fill_pairs(d, clair_web_req.query, strlen(clair_web_req.query), '&');
    } else if (what == 1 || what == 4) {
        const char *ct = clair_web_header("content-type");
        if (what == 1 && ct && strncasecmp(ct, "application/x-www-form-urlencoded", 33) == 0) {
            clair_web_fill_pairs(d, clair_web_req.body, clair_web_req.body_len, '&');
        } else if (ct && strncasecmp(ct, "multipart/form-data", 19) == 0) {
            clair_web_read_parts();
            for (size_t i = 0; i < clair_web_req.np; i++) {
                if (clair_web_req.pfile[i] != (what == 4)) continue;
                clair_web_dict_put(d, clair_web_req.pname[i], strlen(clair_web_req.pname[i]),
                                   clair_web_req.pvalue[i], strlen(clair_web_req.pvalue[i]));
            }
        }
    } else if (what == 2) {
        for (size_t i = 0; i < clair_web_req.nh; i++)
            clair_web_dict_put(d, clair_web_req.hname[i], strlen(clair_web_req.hname[i]),
                               clair_web_req.hvalue[i], strlen(clair_web_req.hvalue[i]));
    } else if (what == 3) {
        for (size_t i = 0; i < clair_web_req.nh; i++)
            if (strcmp(clair_web_req.hname[i], "cookie") == 0)
                clair_web_fill_pairs(d, clair_web_req.hvalue[i], strlen(clair_web_req.hvalue[i]), ';');
    } else if (what == 5) {
        clair_web_read_session(d);
    }
    return d;
}

/* the answer of the route: kept until it is sent (the strings live until the end of the request) */
static int64_t clair_web_reply(int64_t status, const char *type, const char *body, const char *headers) {
    if (status < 100 || status > 599) {
        char n[32];
        snprintf(n, sizeof(n), "%lld", (long long)status);
        clair_web_fail("an HTTP status is a number from 100 to 599, got ", n);
    }
    free(clair_web_resp.owned);
    free(clair_web_resp.file);
    memset(&clair_web_resp, 0, sizeof(clair_web_resp));
    clair_web_resp.set = 1;
    clair_web_resp.status = (int)status;
    clair_web_resp.headers = headers;
    if (strcmp(type, "@file") == 0) {                 /* web.file(path) */
        clair_web_resp.file = clair_web_dup(body, strlen(body));
        clair_web_resp.type = clair_web_mime(body);
        clair_web_resp.body = "";
    } else {
        clair_web_resp.type = type;
        clair_web_resp.body = body;
        clair_web_resp.body_len = strlen(body);
    }
    return 0;
}

static const char *clair_web_escape(const char *s) {
    ClairWebBuf b = { 0 };
    clair_web_buf_add(&b, "", 0);
    clair_web_escape_into(&b, s, strlen(s));
    const char *r = clair_web_text_of(b.p, b.len);
    free(b.p);
    return r;
}

/* for a URL: letters, digits and - . _ ~ stay, the rest is %XX */
static const char *clair_web_quote(const char *s) {
    static const char hex[] = "0123456789ABCDEF";
    size_t n = strlen(s);
    char *d = clair_rt_arena_alloc(n * 3 + 1), *w = d;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') *w++ = (char)c;
        else { *w++ = '%'; *w++ = hex[c >> 4]; *w++ = hex[c & 15]; }
    }
    *w = '\0';
    return d;
}

/* "a\"b" : a string written for JSON, with its quotes */
static const char *clair_web_json(const char *s) {
    ClairWebBuf b = { 0 };
    clair_web_buf_ch(&b, '"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        char e[8];
        switch (*p) {
            case '"': clair_web_buf_str(&b, "\\\""); break;
            case '\\': clair_web_buf_str(&b, "\\\\"); break;
            case '\n': clair_web_buf_str(&b, "\\n"); break;
            case '\r': clair_web_buf_str(&b, "\\r"); break;
            case '\t': clair_web_buf_str(&b, "\\t"); break;
            default:
                if (*p < 0x20) { snprintf(e, sizeof(e), "\\u%04x", *p); clair_web_buf_str(&b, e); }
                else clair_web_buf_ch(&b, (char)*p);
        }
    }
    clair_web_buf_ch(&b, '"');
    const char *r = clair_web_text_of(b.p, b.len);
    free(b.p);
    return r;
}

/* kind 0: an int (what int(s) reads), 1: a float */
static bool clair_web_is_number(const char *s, int64_t kind) {
    if (!*s || isspace((unsigned char)*s)) return false;
    char *end = NULL;
    errno = 0;
    if (kind == 0) {
        long long v = strtoll(s, &end, 10);
        (void)v;
        return errno == 0 && end && *end == '\0';
    }
    for (const char *p = s; *p; p++)                      /* what float() reads: no 0x1p3, no inf */
        if (!isdigit((unsigned char)*p) && *p != '.' && *p != 'e' && *p != 'E' && *p != '+' && *p != '-') return false;
    double v = strtod(s, &end);
    return errno == 0 && end && *end == '\0' && v == v && v - v == 0;
}

/* ------------------------------------------------------------------ */
/* data/: web.load, web.save, web.append                              */
/* ------------------------------------------------------------------ */

static void clair_web_data_name(const char *name) {
    size_t n = strlen(name);
    int ok = n > 0 && n <= 100 && name[0] != '.';
    for (size_t i = 0; ok && i < n; i++) {
        unsigned char c = (unsigned char)name[i];
        ok = isalnum(c) || c == '-' || c == '_' || c == '.';
    }
    if (!ok) clair_web_fail("a name of data/ is made of letters, digits, - _ and . (not first), got ", name);
}

static int clair_web_read_file(const char *path, ClairWebBuf *b) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    char tmp[16384];
    for (;;) {
        ssize_t r = read(fd, tmp, sizeof(tmp));
        if (r < 0 && errno == EINTR) continue;
        if (r < 0) { close(fd); return 0; }
        if (r == 0) break;
        clair_web_buf_add(b, tmp, (size_t)r);
    }
    close(fd);
    clair_web_buf_add(b, "", 0);
    return 1;
}

static void clair_web_write_all(int fd, const char *p, size_t n, const char *path) {
    while (n) {
        ssize_t w = write(fd, p, n);
        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) clair_web_fail("cannot write ", path);
        p += w;
        n -= (size_t)w;
    }
}

/* 0: the text of data/name (or value when there is none), 1: writes value, 2: adds the line value */
static const char *clair_web_data(int64_t op, const char *name, const char *value) {
    clair_web_data_name(name);
    char path[160], tmp[200];
    snprintf(path, sizeof(path), "data/%s", name);
    if (op == 0) {
        ClairWebBuf b = { 0 };
        if (!clair_web_read_file(path, &b)) return value;
        const char *r = clair_web_text_of(b.p, strlen(b.p));
        free(b.p);
        return r;
    }
    if (mkdir("data", 0755) != 0 && errno != EEXIST) clair_web_fail("cannot make the folder ", "data");
    if (op == 1) {
        snprintf(tmp, sizeof(tmp), "data/.%s.%ld.tmp", name, (long)getpid());
        int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) clair_web_fail("cannot write ", path);
        clair_web_write_all(fd, value, strlen(value), path);
        close(fd);
        if (rename(tmp, path) != 0) { unlink(tmp); clair_web_fail("cannot write ", path); }
    } else {
        int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd < 0) clair_web_fail("cannot write ", path);
        size_t n = strlen(value);
        char *line = malloc(n + 2);
        if (!line) clair_web_oom();
        memcpy(line, value, n);
        line[n] = '\n';
        clair_web_write_all(fd, line, n + 1, path);
        free(line);
        close(fd);
    }
    return "";
}

/* web.keep(where, path): a file sent by a form goes to path (1), or 0 when there is none */
static int64_t clair_web_keep(const char *from, const char *to) {
    if (!*from) return 0;
    if (rename(from, to) == 0) return 1;
    ClairWebBuf b = { 0 };
    if (!clair_web_read_file(from, &b)) return 0;
    int fd = open(to, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { free(b.p); clair_web_fail("cannot write ", to); }
    clair_web_write_all(fd, b.p, b.len, to);
    close(fd);
    free(b.p);
    unlink(from);
    return 1;
}

/* ------------------------------------------------------------------ */
/* V0.57: SHA-256, HMAC, PBKDF2 (signed sessions, passwords)          */
/* ------------------------------------------------------------------ */

typedef struct { uint32_t h[8]; uint64_t len; unsigned char buf[64]; size_t n; } ClairSha256;

static const uint32_t clair_sha_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define CLAIR_ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void clair_sha_block(uint32_t h[8], const unsigned char *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | (uint32_t)p[4 * i + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = CLAIR_ROR(w[i - 15], 7) ^ CLAIR_ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = CLAIR_ROR(w[i - 2], 17) ^ CLAIR_ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], k = h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = k + (CLAIR_ROR(e, 6) ^ CLAIR_ROR(e, 11) ^ CLAIR_ROR(e, 25)) + ((e & f) ^ (~e & g)) + clair_sha_k[i] + w[i];
        uint32_t t2 = (CLAIR_ROR(a, 2) ^ CLAIR_ROR(a, 13) ^ CLAIR_ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        k = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += k;
}

static void clair_sha_init(ClairSha256 *s) {
    static const uint32_t iv[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    memcpy(s->h, iv, sizeof(iv));
    s->len = 0;
    s->n = 0;
}

static void clair_sha_add(ClairSha256 *s, const void *data, size_t n) {
    const unsigned char *p = data;
    s->len += n;
    if (s->n) {
        size_t take = 64 - s->n < n ? 64 - s->n : n;
        memcpy(s->buf + s->n, p, take);
        s->n += take;
        p += take;
        n -= take;
        if (s->n < 64) return;
        clair_sha_block(s->h, s->buf);
        s->n = 0;
    }
    while (n >= 64) {
        clair_sha_block(s->h, p);
        p += 64;
        n -= 64;
    }
    memcpy(s->buf, p, n);
    s->n = n;
}

static void clair_sha_end(ClairSha256 *s, unsigned char out[32]) {
    uint64_t bits = s->len * 8;
    unsigned char pad = 0x80;
    clair_sha_add(s, &pad, 1);
    unsigned char zero = 0;
    while (s->n != 56) clair_sha_add(s, &zero, 1);
    unsigned char lenb[8];
    for (int i = 0; i < 8; i++) lenb[i] = (unsigned char)(bits >> (56 - 8 * i));
    clair_sha_add(s, lenb, 8);
    for (int i = 0; i < 8; i++) {
        out[4 * i] = (unsigned char)(s->h[i] >> 24);
        out[4 * i + 1] = (unsigned char)(s->h[i] >> 16);
        out[4 * i + 2] = (unsigned char)(s->h[i] >> 8);
        out[4 * i + 3] = (unsigned char)s->h[i];
    }
}

/* HMAC-SHA256: the key is taken once, then each message costs two blocks of SHA-256 */
typedef struct { ClairSha256 inner, outer; } ClairHmac;

static void clair_hmac_init(ClairHmac *h, const void *key, size_t kn) {
    unsigned char k[64] = { 0 }, pad[64];
    if (kn > 64) {
        ClairSha256 s;
        clair_sha_init(&s);
        clair_sha_add(&s, key, kn);
        clair_sha_end(&s, k);
    } else if (kn) {
        memcpy(k, key, kn);
    }
    for (int i = 0; i < 64; i++) pad[i] = k[i] ^ 0x36;
    clair_sha_init(&h->inner);
    clair_sha_add(&h->inner, pad, 64);
    for (int i = 0; i < 64; i++) pad[i] = k[i] ^ 0x5c;
    clair_sha_init(&h->outer);
    clair_sha_add(&h->outer, pad, 64);
}

static void clair_hmac(const ClairHmac *key, const void *msg, size_t n, unsigned char out[32]) {
    ClairSha256 in = key->inner, ou = key->outer;
    unsigned char t[32];
    clair_sha_add(&in, msg, n);
    clair_sha_end(&in, t);
    clair_sha_add(&ou, t, 32);
    clair_sha_end(&ou, out);
}

/* PBKDF2-HMAC-SHA256, 32 bytes (what Werkzeug and hashlib.pbkdf2_hmac give) */
static void clair_pbkdf2(const char *pw, size_t pn, const char *salt, size_t sn, unsigned long iters, unsigned char out[32]) {
    ClairHmac h;
    clair_hmac_init(&h, pw, pn);
    unsigned char *msg = malloc(sn + 4), u[32];
    if (!msg) clair_web_oom();
    memcpy(msg, salt, sn);
    msg[sn] = 0; msg[sn + 1] = 0; msg[sn + 2] = 0; msg[sn + 3] = 1;
    clair_hmac(&h, msg, sn + 4, u);
    free(msg);
    memcpy(out, u, 32);
    for (unsigned long i = 1; i < iters; i++) {
        clair_hmac(&h, u, 32, u);
        for (int j = 0; j < 32; j++) out[j] ^= u[j];
    }
}

/* the same answer whatever the bytes: no time to guess them from */
static int clair_web_same(const unsigned char *a, const unsigned char *b, size_t n) {
    unsigned char d = 0;
    for (size_t i = 0; i < n; i++) d |= a[i] ^ b[i];
    return d == 0;
}

static void clair_web_random_bytes(unsigned char *out, size_t n) {
    int fd = open("/dev/urandom", O_RDONLY);
    size_t got = 0;
    while (fd >= 0 && got < n) {
        ssize_t r = read(fd, out + got, n - got);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) break;
        got += (size_t)r;
    }
    if (fd >= 0) close(fd);
    if (got < n) clair_web_fail("cannot read /dev/urandom", NULL);
}

static void clair_web_hex_into(char *out, const unsigned char *b, size_t n) {
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) { out[2 * i] = hex[b[i] >> 4]; out[2 * i + 1] = hex[b[i] & 15]; }
    out[2 * n] = '\0';
}

/* web.random_token(n): n random bytes, in hexadecimal */
static const char *clair_web_random(int64_t n) {
    if (n < 1 || n > 1024) clair_web_fail("web.random_token() takes 1 to 1024 bytes", NULL);
    unsigned char *b = malloc((size_t)n);
    char *h = clair_rt_arena_alloc((size_t)n * 2 + 1);
    if (!b) clair_web_oom();
    clair_web_random_bytes(b, (size_t)n);
    clair_web_hex_into(h, b, (size_t)n);
    free(b);
    return h;
}

/* ---- passwords: the format of Werkzeug, pbkdf2:sha256:600000$salt$hash ---- */

#define CLAIR_WEB_PBKDF2_ROUNDS 600000UL

static const char *clair_web_hash_password(const char *pw) {
    static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    unsigned char r[16], dk[32];
    char salt[17], hex[65];
    clair_web_random_bytes(r, sizeof(r));
    for (int i = 0; i < 16; i++) salt[i] = chars[r[i] % 62];
    salt[16] = '\0';
    clair_pbkdf2(pw, strlen(pw), salt, 16, CLAIR_WEB_PBKDF2_ROUNDS, dk);
    clair_web_hex_into(hex, dk, 32);
    char *out = clair_rt_arena_alloc(128);
    snprintf(out, 128, "pbkdf2:sha256:%lu$%s$%s", CLAIR_WEB_PBKDF2_ROUNDS, salt, hex);
    return out;
}

/* web.check_password(hash, password): also the hashes made by Werkzeug with pbkdf2:sha256 */
static bool clair_web_check_password(const char *hash, const char *pw) {
    const char *p = hash;
    unsigned long rounds = 260000;                       /* pbkdf2:sha256 without a number: Werkzeug 2.0 */
    if (strncmp(p, "pbkdf2:sha256", 13) != 0) return false;
    p += 13;
    if (*p == ':') {
        char *end = NULL;
        rounds = strtoul(p + 1, &end, 10);
        if (!end || end == p + 1 || rounds == 0 || rounds > 100000000UL) return false;
        p = end;
    }
    if (*p != '$') return false;
    const char *salt = p + 1, *dollar = strchr(salt, '$');
    if (!dollar || strlen(dollar + 1) != 64) return false;
    unsigned char want[32], dk[32];
    for (int i = 0; i < 32; i++) {
        int a = clair_web_hex((unsigned char)dollar[1 + 2 * i]), b = clair_web_hex((unsigned char)dollar[2 + 2 * i]);
        if (a < 0 || b < 0) return false;
        want[i] = (unsigned char)(a * 16 + b);
    }
    clair_pbkdf2(pw, strlen(pw), salt, (size_t)(dollar - salt), rounds, dk);
    return clair_web_same(dk, want, 32);
}

/* ---- signed sessions: the cookie clair_session=payload.expiry.signature ---- */

#define CLAIR_WEB_SESSION_DAYS 31

static unsigned char clair_web_key[32];
static int clair_web_key_ready = 0;

/* the secret key: $CLAIR_SECRET_KEY, or data/.secret_key, made once at random */
static void clair_web_load_key(void) {
    if (clair_web_key_ready) return;
    const char *env = getenv("CLAIR_SECRET_KEY");
    if (env && *env) {
        ClairSha256 s;
        clair_sha_init(&s);
        clair_sha_add(&s, env, strlen(env));
        clair_sha_end(&s, clair_web_key);
        clair_web_key_ready = 1;
        return;
    }
    ClairWebBuf b = { 0 };
    int ok = clair_web_read_file("data/.secret_key", &b) && b.len >= 64;
    for (int i = 0; ok && i < 32; i++) {
        int x = clair_web_hex((unsigned char)b.p[2 * i]), y = clair_web_hex((unsigned char)b.p[2 * i + 1]);
        if (x < 0 || y < 0) ok = 0;
        else clair_web_key[i] = (unsigned char)(x * 16 + y);
    }
    free(b.p);
    if (!ok) {
        char hex[65];
        clair_web_random_bytes(clair_web_key, 32);
        clair_web_hex_into(hex, clair_web_key, 32);
        if (mkdir("data", 0755) != 0 && errno != EEXIST) clair_web_fail("cannot make the folder ", "data");
        int fd = open("data/.secret_key", O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd < 0) clair_web_fail("cannot write ", "data/.secret_key");
        clair_web_write_all(fd, hex, 64, "data/.secret_key");
        clair_web_write_all(fd, "\n", 1, "data/.secret_key");
        close(fd);
    }
    clair_web_key_ready = 1;
}

static const char clair_web_b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static void clair_web_b64_into(ClairWebBuf *out, const unsigned char *p, size_t n) {
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)p[i] << 16 | (i + 1 < n ? (uint32_t)p[i + 1] << 8 : 0) | (i + 2 < n ? p[i + 2] : 0);
        clair_web_buf_ch(out, clair_web_b64[v >> 18]);
        clair_web_buf_ch(out, clair_web_b64[(v >> 12) & 63]);
        if (i + 1 < n) clair_web_buf_ch(out, clair_web_b64[(v >> 6) & 63]);
        if (i + 2 < n) clair_web_buf_ch(out, clair_web_b64[v & 63]);
    }
}

static int clair_web_b64_value(char c) {
    const char *at = c ? strchr(clair_web_b64, c) : NULL;
    return at ? (int)(at - clair_web_b64) : -1;
}

/* 0 when s is not base64url */
static int clair_web_b64_decode(const char *s, size_t n, ClairWebBuf *out) {
    uint32_t v = 0;
    int bits = 0;
    for (size_t i = 0; i < n; i++) {
        int x = clair_web_b64_value(s[i]);
        if (x < 0) return 0;
        v = v << 6 | (uint32_t)x;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            clair_web_buf_ch(out, (char)((v >> bits) & 255));
        }
    }
    clair_web_buf_add(out, "", 0);
    return 1;
}

static void clair_web_session_mac(const char *text, size_t n, unsigned char mac[32]) {
    ClairHmac h;
    clair_web_load_key();
    clair_hmac_init(&h, clair_web_key, 32);
    ClairWebBuf msg = { 0 };
    clair_web_buf_str(&msg, "clair-session|");
    clair_web_buf_add(&msg, text, n);
    clair_hmac(&h, msg.p, msg.len, mac);
    free(msg.p);
}

static void clair_web_quote_into(ClairWebBuf *b, const char *s) {
    static const char hex[] = "0123456789ABCDEF";
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (isalnum(*p) || *p == '-' || *p == '.' || *p == '_' || *p == '~') clair_web_buf_ch(b, (char)*p);
        else { clair_web_buf_ch(b, '%'); clair_web_buf_ch(b, hex[*p >> 4]); clair_web_buf_ch(b, hex[*p & 15]); }
    }
}

/* web.with_session(r, values): the Set-Cookie of a session holding values, signed, for 31 days */
static const char *clair_web_session_cookie(ClairDict values) {
    ClairWebBuf payload = { 0 };
    clair_web_buf_add(&payload, "", 0);
    for (size_t i = 0; values && i < values->len; i++) {
        if (i) clair_web_buf_ch(&payload, '&');
        clair_web_quote_into(&payload, values->keys[i]);
        clair_web_buf_ch(&payload, '=');
        clair_web_quote_into(&payload, ((const char **)values->values)[i]);
    }
    ClairWebBuf v = { 0 };
    clair_web_b64_into(&v, (const unsigned char *)payload.p, payload.len);
    free(payload.p);
    char exp[32];
    snprintf(exp, sizeof(exp), ".%lld", (long long)time(NULL) + CLAIR_WEB_SESSION_DAYS * 86400LL);
    clair_web_buf_str(&v, exp);
    unsigned char mac[32];
    clair_web_session_mac(v.p, v.len, mac);
    clair_web_buf_ch(&v, '.');
    clair_web_b64_into(&v, mac, 32);
    if (v.len > 3800) {
        free(v.p);
        clair_web_fail("a session holds at most 3 KB: keep big things in data/ and their name in the session", NULL);
    }
    ClairWebBuf c = { 0 };
    clair_web_buf_str(&c, "clair_session=");
    clair_web_buf_add(&c, v.p, v.len);
    char attrs[96];
    snprintf(attrs, sizeof(attrs), "; Path=/; Max-Age=%d; HttpOnly; SameSite=Lax", CLAIR_WEB_SESSION_DAYS * 86400);
    clair_web_buf_str(&c, attrs);
    free(v.p);
    const char *r = clair_web_text_of(c.p, c.len);
    free(c.p);
    return r;
}

/* the values of the session sent by the browser, if its signature is good and it is not too old */
static void clair_web_read_session(ClairDict d) {
    for (size_t i = 0; i < clair_web_req.nh; i++) {
        if (strcmp(clair_web_req.hname[i], "cookie") != 0) continue;
        const char *c = clair_web_req.hvalue[i];
        for (const char *p = c; (p = strstr(p, "clair_session=")) != NULL; p += 14) {
            if (p != c && p[-1] != ' ' && p[-1] != ';') continue;
            const char *v = p + 14;
            size_t n = strcspn(v, "; ");
            const char *dot2 = NULL, *dot1 = NULL;
            for (size_t k = n; k > 0; k--) {
                if (v[k - 1] != '.') continue;
                if (!dot2) dot2 = v + k - 1;
                else { dot1 = v + k - 1; break; }
            }
            if (!dot1 || !dot2) return;
            unsigned char mac[32];
            clair_web_session_mac(v, (size_t)(dot2 - v), mac);
            ClairWebBuf sig = { 0 };
            int good = clair_web_b64_decode(dot2 + 1, (size_t)(v + n - dot2 - 1), &sig) && sig.len == 32 &&
                       clair_web_same((const unsigned char *)sig.p, mac, 32);
            free(sig.p);
            if (!good) return;
            long long exp = strtoll(dot1 + 1, NULL, 10);
            if (exp < (long long)time(NULL)) return;
            ClairWebBuf payload = { 0 };
            if (!clair_web_b64_decode(v, (size_t)(dot1 - v), &payload)) { free(payload.p); return; }
            clair_web_fill_pairs(d, payload.p ? payload.p : "", payload.len, '&');
            free(payload.p);
            return;
        }
    }
}

/* ---- what a browser cannot be made to do by another site ---- */

/* NULL when the request may go on, else why it is refused (403) */
static const char *clair_web_refuse(void) {
    /* a server of this phone only: the address asked must be this phone (no DNS rebinding) */
    const char *host = clair_web_header("host");
    if (!clair_web_public && host) {
        char a[64], b[64], c[64];
        snprintf(a, sizeof(a), "127.0.0.1:%lld", (long long)clair_web_port);
        snprintf(b, sizeof(b), "localhost:%lld", (long long)clair_web_port);
        snprintf(c, sizeof(c), "[::1]:%lld", (long long)clair_web_port);
        int ok = strcasecmp(host, a) == 0 || strcasecmp(host, b) == 0 || strcasecmp(host, c) == 0 ||
                 (clair_web_port == 80 && (strcasecmp(host, "127.0.0.1") == 0 || strcasecmp(host, "localhost") == 0));
        if (!ok) return "this server answers only to 127.0.0.1 and localhost (Host header)";
    }
    /* a form or a script of another site cannot send POST, PUT, DELETE, PATCH here (CSRF) */
    const char *m = clair_web_req.method;
    if (strcmp(m, "GET") == 0 || strcmp(m, "HEAD") == 0 || strcmp(m, "OPTIONS") == 0) return NULL;
    const char *site = clair_web_header("sec-fetch-site");
    if (site) {
        if (strcmp(site, "same-origin") == 0 || strcmp(site, "none") == 0) return NULL;
        return "a page of another site cannot send this request (CSRF protection)";
    }
    const char *origin = clair_web_header("origin");
    if (!origin || !host) return NULL;                   /* not a browser */
    const char *o = strstr(origin, "://");
    o = o ? o + 3 : origin;
    if (strcasecmp(o, host) != 0) return "a page of another site cannot send this request (CSRF protection)";
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Templates of templates/                                            */
/* ------------------------------------------------------------------ */

enum { CLAIR_TPL_TEXT, CLAIR_TPL_VAR, CLAIR_TPL_IF, CLAIR_TPL_FOR, CLAIR_TPL_INCLUDE, CLAIR_TPL_BLOCK, CLAIR_TPL_EXTENDS };

typedef struct ClairTplNode {
    int kind;
    const char *s;                     /* text, or the name (variable, block, loop variable) */
    size_t n;
    const char *s2;                    /* for: the name of the list; include, extends: the file */
    size_t n2;
    int flag;                          /* var: |safe; if: not */
    struct ClairTplNode *kids, *alt, *next;
} ClairTplNode;

typedef struct {
    const char *file;                  /* for the messages */
    const char *src;
    size_t pos, len, line;             /* line: the line of pos */
    const char *rest;                  /* after the tag that ended a list: the condition of an elif */
    size_t rest_n, rest_line;
} ClairTpl;

_Noreturn static void clair_tpl_error(const ClairTpl *t, size_t line, const char *what) {
    fprintf(stderr, "Clair runtime error: template %s, line %zu: %s\n", t->file, line, what);
    exit(EXIT_FAILURE);
}

static ClairTplNode *clair_tpl_node(int kind) {
    ClairTplNode *n = clair_rt_arena_alloc(sizeof(ClairTplNode));
    memset(n, 0, sizeof(*n));
    n->kind = kind;
    return n;
}

static int clair_tpl_is_name(const char *s, size_t n) {
    if (!n || !(isalpha((unsigned char)s[0]) || s[0] == '_')) return 0;
    for (size_t i = 1; i < n; i++) if (!(isalnum((unsigned char)s[i]) || s[i] == '_')) return 0;
    return 1;
}

/* the next word of s[*i..n): a quoted string is one word */
static size_t clair_tpl_word(const char *s, size_t n, size_t *i, const char **w) {
    while (*i < n && isspace((unsigned char)s[*i])) (*i)++;
    *w = s + *i;
    size_t start = *i;
    if (*i < n && (s[*i] == '"' || s[*i] == '\'')) {
        char q = s[*i];
        (*i)++;
        while (*i < n && s[*i] != q) (*i)++;
        if (*i < n) (*i)++;
    } else {
        while (*i < n && !isspace((unsigned char)s[*i])) (*i)++;
    }
    return *i - start;
}

/* "file.html" without its quotes */
static int clair_tpl_quoted(const char *w, size_t n, const char **s, size_t *len) {
    if (n < 2 || (w[0] != '"' && w[0] != '\'') || w[n - 1] != w[0]) return 0;
    *s = w + 1;
    *len = n - 2;
    return *len > 0;
}

static void clair_tpl_trim(const char **s, size_t *n) {
    while (*n && isspace((unsigned char)**s)) { (*s)++; (*n)--; }
    while (*n && isspace((unsigned char)(*s)[*n - 1])) (*n)--;
}

static ClairTplNode *clair_tpl_parse(ClairTpl *t, const char *const *ends, int *which, size_t open_line);

static ClairTplNode *clair_tpl_parse_if(ClairTpl *t, const char *cond, size_t cn, size_t line) {
    ClairTplNode *n = clair_tpl_node(CLAIR_TPL_IF);
    size_t i = 0;
    const char *w, *w2;
    size_t wl = clair_tpl_word(cond, cn, &i, &w);
    if (wl == 3 && memcmp(w, "not", 3) == 0) {
        n->flag = 1;
        wl = clair_tpl_word(cond, cn, &i, &w);
    }
    if (!clair_tpl_is_name(w, wl) || clair_tpl_word(cond, cn, &i, &w2))
        clair_tpl_error(t, line, "write {% if name %} or {% if not name %}");
    n->s = w;
    n->n = wl;
    static const char *const ends[] = { "elif", "else", "endif", NULL };
    int which = -1;
    n->kids = clair_tpl_parse(t, ends, &which, line);
    if (which == 0) {
        n->alt = clair_tpl_parse_if(t, t->rest, t->rest_n, t->rest_line);
    } else if (which == 1) {
        static const char *const end_if[] = { "endif", NULL };
        n->alt = clair_tpl_parse(t, end_if, &which, line);
    }
    return n;
}

/* the nodes up to one of ends (which: its index), or to the end of the file when ends is NULL */
static ClairTplNode *clair_tpl_parse(ClairTpl *t, const char *const *ends, int *which, size_t open_line) {
    ClairTplNode *first = NULL, **tail = &first;
    for (;;) {
        size_t start = t->pos, open = t->len;
        for (size_t i = t->pos; i + 1 < t->len; i++) {
            if (t->src[i] == '{' && (t->src[i + 1] == '{' || t->src[i + 1] == '%' || t->src[i + 1] == '#')) { open = i; break; }
        }
        if (open > start) {
            ClairTplNode *n = clair_tpl_node(CLAIR_TPL_TEXT);
            n->s = t->src + start;
            n->n = open - start;
            *tail = n;
            tail = &n->next;
        }
        for (size_t i = start; i < open; i++) t->line += t->src[i] == '\n';
        t->pos = open;
        if (open == t->len) {
            if (ends) {
                const char *need = ends[0];
                for (int k = 0; ends[k]; k++) if (strncmp(ends[k], "end", 3) == 0) need = ends[k];
                char msg[96];
                snprintf(msg, sizeof(msg), "{%% %s %%} is missing", need);
                clair_tpl_error(t, open_line, msg);
            }
            return first;
        }
        char kind = t->src[open + 1];
        char c1 = kind == '{' ? '}' : kind, c2 = '}';
        size_t tag_line = t->line, close = t->len;
        for (size_t i = open + 2; i + 1 < t->len; i++) {
            if (t->src[i] == c1 && t->src[i + 1] == c2) { close = i; break; }
        }
        if (close == t->len) {
            char msg[64];
            snprintf(msg, sizeof(msg), "{%c is not closed by %c%c", kind, c1, c2);
            clair_tpl_error(t, tag_line, msg);
        }
        for (size_t i = open; i < close + 2; i++) t->line += t->src[i] == '\n';
        t->pos = close + 2;
        if (kind == '#') continue;                                    /* {# a comment #} */
        const char *in = t->src + open + 2;
        size_t in_n = close - open - 2;
        if (in_n && in[0] == '-') { in++; in_n--; }                   /* {%- and -%}: accepted, no effect */
        if (in_n && in[in_n - 1] == '-') in_n--;
        clair_tpl_trim(&in, &in_n);
        if (kind == '{') {                                            /* {{ name }}, {{ name|safe }} */
            const char *name = in, *filter = NULL;
            size_t name_n = in_n, filter_n = 0;
            const char *bar = memchr(in, '|', in_n);
            if (bar) {
                name_n = (size_t)(bar - in);
                filter = bar + 1;
                filter_n = in_n - name_n - 1;
                clair_tpl_trim(&name, &name_n);
                clair_tpl_trim(&filter, &filter_n);
            }
            ClairTplNode *n = clair_tpl_node(CLAIR_TPL_VAR);
            if (!clair_tpl_is_name(name, name_n)) clair_tpl_error(t, tag_line, "write {{ name }} or {{ name|safe }}");
            if (filter && !(filter_n == 4 && memcmp(filter, "safe", 4) == 0))
                clair_tpl_error(t, tag_line, "the only filter is |safe: {{ name|safe }}");
            n->s = name;
            n->n = name_n;
            n->flag = filter != NULL;
            *tail = n;
            tail = &n->next;
            continue;
        }
        size_t i = 0;
        const char *tag;
        size_t tl = clair_tpl_word(in, in_n, &i, &tag);
        for (int k = 0; ends && ends[k]; k++) {
            if (strlen(ends[k]) == tl && memcmp(ends[k], tag, tl) == 0) {
                *which = k;
                t->rest = in + i;
                t->rest_n = in_n - i;
                t->rest_line = tag_line;
                return first;
            }
        }
        ClairTplNode *n = NULL;
        if (tl == 2 && memcmp(tag, "if", 2) == 0) {
            n = clair_tpl_parse_if(t, in + i, in_n - i, tag_line);
        } else if (tl == 3 && memcmp(tag, "for", 3) == 0) {
            const char *v, *kw, *list, *extra;
            size_t vl = clair_tpl_word(in, in_n, &i, &v), kl = clair_tpl_word(in, in_n, &i, &kw);
            size_t ll = clair_tpl_word(in, in_n, &i, &list), el = clair_tpl_word(in, in_n, &i, &extra);
            if (!clair_tpl_is_name(v, vl) || kl != 2 || memcmp(kw, "in", 2) != 0 || !clair_tpl_is_name(list, ll) || el)
                clair_tpl_error(t, tag_line, "write {% for x in name %}");
            n = clair_tpl_node(CLAIR_TPL_FOR);
            n->s = v; n->n = vl; n->s2 = list; n->n2 = ll;
            static const char *const end_for[] = { "endfor", NULL };
            int w = -1;
            n->kids = clair_tpl_parse(t, end_for, &w, tag_line);
        } else if ((tl == 7 && memcmp(tag, "include", 7) == 0) || (tl == 7 && memcmp(tag, "extends", 7) == 0)) {
            const char *f, *extra;
            size_t fl = clair_tpl_word(in, in_n, &i, &f), el = clair_tpl_word(in, in_n, &i, &extra);
            n = clair_tpl_node(tag[0] == 'i' ? CLAIR_TPL_INCLUDE : CLAIR_TPL_EXTENDS);
            if (!clair_tpl_quoted(f, fl, &n->s2, &n->n2) || el)
                clair_tpl_error(t, tag_line, tag[0] == 'i' ? "write {% include \"file.html\" %}" : "write {% extends \"file.html\" %}");
        } else if (tl == 5 && memcmp(tag, "block", 5) == 0) {
            const char *b, *extra;
            size_t bl = clair_tpl_word(in, in_n, &i, &b), el = clair_tpl_word(in, in_n, &i, &extra);
            if (!clair_tpl_is_name(b, bl) || el) clair_tpl_error(t, tag_line, "write {% block name %}");
            n = clair_tpl_node(CLAIR_TPL_BLOCK);
            n->s = b; n->n = bl;
            static const char *const end_block[] = { "endblock", NULL };
            int w = -1;
            n->kids = clair_tpl_parse(t, end_block, &w, tag_line);
        } else {
            char msg[160];
            snprintf(msg, sizeof(msg), "{%% %.*s %%} is not a tag here (if, elif, else, endif, for, endfor, include, "
                     "extends, block, endblock)", (int)(tl > 40 ? 40 : tl), tag);
            clair_tpl_error(t, tag_line, msg);
        }
        *tail = n;
        tail = &n->next;
    }
}

typedef struct {
    ClairDict values;
    const char *bname[32], *bval[32];
    size_t bname_n[32], bval_n[32];
    int nb;
    ClairTplNode *blocks[64];
    int nblocks;
    int depth;
    ClairWebBuf *out;
} ClairTplEnv;

static int clair_tpl_lookup(ClairTplEnv *e, const char *s, size_t n, const char **v, size_t *vn) {
    for (int i = e->nb - 1; i >= 0; i--) {
        if (e->bname_n[i] == n && memcmp(e->bname[i], s, n) == 0) { *v = e->bval[i]; *vn = e->bval_n[i]; return 1; }
    }
    for (size_t i = 0; e->values && i < e->values->len; i++) {
        const char *k = e->values->keys[i];
        if (strlen(k) == n && memcmp(k, s, n) == 0) {
            *v = ((const char **)e->values->values)[i];
            *vn = strlen(*v);
            return 1;
        }
    }
    return 0;
}

static void clair_tpl_file(const char *name, size_t name_n, ClairTplEnv *e);

static void clair_tpl_render(ClairTplNode *n, ClairTplEnv *e) {
    for (; n; n = n->next) {
        const char *v;
        size_t vn;
        switch (n->kind) {
            case CLAIR_TPL_TEXT: clair_web_buf_add(e->out, n->s, n->n); break;
            case CLAIR_TPL_VAR:
                if (clair_tpl_lookup(e, n->s, n->n, &v, &vn)) {
                    if (n->flag) clair_web_buf_add(e->out, v, vn);
                    else clair_web_escape_into(e->out, v, vn);
                }
                break;
            case CLAIR_TPL_IF: {
                int truth = clair_tpl_lookup(e, n->s, n->n, &v, &vn) && vn > 0;
                if (n->flag) truth = !truth;
                clair_tpl_render(truth ? n->kids : n->alt, e);
                break;
            }
            case CLAIR_TPL_FOR: {
                if (!clair_tpl_lookup(e, n->s2, n->n2, &v, &vn) || vn == 0) break;
                if (e->nb >= 32) {
                    fprintf(stderr, "Clair runtime error: templates: more than 32 {%% for %%} inside each other\n");
                    exit(EXIT_FAILURE);
                }
                int slot = e->nb++;
                e->bname[slot] = n->s;
                e->bname_n[slot] = n->n;
                size_t i = 0;
                while (i < vn) {                       /* one line, one turn ("a\nb\n": a, then b) */
                    size_t j = i;
                    while (j < vn && v[j] != '\n') j++;
                    size_t k = j;
                    if (k > i && v[k - 1] == '\r') k--;
                    e->bval[slot] = v + i;
                    e->bval_n[slot] = k - i;
                    clair_tpl_render(n->kids, e);
                    i = j + 1;
                }
                e->nb--;
                break;
            }
            case CLAIR_TPL_INCLUDE: clair_tpl_file(n->s2, n->n2, e); break;
            case CLAIR_TPL_BLOCK: {
                ClairTplNode *use = n->kids;
                for (int i = 0; i < e->nblocks; i++) {
                    if (e->blocks[i]->n == n->n && memcmp(e->blocks[i]->s, n->s, n->n) == 0) { use = e->blocks[i]->kids; break; }
                }
                clair_tpl_render(use, e);
                break;
            }
            default: break;                             /* extends: handled by clair_tpl_file */
        }
    }
}

static void clair_tpl_file(const char *name, size_t name_n, ClairTplEnv *e) {
    char *fname = clair_web_dup(name, name_n);
    const char *file = clair_web_text_of(fname, name_n);
    free(fname);
    int ok = name_n > 0 && name_n < 200 && file[0] != '/';
    for (const char *p = file; ok && *p; p++) {
        if (p[0] == '.' && p[1] == '.' && (p == file || p[-1] == '/')) ok = 0;
    }
    if (!ok) clair_web_fail("a template is a file of templates/, got ", file);
    if (++e->depth > 16) clair_web_fail("templates include or extend each other more than 16 times: ", file);
    char path[256];
    snprintf(path, sizeof(path), "templates/%s", file);
    ClairWebBuf b = { 0 };
    if (!clair_web_read_file(path, &b)) clair_web_fail("no template ", path);
    ClairTpl t;
    memset(&t, 0, sizeof(t));
    t.file = file;
    t.src = clair_web_text_of(b.p, b.len);
    t.len = b.len;
    t.line = 1;
    free(b.p);
    int w = -1;
    ClairTplNode *nodes = clair_tpl_parse(&t, NULL, &w, 1);
    ClairTplNode *first = nodes;
    while (first && first->kind == CLAIR_TPL_TEXT) {
        size_t k = 0;
        while (k < first->n && isspace((unsigned char)first->s[k])) k++;
        if (k < first->n) break;
        first = first->next;
    }
    if (first && first->kind == CLAIR_TPL_EXTENDS) {
        for (ClairTplNode *n = first->next; n; n = n->next) {        /* the blocks of the page win */
            if (n->kind != CLAIR_TPL_BLOCK) continue;
            if (e->nblocks >= 64) clair_web_fail("templates: more than 64 blocks in ", file);
            e->blocks[e->nblocks++] = n;
        }
        clair_tpl_file(first->s2, first->n2, e);
    } else {
        clair_tpl_render(nodes, e);
    }
    e->depth--;
}

/* web.page(name, values): templates/name with the values */
static const char *clair_web_render(const char *name, ClairDict values) {
    ClairWebBuf out = { 0 };
    clair_web_buf_add(&out, "", 0);
    ClairTplEnv e;
    memset(&e, 0, sizeof(e));
    e.values = values;
    e.out = &out;
    clair_tpl_file(name, strlen(name), &e);
    const char *r = clair_web_text_of(out.p, out.len);
    free(out.p);
    return r;
}

/* ------------------------------------------------------------------ */
/* PHP pages                                                          */
/* ------------------------------------------------------------------ */

/* with php (no php-cgi): fills $_GET, $_POST, $_COOKIE like php-cgi does, and writes the status first */
static const char clair_web_php_shim_text[] =
    "<?php\n"
    "(function () {\n"
    "    $q = getenv('QUERY_STRING'); if ($q !== false) { parse_str($q, $_GET); }\n"
    "    if (getenv('REQUEST_METHOD') === 'POST') {\n"
    "        $body = file_get_contents('php://stdin');\n"
    "        if (stripos((string)getenv('CONTENT_TYPE'), 'application/x-www-form-urlencoded') === 0) { parse_str($body, $_POST); }\n"
    "    }\n"
    "    $c = getenv('HTTP_COOKIE');\n"
    "    if ($c !== false) { foreach (explode(';', $c) as $p) { $kv = explode('=', trim($p), 2);"
    " if (count($kv) == 2) { $_COOKIE[$kv[0]] = urldecode($kv[1]); } } }\n"
    "    $_REQUEST = array_merge($_GET, $_POST);\n"
    "    foreach (getenv() as $k => $v) { $_SERVER[$k] = $v; }\n"
    "    $_SERVER['PHP_SELF'] = $_SERVER['SCRIPT_NAME'] ?? '';\n"
    "    ob_start();\n"
    "    register_shutdown_function(function () {\n"
    "        while (ob_get_level() > 1) { ob_end_flush(); }\n"
    "        $body = ob_get_clean();\n"
    "        $code = http_response_code();\n"
    "        echo 'Status: ' . ($code ? $code : 200) . \"\\r\\nContent-Type: text/html; charset=utf-8\\r\\n\\r\\n\";\n"
    "        echo $body === false ? '' : $body;\n"
    "    });\n"
    "})();\n";

static char clair_web_php_bin[512];
static int clair_web_php_kind = -1;            /* -1 not looked for, 0 none, 1 php-cgi, 2 php */
static char clair_web_php_shim[512];

static int clair_web_find_in_path(const char *name, char *out, size_t out_n) {
    const char *path = getenv("PATH");
    if (!path) path = "/usr/bin:/bin";
    while (*path) {
        size_t n = strcspn(path, ":");
        if (n && n + strlen(name) + 2 < out_n) {
            snprintf(out, out_n, "%.*s/%s", (int)n, path, name);
            if (access(out, X_OK) == 0) return 1;
        }
        path += n;
        if (*path == ':') path++;
    }
    return 0;
}

static int clair_web_find_php(void) {
    if (clair_web_php_kind >= 0) return clair_web_php_kind;
    clair_web_php_kind = 0;
    if (clair_web_find_in_path("php-cgi", clair_web_php_bin, sizeof(clair_web_php_bin))) {
        clair_web_php_kind = 1;
    } else if (clair_web_find_in_path("php", clair_web_php_bin, sizeof(clair_web_php_bin))) {
        snprintf(clair_web_php_shim, sizeof(clair_web_php_shim), "%s/clair_web_php_%ld.php", clair_web_tmpdir(), (long)getpid());
        int fd = open(clair_web_php_shim, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd >= 0) {
            size_t n = sizeof(clair_web_php_shim_text) - 1;
            ssize_t w = write(fd, clair_web_php_shim_text, n);
            close(fd);
            if (w == (ssize_t)n) clair_web_php_kind = 2;
        }
    }
    return clair_web_php_kind;
}

static void clair_web_cgi_env(const char *name, const char *value) { setenv(name, value ? value : "", 1); }

/* the output of PHP: "Status: 302\r\nLocation: /x\r\n\r\nbody" */
static void clair_web_cgi_answer(char *out, size_t n) {
    size_t head_end = 0, body_at = 0;
    for (size_t i = 0; i < n; i++) {
        if (out[i] != '\n') continue;
        if (i + 1 < n && out[i + 1] == '\n') { head_end = i; body_at = i + 2; break; }
        if (i + 2 < n && out[i + 1] == '\r' && out[i + 2] == '\n') { head_end = i; body_at = i + 3; break; }
    }
    int status = 0, has_location = 0;
    const char *type = "text/html; charset=utf-8";
    ClairWebBuf extra = { 0 };
    clair_web_buf_add(&extra, "", 0);
    if (body_at) {
        out[head_end] = '\0';
        for (char *h = out; h;) {
            char *nl = strchr(h, '\n');
            if (nl) *nl = '\0';
            size_t hl = strlen(h);
            if (hl && h[hl - 1] == '\r') h[--hl] = '\0';
            char *colon = strchr(h, ':');
            if (colon) {
                char *v = colon + 1;
                while (*v == ' ' || *v == '\t') v++;
                size_t nlen = (size_t)(colon - h);
                if (nlen == 6 && strncasecmp(h, "status", 6) == 0) status = atoi(v);
                else if (nlen == 12 && strncasecmp(h, "content-type", 12) == 0) type = clair_web_text_of(v, strlen(v));
                else {
                    if (nlen == 8 && strncasecmp(h, "location", 8) == 0) has_location = 1;
                    clair_web_buf_str(&extra, h);
                    clair_web_buf_ch(&extra, '\n');
                }
            }
            h = nl ? nl + 1 : NULL;
        }
    } else {
        body_at = 0;
    }
    if (!status) status = has_location ? 302 : 200;
    if (status < 100 || status > 599) status = 500;
    free(clair_web_resp.owned);
    free(clair_web_resp.file);
    memset(&clair_web_resp, 0, sizeof(clair_web_resp));
    clair_web_resp.set = 1;
    clair_web_resp.status = status;
    clair_web_resp.type = type;
    clair_web_resp.headers = clair_web_text_of(extra.p, extra.len);
    free(extra.p);
    clair_web_resp.owned = out;
    clair_web_resp.body = out + body_at;
    clair_web_resp.body_len = n - body_at;
}

/* runs the PHP page script (a file of static/), url_path: how it was asked */
static void clair_web_php(const char *script, const char *url_path) {
    int kind = clair_web_find_php();
    if (kind == 0) {
        clair_web_page(500, clair_web_public ? NULL : "PHP is not installed (in Termux: pkg install php)");
        return;
    }
    char *abs = realpath(script, NULL), *root = realpath("static", NULL);
    if (!abs) { clair_web_page(404, NULL); free(root); return; }
    int in[2], out[2];
    if (pipe(in) != 0) { free(abs); free(root); clair_web_page(500, "pipe"); return; }
    if (pipe(out) != 0) { close(in[0]); close(in[1]); free(abs); free(root); clair_web_page(500, "pipe"); return; }
    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) {
        close(in[0]); close(in[1]); close(out[0]); close(out[1]);
        free(abs); free(root);
        clair_web_page(500, "fork");
        return;
    }
    if (pid == 0) {
        dup2(in[0], 0);
        dup2(out[1], 1);
        close(in[0]); close(in[1]); close(out[0]); close(out[1]);
        char port[32], len[32];
        snprintf(port, sizeof(port), "%lld", (long long)clair_web_port);
        snprintf(len, sizeof(len), "%zu", clair_web_req.body_len);
        clair_web_cgi_env("GATEWAY_INTERFACE", "CGI/1.1");
        clair_web_cgi_env("SERVER_PROTOCOL", "HTTP/1.1");
        clair_web_cgi_env("SERVER_SOFTWARE", "Clair");
        clair_web_cgi_env("SERVER_NAME", clair_web_host);
        clair_web_cgi_env("SERVER_PORT", port);
        clair_web_cgi_env("REQUEST_METHOD", clair_web_req.method);
        clair_web_cgi_env("QUERY_STRING", clair_web_req.query);
        clair_web_cgi_env("REQUEST_URI", clair_web_req.target);
        clair_web_cgi_env("SCRIPT_NAME", url_path);
        clair_web_cgi_env("SCRIPT_FILENAME", abs);
        clair_web_cgi_env("DOCUMENT_ROOT", root ? root : ".");
        clair_web_cgi_env("REMOTE_ADDR", clair_web_req.ip);
        clair_web_cgi_env("REDIRECT_STATUS", "200");
        clair_web_cgi_env("CONTENT_LENGTH", len);
        const char *ct = clair_web_header("content-type");
        if (ct) clair_web_cgi_env("CONTENT_TYPE", ct);
        else unsetenv("CONTENT_TYPE");
        for (size_t i = 0; i < clair_web_req.nh; i++) {
            if (strcmp(clair_web_req.hname[i], "content-type") == 0 || strcmp(clair_web_req.hname[i], "content-length") == 0) continue;
            char name[160];
            snprintf(name, sizeof(name), "HTTP_%s", clair_web_req.hname[i]);
            for (char *q = name + 5; *q; q++) *q = *q == '-' ? '_' : (char)toupper((unsigned char)*q);
            clair_web_cgi_env(name, clair_web_req.hvalue[i]);
        }
        char *dir = clair_web_dup(abs, strlen(abs));
        char *slash = strrchr(dir, '/');
        if (slash) { *slash = '\0'; if (chdir(dir[0] ? dir : "/") != 0) _exit(126); }
        if (kind == 1) {
            char *const argv[] = { clair_web_php_bin, NULL };
            execv(clair_web_php_bin, argv);
        } else {
            char opt[600];
            snprintf(opt, sizeof(opt), "auto_prepend_file=%s", clair_web_php_shim);
            char *const argv[] = { clair_web_php_bin, "-d", opt, abs, NULL };
            execv(clair_web_php_bin, argv);
        }
        _exit(127);
    }
    close(in[0]);
    close(out[1]);
    free(abs);
    free(root);
    fcntl(in[1], F_SETFL, O_NONBLOCK);
    ClairWebBuf got = { 0 };
    clair_web_buf_add(&got, "", 0);
    size_t sent = 0;
    int in_open = 1, out_open = 1, too_long = 0;
    if (clair_web_req.body_len == 0) { close(in[1]); in_open = 0; }
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    while (out_open) {
        struct pollfd fds[2];
        int nf = 0;
        fds[nf].fd = out[0]; fds[nf].events = POLLIN; fds[nf].revents = 0; nf++;
        if (in_open) { fds[nf].fd = in[1]; fds[nf].events = POLLOUT; fds[nf].revents = 0; nf++; }
        int r = poll(fds, (nfds_t)nf, 1000);
        if (r < 0 && errno != EINTR) break;
        if (clair_web_ms_since(&t0) > 60000) { too_long = 1; break; }
        if (r <= 0) continue;
        if (in_open && nf == 2 && (fds[1].revents & (POLLOUT | POLLERR | POLLHUP))) {
            ssize_t w = write(in[1], clair_web_req.body + sent, clair_web_req.body_len - sent);
            if (w > 0) sent += (size_t)w;
            if (w < 0 && errno != EAGAIN && errno != EINTR) sent = clair_web_req.body_len;
            if (sent >= clair_web_req.body_len) { close(in[1]); in_open = 0; }
        }
        if (fds[0].revents & (POLLIN | POLLERR | POLLHUP)) {
            char tmp[16384];
            ssize_t n = read(out[0], tmp, sizeof(tmp));
            if (n > 0) {
                if (got.len < ((size_t)64 << 20)) clair_web_buf_add(&got, tmp, (size_t)n);
            } else if (n == 0 || (errno != EINTR && errno != EAGAIN)) {
                out_open = 0;
            }
        }
    }
    if (in_open) close(in[1]);
    close(out[0]);
    if (too_long) kill(pid, SIGKILL);
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    if (too_long) { free(got.p); clair_web_page(504, clair_web_public ? NULL : "the PHP page took more than 60 s"); return; }
    if (got.len == 0 && (!WIFEXITED(status) || WEXITSTATUS(status) != 0)) {
        free(got.p);
        clair_web_page(500, clair_web_public ? NULL : "PHP stopped without an answer (see the terminal)");
        return;
    }
    clair_web_cgi_answer(got.p, got.len);
}

/* ------------------------------------------------------------------ */
/* static/: files and PHP pages                                       */
/* ------------------------------------------------------------------ */

static int clair_web_is_dir(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
static int clair_web_is_file(const char *p) { struct stat st; return stat(p, &st) == 0 && S_ISREG(st.st_mode); }

/* the answer to a path no route takes: a file of static/ (as /static/x or /x), a PHP page, or 404 */
static int64_t clair_web_static(const char *path) {
    const char *rel = path;
    if (strncmp(rel, "/static/", 8) == 0) rel += 7;
    else if (strcmp(rel, "/static") == 0) rel = "/";
    /* no .. and no hidden file */
    for (const char *p = rel; *p; p++) {
        if (*p == '/' && p[1] == '.') { clair_web_page(404, NULL); return 0; }
        if (*p == '\\') { clair_web_page(404, NULL); return 0; }
    }
    ClairWebBuf full = { 0 };
    clair_web_buf_str(&full, "static");
    clair_web_buf_str(&full, rel);
    ClairWebBuf url = { 0 };
    clair_web_buf_str(&url, path);
    if (clair_web_is_dir(full.p)) {
        if (full.p[full.len - 1] != '/') clair_web_buf_ch(&full, '/');
        if (url.p[url.len - 1] != '/') clair_web_buf_ch(&url, '/');
        size_t at = full.len, uat = url.len;
        clair_web_buf_str(&full, "index.html");
        if (!clair_web_is_file(full.p)) {
            full.len = at;
            full.p[at] = '\0';
            clair_web_buf_str(&full, "index.php");
            url.len = uat;
            url.p[uat] = '\0';
            clair_web_buf_str(&url, "index.php");
        }
    }
    int get = strcmp(clair_web_req.method, "GET") == 0 || strcmp(clair_web_req.method, "HEAD") == 0;
    if (!clair_web_is_file(full.p)) {
        clair_web_page(404, NULL);
    } else if (clair_web_ends_with(full.p, ".php")) {
        clair_web_php(full.p, url.p);
    } else if (!get) {
        clair_web_page(405, NULL);
    } else {
        free(clair_web_resp.owned);
        free(clair_web_resp.file);
        memset(&clair_web_resp, 0, sizeof(clair_web_resp));
        clair_web_resp.set = 1;
        clair_web_resp.status = 200;
        clair_web_resp.file = clair_web_dup(full.p, full.len);
        clair_web_resp.type = clair_web_mime(full.p);
        clair_web_resp.body = "";
        clair_web_resp.headers = "";
    }
    free(full.p);
    free(url.p);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Sending the answer                                                 */
/* ------------------------------------------------------------------ */

static int clair_web_send_all(int fd, const char *p, size_t n) {
    while (n) {
        ssize_t w = send(fd, p, n, MSG_NOSIGNAL);
        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) return 0;
        p += w;
        n -= (size_t)w;
    }
    return 1;
}

static void clair_web_send(int fd, int head_only) {
    int file_fd = -1;
    size_t length = clair_web_resp.body_len;
    if (clair_web_resp.file) {
        struct stat st;
        file_fd = open(clair_web_resp.file, O_RDONLY);
        if (file_fd < 0 || fstat(file_fd, &st) != 0 || !S_ISREG(st.st_mode)) {
            if (file_fd >= 0) close(file_fd);
            file_fd = -1;
            clair_web_page(404, NULL);
            length = clair_web_resp.body_len;
        } else {
            length = (size_t)st.st_size;
        }
    }
    int status = clair_web_resp.status;
    int no_body = status == 204 || status == 304 || (status >= 100 && status < 200);
    ClairWebBuf h = { 0 };
    char line[256];
    snprintf(line, sizeof(line), "HTTP/1.1 %d %s\r\nServer: Clair\r\n", status, clair_web_reason(status));
    clair_web_buf_str(&h, line);
    if (!no_body) {
        if (clair_web_resp.type && clair_web_resp.type[0]) {
            clair_web_buf_str(&h, "Content-Type: ");
            clair_web_buf_str(&h, clair_web_resp.type);
            clair_web_buf_str(&h, "\r\n");
        }
        snprintf(line, sizeof(line), "Content-Length: %zu\r\n", length);
        clair_web_buf_str(&h, line);
    }
    clair_web_buf_str(&h, "Connection: close\r\n");
    /* V0.57: no guessing of the type, no page of this site inside a frame of another (unless the route says so) */
    const char *extra = clair_web_resp.headers ? clair_web_resp.headers : "";
    if (!strcasestr(extra, "X-Content-Type-Options:")) clair_web_buf_str(&h, "X-Content-Type-Options: nosniff\r\n");
    if (!strcasestr(extra, "X-Frame-Options:")) clair_web_buf_str(&h, "X-Frame-Options: SAMEORIGIN\r\n");
    for (const char *p = extra; *p;) {
        size_t n = strcspn(p, "\n");
        size_t k = n;
        while (k && (p[k - 1] == '\r' || p[k - 1] == ' ')) k--;
        if (k) {
            clair_web_buf_add(&h, p, k);
            clair_web_buf_str(&h, "\r\n");
        }
        p += n;
        if (*p == '\n') p++;
    }
    clair_web_buf_str(&h, "\r\n");
    int ok = clair_web_send_all(fd, h.p, h.len);
    free(h.p);
    if (ok && !head_only && !no_body) {
        if (file_fd >= 0) {
            char tmp[65536];
            for (;;) {
                ssize_t r = read(file_fd, tmp, sizeof(tmp));
                if (r < 0 && errno == EINTR) continue;
                if (r <= 0 || !clair_web_send_all(fd, tmp, (size_t)r)) break;
            }
        } else {
            clair_web_send_all(fd, clair_web_resp.body, clair_web_resp.body_len);
        }
    }
    if (file_fd >= 0) close(file_fd);
}

/* ------------------------------------------------------------------ */
/* The server                                                         */
/* ------------------------------------------------------------------ */

static char *clair_web_err_text = NULL;
static size_t clair_web_err_len = 0;

static void clair_web_log(int status, double ms) {
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    const char *target = clair_web_req.target ? clair_web_req.target : "-";
    fprintf(clair_web_real_stderr(), "[%02d:%02d:%02d] %s %s %.200s %d %.1f ms\n", tm.tm_hour, tm.tm_min, tm.tm_sec,
            clair_web_req.ip[0] ? clair_web_req.ip : "-", clair_web_req.method ? clair_web_req.method : "-", target, status, ms);
    fflush(clair_web_real_stderr());
}

/* the lists and dictionaries made by a request are given back after it */
static size_t clair_web_dicts_before = 0, clair_web_lists_before = 0;

static void clair_web_remember_containers(void) {
    clair_web_dicts_before = clair_rt_dict_count;
#ifdef CLAIR_WEB_HAS_LISTS
    while (clair_rt_list_count > 0 && !clair_rt_lists[clair_rt_list_count - 1]) clair_rt_list_count--;
    clair_web_lists_before = clair_rt_list_count;
#endif
}

static void clair_web_free_containers(void) {
    while (clair_rt_dict_count > clair_web_dicts_before) {
        ClairDict d = clair_rt_dicts[--clair_rt_dict_count];
        if (!d) continue;
        for (size_t k = 0; k < d->len; k++) free((char *)d->keys[k]);
        free(d->keys);
        free(d->values);
        free(d);
    }
#ifdef CLAIR_WEB_HAS_LISTS
    while (clair_rt_list_count > clair_web_lists_before) {
        ClairList l = clair_rt_lists[--clair_rt_list_count];
        if (!l) continue;
        free(l->data);
        free(l);
    }
#endif
}

static void clair_web_serve(int fd) {
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    memset(&clair_web_resp, 0, sizeof(clair_web_resp));
    int st = clair_web_read(fd);
    if (st < 0) { clair_web_request_free(); return; }
    int head_only = clair_web_req.method && strcmp(clair_web_req.method, "HEAD") == 0;
    if (st > 0) {
        clair_web_page(st, NULL);
        clair_web_send(fd, head_only);
        clair_web_log(st, clair_web_ms_since(&t0));
        clair_web_response_free();
        clair_web_request_free();
        return;
    }
    const char *refused = clair_web_refuse();             /* V0.57 */
    if (refused) {
        clair_web_page(403, refused);
        clair_web_send(fd, head_only);
        clair_web_log(403, clair_web_ms_since(&t0));
        clair_web_response_free();
        clair_web_request_free();
        return;
    }
    ClairArenaMark mark = clair_rt_arena_mark();
    clair_web_remember_containers();
    clair_web_err_text = NULL;
    clair_web_err_len = 0;
    clair_web_capture = open_memstream(&clair_web_err_text, &clair_web_err_len);
    int failed = 0;
    if (setjmp(clair_web_jump) == 0) {
        clair_web_in_handler = 1;
        clair_web_handler();
        clair_web_in_handler = 0;
    } else {
        failed = 1;
    }
    clair_web_in_handler = 0;
    if (clair_web_capture) {
        fclose(clair_web_capture);
        clair_web_capture = NULL;
    }
    if (clair_web_err_len) {
        fwrite(clair_web_err_text, 1, clair_web_err_len, clair_web_real_stderr());
        fflush(clair_web_real_stderr());
    }
    if (failed) {
        size_t n = clair_web_err_len;
        while (n && (clair_web_err_text[n - 1] == '\n')) n--;
        if (clair_web_err_text) clair_web_err_text[n] = '\0';
        clair_web_page(500, clair_web_public ? NULL : clair_web_err_text);
    } else if (!clair_web_resp.set) {
        clair_web_page(500, clair_web_public ? NULL : "the route gave no answer");
    }
    free(clair_web_err_text);
    clair_web_err_text = NULL;
    fflush(stdout);
    clair_web_send(fd, head_only);
    clair_web_log(clair_web_resp.status, clair_web_ms_since(&t0));
    clair_web_response_free();
    clair_web_free_containers();
    clair_rt_arena_reset(mark);
    clair_web_request_free();
}

static void clair_web_banner(const char *host, int port) {
    FILE *e = clair_web_real_stderr();
    fprintf(e, "\n * Clair web: http://%s:%d/\n", strcmp(host, "0.0.0.0") == 0 ? "127.0.0.1" : host, port);
    if (strcmp(host, "0.0.0.0") == 0) {
        struct ifaddrs *list = NULL;
        if (getifaddrs(&list) == 0) {
            for (struct ifaddrs *a = list; a; a = a->ifa_next) {
                if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET) continue;
                char ip[64];
                inet_ntop(AF_INET, &((struct sockaddr_in *)a->ifa_addr)->sin_addr, ip, sizeof(ip));
                if (strncmp(ip, "127.", 4) == 0) continue;
                fprintf(e, " * on the network (%s): http://%s:%d/\n", a->ifa_name, ip, port);
            }
            freeifaddrs(list);
        }
    }
    fprintf(e, " * Ctrl+C to stop\n\n");
    fflush(e);
}

/* web.run(port), web.run_public(port): serves until the program is stopped */
static int64_t clair_web_run(int64_t port, const char *host) {
    if (port < 0 || port > 65535) {
        char n[32];
        snprintf(n, sizeof(n), "%lld", (long long)port);
        clair_web_fail("a port is a number from 0 to 65535, got ", n);
    }
    signal(SIGPIPE, SIG_IGN);
    snprintf(clair_web_host, sizeof(clair_web_host), "%s", host);
    clair_web_public = strcmp(host, "127.0.0.1") != 0;
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) clair_web_fail("cannot open a socket", NULL);
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) clair_web_fail("not an address: ", host);
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        char msg[160];
        snprintf(msg, sizeof(msg), "%lld (%s)%s", (long long)port, strerror(errno),
                 errno == EADDRINUSE ? ": another server uses it, stop it or choose another port" : "");
        close(s);
        clair_web_fail("cannot serve on the port ", msg);
    }
    if (listen(s, 64) != 0) clair_web_fail("cannot listen: ", strerror(errno));
    socklen_t al = sizeof(addr);
    getsockname(s, (struct sockaddr *)&addr, &al);
    clair_web_port = ntohs(addr.sin_port);
    clair_web_banner(host, (int)clair_web_port);
    for (;;) {
        struct sockaddr_in peer;
        socklen_t pl = sizeof(peer);
        int c = accept(s, (struct sockaddr *)&peer, &pl);
        if (c < 0) {
            if (errno == EINTR || errno == ECONNABORTED || errno == EAGAIN) continue;
            if (errno == EMFILE || errno == ENFILE || errno == ENOBUFS || errno == ENOMEM) { usleep(100000); continue; }
            clair_web_fail("the server stopped: ", strerror(errno));
        }
        struct timeval tv = { 10, 0 };
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        memset(&clair_web_req, 0, sizeof(clair_web_req));
        inet_ntop(AF_INET, &peer.sin_addr, clair_web_req.ip, sizeof(clair_web_req.ip));
        clair_web_serve(c);
        close(c);
    }
    return 0;
}

static const char *clair_web_lower(const char *s) {
    size_t n = strlen(s);
    char *d = clair_rt_arena_alloc(n + 1);
    for (size_t i = 0; i < n; i++) d[i] = (char)tolower((unsigned char)s[i]);
    d[n] = '\0';
    return d;
}
