/* ==== V0.72 : les textes comptes en caracteres, et leurs methodes, comme Python ====
 * (src/clair_text_rt.c, recopie dans le C que Clair ecrit, apres l'arene et les listes)
 *
 * Un texte de Clair est en UTF-8 : un caractere y prend 1 a 4 octets. len(s), s[i], s[a:b],
 * for c in s et les positions de find() comptent en caracteres, comme Python.
 * Un caractere commence a chaque octet qui n'est pas une suite (10xxxxxx) : un texte mal forme
 * garde ses octets, et len, s[i] et for c in s restent d'accord entre eux.
 * Les textes rendus vivent dans l'arene, comme ceux de + ; un caractere seul (s[i], for c in s)
 * est garde une fois pour toutes : parcourir un texte ne prend pas de memoire.
 *
 * noinline : sans lui, Clang recopie la recherche dans les tables partout, et compile deux fois plus
 * lentement un programme qui fait s.lower().
 * CLAIR_TX_LISTS : les listes sont la (split, splitlines, join).
 * CLAIR_TX_TABLES : les tables Unicode (lower, upper, title, isalpha, isdigit...), faites par
 * tools/clair_text_tables.py depuis Python et verifiees caractere par caractere.
 */

static _Noreturn void clair_tx_fail(const char *message, size_t line) {
    if (line) fprintf(stderr, "Clair runtime error: %s (line %zu)\n", message, line);
    else fprintf(stderr, "Clair runtime error: %s\n", message);
    exit(EXIT_FAILURE);
}

static inline int clair_tx_cont(unsigned char b) { return (b & 0xC0) == 0x80; }

static int clair_tx_is_ascii(const unsigned char *p, size_t n) {
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        uint64_t w;
        memcpy(&w, p + i, 8);
        if (w & 0x8080808080808080ULL) return 0;
    }
    for (; i < n; i++) if (p[i] & 0x80) return 0;
    return 1;
}

/* the characters in the bytes [0, n) */
static int64_t clair_tx_chars(const unsigned char *p, size_t n) {
    int64_t k = 0;
    for (size_t i = 0; i < n; i++) k += !clair_tx_cont(p[i]);
    return k + (n > 0 && clair_tx_cont(p[0]));
}

/* len(s) */
static int64_t clair_tx_len(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    size_t n = strlen(s);
    return clair_tx_is_ascii(p, n) ? (int64_t)n : clair_tx_chars(p, n);
}

typedef struct { const unsigned char *p; size_t bytes; int64_t len; int ascii; } ClairTxView;

static ClairTxView clair_tx_view(const char *s) {
    ClairTxView v;
    v.p = (const unsigned char *)s;
    v.bytes = strlen(s);
    v.ascii = clair_tx_is_ascii(v.p, v.bytes);
    v.len = v.ascii ? (int64_t)v.bytes : clair_tx_chars(v.p, v.bytes);
    return v;
}

/* where the character k starts (k >= len: the end) */
static size_t clair_tx_byte(const ClairTxView *v, int64_t k) {
    if (k <= 0) return 0;
    if (k >= v->len) return v->bytes;
    if (v->ascii) return (size_t)k;
    for (size_t i = 1; i < v->bytes; i++) if (!clair_tx_cont(v->p[i]) && --k == 0) return i;
    return v->bytes;
}

/* the characters before the byte b (a character start) */
static int64_t clair_tx_char_index(const ClairTxView *v, size_t b) {
    return v->ascii ? (int64_t)b : clair_tx_chars(v->p, b);
}

/* start and end of find(sub, start, end) and the others, like Python's slices */
static void clair_tx_adjust(int64_t *start, int64_t *end, int64_t len) {
    if (*end > len) *end = len;
    else if (*end < 0) { *end += len; if (*end < 0) *end = 0; }
    if (*start < 0) { *start += len; if (*start < 0) *start = 0; }
}

/* ---- new texts, in the arena ---- */
static const char *clair_tx_copy(const void *p, size_t n) {
    char *r = clair_rt_arena_alloc(n + 1);
    memcpy(r, p, n);
    r[n] = '\0';
    return r;
}

typedef struct { char *d; size_t n, cap; } ClairTxBuf;

static __attribute__((noinline)) void clair_tx_add(ClairTxBuf *b, const void *src, size_t n) {
    if (b->n + n > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        while (cap < b->n + n) cap *= 2;
        char *d = realloc(b->d, cap);
        if (!d) clair_tx_fail("out of memory", 0);
        b->d = d;
        b->cap = cap;
    }
    if (n) memcpy(b->d + b->n, src, n);
    b->n += n;
}

static const char *clair_tx_done(ClairTxBuf *b) {
    const char *r = clair_tx_copy(b->d ? b->d : "", b->n);
    free(b->d);
    return r;
}

/* ---- one character: s[i] and for c in s, kept once for all ---- */
typedef struct { uint64_t key; char *text; } ClairTxOne;
static ClairTxOne *clair_tx_ones = NULL;
static size_t clair_tx_one_cap = 0, clair_tx_one_used = 0;
static char clair_tx_ascii1[128][2];

static void clair_tx_ones_free(void) {
    for (size_t i = 0; i < clair_tx_one_cap; i++) free(clair_tx_ones[i].text);
    free(clair_tx_ones);
    clair_tx_ones = NULL;
    clair_tx_one_cap = clair_tx_one_used = 0;
}

static const char *clair_tx_one(const unsigned char *p, size_t n) {
    if (n == 1 && p[0] < 0x80) {
        clair_tx_ascii1[p[0]][0] = (char)p[0];
        return clair_tx_ascii1[p[0]];
    }
    if (n > 4) return clair_tx_copy(p, n);
    uint64_t key = (uint64_t)n << 32;
    for (size_t i = 0; i < n; i++) key |= (uint64_t)p[i] << (8 * i);
    if (2 * (clair_tx_one_used + 1) > clair_tx_one_cap) {
        size_t cap = clair_tx_one_cap ? clair_tx_one_cap * 2 : 256;
        ClairTxOne *t = calloc(cap, sizeof(*t));
        if (!t) clair_tx_fail("out of memory", 0);
        if (!clair_tx_ones && atexit(clair_tx_ones_free) != 0) clair_tx_fail("cannot register memory cleanup", 0);
        for (size_t i = 0; i < clair_tx_one_cap; i++) {
            if (!clair_tx_ones[i].text) continue;
            size_t h = (size_t)((clair_tx_ones[i].key * 0x9E3779B97F4A7C15ULL) >> 32) & (cap - 1);
            while (t[h].text) h = (h + 1) & (cap - 1);
            t[h] = clair_tx_ones[i];
        }
        free(clair_tx_ones);
        clair_tx_ones = t;
        clair_tx_one_cap = cap;
    }
    size_t h = (size_t)((key * 0x9E3779B97F4A7C15ULL) >> 32) & (clair_tx_one_cap - 1);
    while (clair_tx_ones[h].text) {
        if (clair_tx_ones[h].key == key) return clair_tx_ones[h].text;
        h = (h + 1) & (clair_tx_one_cap - 1);
    }
    char *text = malloc(n + 1);
    if (!text) clair_tx_fail("out of memory", 0);
    memcpy(text, p, n);
    text[n] = '\0';
    clair_tx_ones[h].key = key;
    clair_tx_ones[h].text = text;
    clair_tx_one_used++;
    return text;
}

/* s[i] (i < 0 counts from the end) */
static const char *clair_tx_at(const char *s, int64_t i, size_t line) {
    ClairTxView v;
    v.p = (const unsigned char *)s;
    v.bytes = strlen(s);
    v.ascii = clair_tx_is_ascii(v.p, v.bytes);
    v.len = v.ascii || i < 0 ? (v.ascii ? (int64_t)v.bytes : clair_tx_chars(v.p, v.bytes)) : INT64_MAX;
    if (i < 0) i += v.len;
    if (i < 0 || i >= v.len) clair_tx_fail("string index out of range", line);
    size_t b = clair_tx_byte(&v, i);
    if (b >= v.bytes) clair_tx_fail("string index out of range", line);
    size_t n = 1;
    while (b + n < v.bytes && clair_tx_cont(v.p[b + n])) n++;
    return clair_tx_one(v.p + b, n);
}

/* for c in s: the character at *at, and *at moves past it */
static const char *clair_tx_next(const char **at) {
    const unsigned char *p = (const unsigned char *)*at;
    size_t n = 1;
    while (clair_tx_cont(p[n])) n++;
    *at = (const char *)(p + n);
    return clair_tx_one(p, n);
}

/* ---- characters as numbers (code points) ---- */
#define CLAIR_TX_RAW 0x80000000u     /* a byte that starts no well-formed character: kept as it is */

/* the character at byte i, and its bytes in *w */
static uint32_t clair_tx_get(const unsigned char *p, size_t bytes, size_t i, size_t *w) {
    unsigned char b = p[i];
    uint32_t c = 0;
    size_t need = 0;
    if (b < 0x80) { *w = 1; return b; }
    if (b >= 0xC2 && b <= 0xDF) { c = b & 0x1F; need = 2; }
    else if (b >= 0xE0 && b <= 0xEF) { c = b & 0x0F; need = 3; }
    else if (b >= 0xF0 && b <= 0xF4) { c = b & 0x07; need = 4; }
    int ok = need > 0 && i + need <= bytes;
    for (size_t k = 1; ok && k < need; k++) {
        if (!clair_tx_cont(p[i + k])) ok = 0;
        else c = (c << 6) | (p[i + k] & 0x3F);
    }
    if (ok && need == 3 && (c < 0x800 || (c >= 0xD800 && c <= 0xDFFF))) ok = 0;
    if (ok && need == 4 && (c < 0x10000 || c > 0x10FFFF)) ok = 0;
    if (!ok) { *w = 1; return CLAIR_TX_RAW | b; }
    *w = need;
    return c;
}

static size_t clair_tx_put(char *o, uint32_t c) {
    if (c & CLAIR_TX_RAW) { o[0] = (char)(c & 0xFF); return 1; }
    if (c < 0x80) { o[0] = (char)c; return 1; }
    if (c < 0x800) { o[0] = (char)(0xC0 | (c >> 6)); o[1] = (char)(0x80 | (c & 0x3F)); return 2; }
    if (c < 0x10000) {
        o[0] = (char)(0xE0 | (c >> 12)); o[1] = (char)(0x80 | ((c >> 6) & 0x3F)); o[2] = (char)(0x80 | (c & 0x3F));
        return 3;
    }
    o[0] = (char)(0xF0 | (c >> 18)); o[1] = (char)(0x80 | ((c >> 12) & 0x3F));
    o[2] = (char)(0x80 | ((c >> 6) & 0x3F)); o[3] = (char)(0x80 | (c & 0x3F));
    return 4;
}

static __attribute__((noinline)) void clair_tx_add_char(ClairTxBuf *b, uint32_t c) {
    char t[4];
    clair_tx_add(b, t, clair_tx_put(t, c));
}

/* the characters of s and where each starts (start[n] = the end) */
static __attribute__((noinline)) uint32_t *clair_tx_decode(const unsigned char *p, size_t bytes, size_t *count, size_t **starts) {
    uint32_t *c = malloc((bytes + 1) * sizeof(*c));
    size_t *at = starts ? malloc((bytes + 1) * sizeof(*at)) : NULL;
    if (!c || (starts && !at)) clair_tx_fail("out of memory", 0);
    size_t n = 0;
    for (size_t i = 0, w = 0; i < bytes; i += w) {
        if (at) at[n] = i;
        c[n++] = clair_tx_get(p, bytes, i, &w);
    }
    if (at) { at[n] = bytes; *starts = at; }
    *count = n;
    return c;
}

/* Python's whitespace (str.isspace, strip(), split()) */
static inline int clair_tx_space(uint32_t c) {
    return (c >= 9 && c <= 13) || (c >= 0x1C && c <= 0x20) || c == 0x85 || c == 0xA0 || c == 0x1680 ||
           (c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000;
}

/* ---- strip, lstrip, rstrip: chars NULL means whitespace ---- */
static const char *clair_tx_strip_side(const char *s, const char *chars, int left, int right) {
    const unsigned char *p = (const unsigned char *)s;
    size_t bytes = strlen(s), set_n = 0;
    uint32_t *set = NULL;
    if (chars) set = clair_tx_decode((const unsigned char *)chars, strlen(chars), &set_n, NULL);
    size_t first = bytes, last = 0;          /* the kept characters: bytes [first, last) */
    for (size_t i = 0, w = 0; i < bytes; i += w) {
        uint32_t c = clair_tx_get(p, bytes, i, &w);
        int drop;
        if (set) {
            drop = 0;
            for (size_t k = 0; k < set_n && !drop; k++) drop = set[k] == c;
        } else {
            drop = clair_tx_space(c);
        }
        if (!drop) {
            if (first == bytes) first = i;
            last = i + w;
        }
    }
    free(set);
    if (first == bytes) return "";          /* nothing but dropped characters */
    size_t a = left ? first : 0, b = right ? last : bytes;
    if (a == 0 && b == bytes) return s;
    return clair_tx_copy(p + a, b - a);
}

static const char *clair_tx_strip(const char *s, const char *chars) { return clair_tx_strip_side(s, chars, 1, 1); }
static const char *clair_tx_lstrip(const char *s, const char *chars) { return clair_tx_strip_side(s, chars, 1, 0); }
static const char *clair_tx_rstrip(const char *s, const char *chars) { return clair_tx_strip_side(s, chars, 0, 1); }

/* ---- searching, in characters ---- */

/* the first (last when backward) place of sub in the bytes [a, b) of s, or -1 */
static int64_t clair_tx_search(const unsigned char *p, size_t a, size_t b, const char *sub, size_t m, int backward) {
    if (b < a || b - a < m) return -1;
    if (m == 0) return (int64_t)(backward ? b : a);
    if (!backward) {
        for (size_t i = a; i + m <= b; i++) {
            const unsigned char *q = memchr(p + i, (unsigned char)sub[0], b - m + 1 - i);
            if (!q) return -1;
            i = (size_t)(q - p);
            if (memcmp(q, sub, m) == 0) return (int64_t)i;
        }
        return -1;
    }
    for (size_t i = b - m + 1; i-- > a;) if (p[i] == (unsigned char)sub[0] && memcmp(p + i, sub, m) == 0) return (int64_t)i;
    return -1;
}

/* find, rfind, index, rindex: the position in characters, or -1 */
static int64_t clair_tx_find_any(const char *s, const char *sub, int64_t start, int64_t end, int backward) {
    ClairTxView v = clair_tx_view(s);
    int64_t m_chars = clair_tx_len(sub);
    clair_tx_adjust(&start, &end, v.len);
    if (end - start < m_chars) return -1;
    size_t a = clair_tx_byte(&v, start), b = clair_tx_byte(&v, end);
    int64_t at = clair_tx_search(v.p, a, b, sub, strlen(sub), backward);
    return at < 0 ? -1 : clair_tx_char_index(&v, (size_t)at);
}

static int64_t clair_tx_find(const char *s, const char *sub, int64_t start, int64_t end) {
    return clair_tx_find_any(s, sub, start, end, 0);
}

static int64_t clair_tx_rfind(const char *s, const char *sub, int64_t start, int64_t end) {
    return clair_tx_find_any(s, sub, start, end, 1);
}

static int64_t clair_tx_index(const char *s, const char *sub, int64_t start, int64_t end, size_t line) {
    int64_t at = clair_tx_find_any(s, sub, start, end, 0);
    if (at < 0) clair_tx_fail("substring not found", line);
    return at;
}

static int64_t clair_tx_rindex(const char *s, const char *sub, int64_t start, int64_t end, size_t line) {
    int64_t at = clair_tx_find_any(s, sub, start, end, 1);
    if (at < 0) clair_tx_fail("substring not found", line);
    return at;
}

/* how many times sub is there, without overlapping */
static int64_t clair_tx_count(const char *s, const char *sub, int64_t start, int64_t end) {
    ClairTxView v = clair_tx_view(s);
    int64_t m_chars = clair_tx_len(sub);
    clair_tx_adjust(&start, &end, v.len);
    if (end - start < m_chars) return 0;
    if (m_chars == 0) return end - start + 1;
    size_t a = clair_tx_byte(&v, start), b = clair_tx_byte(&v, end), m = strlen(sub);
    int64_t count = 0;
    for (int64_t at; (at = clair_tx_search(v.p, a, b, sub, m, 0)) >= 0; a = (size_t)at + m) count++;
    return count;
}

/* startswith and endswith */
static bool clair_tx_tailmatch(const char *s, const char *sub, int64_t start, int64_t end, int at_end) {
    ClairTxView v = clair_tx_view(s);
    int64_t m_chars = clair_tx_len(sub);
    clair_tx_adjust(&start, &end, v.len);
    end -= m_chars;
    if (end < start) return false;
    if (m_chars == 0) return true;
    size_t at = clair_tx_byte(&v, at_end ? end : start), m = strlen(sub);
    return at + m <= v.bytes && memcmp(v.p + at, sub, m) == 0;
}

static bool clair_tx_startswith(const char *s, const char *sub, int64_t start, int64_t end) {
    return clair_tx_tailmatch(s, sub, start, end, 0);
}

static bool clair_tx_endswith(const char *s, const char *sub, int64_t start, int64_t end) {
    return clair_tx_tailmatch(s, sub, start, end, 1);
}

/* ---- V0.76: partition and rpartition, three texts: before, the separator, after ---- */
typedef struct { const char *a, *b, *c; } ClairTx3;

static ClairTx3 clair_tx_partition_any(const char *s, const char *sep, size_t line, int backward) {
    size_t n = strlen(s), m = strlen(sep);
    if (m == 0) clair_tx_fail("empty separator", line);
    int64_t at = clair_tx_search((const unsigned char *)s, 0, n, sep, m, backward);
    if (at < 0) return backward ? (ClairTx3){ "", "", s } : (ClairTx3){ s, "", "" };
    return (ClairTx3){ clair_tx_copy(s, (size_t)at), clair_tx_copy(sep, m),
                       clair_tx_copy(s + at + (int64_t)m, n - (size_t)at - m) };
}

static ClairTx3 clair_tx_partition(const char *s, const char *sep, size_t line) {
    return clair_tx_partition_any(s, sep, line, 0);
}

static ClairTx3 clair_tx_rpartition(const char *s, const char *sep, size_t line) {
    return clair_tx_partition_any(s, sep, line, 1);
}

static const char *clair_tx_removeprefix(const char *s, const char *prefix) {
    size_t m = strlen(prefix);
    return strncmp(s, prefix, m) == 0 ? s + m : s;
}

static const char *clair_tx_removesuffix(const char *s, const char *suffix) {
    size_t n = strlen(s), m = strlen(suffix);
    if (m == 0 || m > n || memcmp(s + n - m, suffix, m) != 0) return s;
    return clair_tx_copy(s, n - m);
}

/* replace(old, new, count): count < 0, all of them */
static const char *clair_tx_replace(const char *s, const char *old, const char *with, int64_t count) {
    size_t n = strlen(s), m = strlen(old), k = strlen(with);
    const unsigned char *p = (const unsigned char *)s;
    if (count < 0) count = INT64_MAX;
    ClairTxBuf b = { 0 };
    if (m == 0) {                            /* "abc".replace("", "-") gives -a-b-c- */
        size_t i = 0;
        while (count > 0) {
            clair_tx_add(&b, with, k);
            count--;
            if (i >= n) break;
            size_t w = 1;
            while (i + w < n && clair_tx_cont(p[i + w])) w++;
            clair_tx_add(&b, p + i, w);
            i += w;
        }
        clair_tx_add(&b, p + i, n - i);
        return clair_tx_done(&b);
    }
    size_t i = 0;
    for (int64_t at; count > 0 && (at = clair_tx_search(p, i, n, old, m, 0)) >= 0; count--) {
        clair_tx_add(&b, p + i, (size_t)at - i);
        clair_tx_add(&b, with, k);
        i = (size_t)at + m;
    }
    if (i == 0 && !b.n) { free(b.d); return s; }
    clair_tx_add(&b, p + i, n - i);
    return clair_tx_done(&b);
}

/* ---- ljust, rjust, center, zfill ---- */
static const char *clair_tx_pad(const char *s, int64_t left, int64_t right, const char *fill, size_t line) {
    if (clair_tx_len(fill) != 1) clair_tx_fail("the fill character must be exactly one character long", line);
    if (left < 0) left = 0;
    if (right < 0) right = 0;
    if (left == 0 && right == 0) return s;
    ClairTxBuf b = { 0 };
    size_t f = strlen(fill);
    for (int64_t i = 0; i < left; i++) clair_tx_add(&b, fill, f);
    clair_tx_add(&b, s, strlen(s));
    for (int64_t i = 0; i < right; i++) clair_tx_add(&b, fill, f);
    return clair_tx_done(&b);
}

static const char *clair_tx_ljust(const char *s, int64_t width, const char *fill, size_t line) {
    int64_t n = clair_tx_len(s);
    return clair_tx_pad(s, 0, width - n, fill, line);
}

static const char *clair_tx_rjust(const char *s, int64_t width, const char *fill, size_t line) {
    int64_t n = clair_tx_len(s);
    return clair_tx_pad(s, width - n, 0, fill, line);
}

static const char *clair_tx_center(const char *s, int64_t width, const char *fill, size_t line) {
    int64_t n = clair_tx_len(s);
    if (n >= width) return clair_tx_pad(s, 0, 0, fill, line);
    int64_t marg = width - n, left = marg / 2 + (marg & width & 1);
    return clair_tx_pad(s, left, marg - left, fill, line);
}

static const char *clair_tx_zfill(const char *s, int64_t width) {
    int64_t n = clair_tx_len(s);
    if (n >= width) return s;
    int64_t fill = width - n;
    size_t bytes = strlen(s);
    char *r = clair_rt_arena_alloc(bytes + (size_t)fill + 1);
    memset(r, '0', (size_t)fill);
    memcpy(r + fill, s, bytes + 1);
    if (r[fill] == '+' || r[fill] == '-') { r[0] = r[fill]; r[fill] = '0'; }
    return r;
}

/* ---- isspace, isascii ---- */
static bool clair_tx_isspace(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    size_t n = strlen(s);
    if (n == 0) return false;
    for (size_t i = 0, w = 0; i < n; i += w) if (!clair_tx_space(clair_tx_get(p, n, i, &w))) return false;
    return true;
}

static bool clair_tx_isascii(const char *s) {
    return clair_tx_is_ascii((const unsigned char *)s, strlen(s));
}

/* ---- split, rsplit, splitlines, join ---- */
#ifdef CLAIR_TX_LISTS
typedef struct { const char **items; size_t n, cap; } ClairTxPieces;

static void clair_tx_piece(ClairTxPieces *l, const unsigned char *p, size_t a, size_t b) {
    if (l->n == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 16;
        l->items = realloc(l->items, l->cap * sizeof(*l->items));
        if (!l->items) clair_tx_fail("out of memory", 0);
    }
    l->items[l->n++] = clair_tx_copy(p + a, b - a);
}

static ClairList clair_tx_list(ClairTxPieces *l, int reverse) {
    if (reverse)
        for (size_t i = 0, j = l->n; i + 1 < j; i++, j--) {
            const char *t = l->items[i]; l->items[i] = l->items[j - 1]; l->items[j - 1] = t;
        }
    ClairList r = clair_rt_list_make(l->items, l->n, sizeof(const char *));
    free(l->items);
    return r;
}

/* split(sep, maxsplit), rsplit: sep NULL, the words between whitespace */
static ClairList clair_tx_split_any(const char *s, const char *sep, int64_t maxsplit, int backward, size_t line) {
    const unsigned char *p = (const unsigned char *)s;
    size_t bytes = strlen(s);
    int64_t left = maxsplit < 0 ? INT64_MAX : maxsplit;
    ClairTxPieces l = { 0 };
    if (sep) {
        size_t m = strlen(sep);
        if (m == 0) clair_tx_fail("empty separator", line);
        if (!backward) {
            size_t i = 0;
            for (int64_t at; left-- > 0 && (at = clair_tx_search(p, i, bytes, sep, m, 0)) >= 0; i = (size_t)at + m)
                clair_tx_piece(&l, p, i, (size_t)at);
            clair_tx_piece(&l, p, i, bytes);
            return clair_tx_list(&l, 0);
        }
        size_t j = bytes;
        for (int64_t at; left-- > 0 && (at = clair_tx_search(p, 0, j, sep, m, 1)) >= 0; j = (size_t)at)
            clair_tx_piece(&l, p, (size_t)at + m, j);
        clair_tx_piece(&l, p, 0, j);
        return clair_tx_list(&l, 1);
    }
    size_t n, *at;
    uint32_t *c = clair_tx_decode(p, bytes, &n, &at);
    if (!backward) {
        size_t i = 0;
        while (left-- > 0) {
            while (i < n && clair_tx_space(c[i])) i++;
            if (i == n) break;
            size_t j = i++;
            while (i < n && !clair_tx_space(c[i])) i++;
            clair_tx_piece(&l, p, at[j], at[i]);
        }
        if (i < n) {
            while (i < n && clair_tx_space(c[i])) i++;
            if (i != n) clair_tx_piece(&l, p, at[i], at[n]);
        }
    } else {
        int64_t i = (int64_t)n - 1;
        while (left-- > 0) {
            while (i >= 0 && clair_tx_space(c[i])) i--;
            if (i < 0) break;
            int64_t j = i--;
            while (i >= 0 && !clair_tx_space(c[i])) i--;
            clair_tx_piece(&l, p, at[i + 1], at[j + 1]);
        }
        if (i >= 0) {
            while (i >= 0 && clair_tx_space(c[i])) i--;
            if (i >= 0) clair_tx_piece(&l, p, 0, at[i + 1]);
        }
    }
    free(c);
    free(at);
    return clair_tx_list(&l, backward);
}

static ClairList clair_tx_split(const char *s, const char *sep, int64_t maxsplit, size_t line) {
    return clair_tx_split_any(s, sep, maxsplit, 0, line);
}

static ClairList clair_tx_rsplit(const char *s, const char *sep, int64_t maxsplit, size_t line) {
    return clair_tx_split_any(s, sep, maxsplit, 1, line);
}

static inline int clair_tx_line_break(uint32_t c) {
    return c == '\n' || c == '\r' || c == 0x0B || c == 0x0C || c == 0x1C || c == 0x1D || c == 0x1E ||
           c == 0x85 || c == 0x2028 || c == 0x2029;
}

static ClairList clair_tx_splitlines(const char *s, bool keepends) {
    const unsigned char *p = (const unsigned char *)s;
    size_t bytes = strlen(s), n, *at;
    uint32_t *c = clair_tx_decode(p, bytes, &n, &at);
    ClairTxPieces l = { 0 };
    for (size_t i = 0, j = 0; i < n; j = i) {
        while (i < n && !clair_tx_line_break(c[i])) i++;
        size_t eol = i;
        if (i < n) {
            i += c[i] == '\r' && i + 1 < n && c[i + 1] == '\n' ? 2 : 1;
            if (keepends) eol = i;
        }
        clair_tx_piece(&l, p, at[j], at[eol]);
    }
    free(c);
    free(at);
    return clair_tx_list(&l, 0);
}

/* sep.join(xs) */
static const char *clair_tx_join(const char *sep, ClairList xs) {
    const char **items = (const char **)xs->data;
    size_t m = strlen(sep);
    ClairTxBuf b = { 0 };
    for (size_t i = 0; i < xs->len; i++) {
        if (i) clair_tx_add(&b, sep, m);
        clair_tx_add(&b, items[i], strlen(items[i]));
    }
    return clair_tx_done(&b);
}
#endif

/* ---- lower, upper, title, isalpha...: the Unicode tables ---- */
#ifdef CLAIR_TX_TABLES
/* --- tables : tools/clair_text_tables.py (Unicode 15.1.0, Python 3.13) --- */
/* lower(c) = c + ecart, par suites */
static const char clair_tx_t_lower[] =
    "1FhCZBgCmDWBgCCGBgCiBuBCCEECCDOCCDsBCCCABxHBECCEABkNBCCCCAB8MBABCCBB6MBABCDAB+EBAB0MBAB2MBABCCAB"
    "6MBAB+MCABmNBABiNBABCEABmNBABqNCABsNBECCCAB0NBABCCAB0NDABCCAB0NBABCCBByNBCCCCAB2NBABCEABCIABEBAB"
    "CCABEBABCCABEBQCCDQCCDABEBCCCCABhGBABvDBmBCCCABjICQCCIAB2iVBABCCABlKBABwiVDABCCABlMBABqEBABuEBIC"
    "CiJCCCEABCJABoHHABsCCCBqCCABgECBB+DCQBgCCIBgCkBABQJWCCGAB3DDABCCABNBABCDCBjIBPBgFBfBgCxBgBCCK0BC"
    "CCABeBMCCD+CCCDlBBgDq6ClBBgmOCABgmOGABgmOzWvCBg9rCBFBQ7kCqBB-7FDCB-7FhK0ECCKAB97OC+CCCKHBPJFBPLH"
    "BPJHBPJFBPMGCPJHBPZHBPJHBPJHBPJBBPBBBzEBABRMDBrFBABRMBBPBBBnGNBBPBBB-GBABNMBB-HBBB7HBABRqJAB51OE"
    "AB9rQBABrkQHAB4BuBPBgBUABCzZZB0Bx5BvBBgDxBABCCABt-UBABruHBABt+UDECCCAB3hVBAB5-UBAB9hVBAB7hVCABCD"
    "ABCJBB9jVBiDCCJCCCFABCuqesBCCUaCCoEMCCE8BCCLCCCCABnglCBICCFABCCABvyyCDCCCESCCCABn0yCBAB90yCBAB10"
    "yCBABh0yCBABn0yCCABjxyCBABzyyCBABpxyCBABg6BBOCCCAB-CBABl0yCBABvjlCBCCCHABCGCCCdABCs5VZBgCmmBnBBw"
    "CpEjBBwC9EKBuCCOBuCCGBuCCBBuCr3ByBBgEu-CfBgChsVfBgCh1ehBBkC";
/* upper(c) */
static const char clair_tx_t_upper[] =
    "iGhDZB-B7BABuuBrBWB-BCGB-BBAByHCuBCBCABvOCECBDOCBDsBCBDECBBAB3SBABmMDCCBDABBEABBGABBDABiGEABBBAB"
    "mKEABkIDECBDABBFABBDABBECCBDABBEABBCABwDGABBBABDCABBBABDCABBBABDCOCBBAB9ECQCBDABBBABDCABBEmBCBEQ"
    "CBJABBDBB+jVCABBFICBBAB+hVBAB4hVBAB8hVBABjNBAB7MCBB5MCABzMCAB1MBAB+0yCEAB5MBAB20yCCAB9MCABwyyCBA"
    "Bo0yCCABhNBABlNBABo0yCBABu-UBABi0yCDABlNCAB6-UBABpNDABrNIABu+UDABzNCABm0yCBABzNEAB0yyCBABzNBABpE"
    "BBBxNBABtEGAB1NLABqxyCBABkxyCnFABoFsBCCBEABBECBkIvBABrCBCBpCCQB-BBAB9BBIB-BBAB-DBBB9DCAB7DBABxDE"
    "AB9CBABrDBABPCWCBBABrFBAB-EBABOBABnHCAB-FDABBDABB1BfB-BBPB-ECgBCBK0BCBDMCBBABdC+CCByBlBB-Cq6CqBB"
    "g8FDCBg8F5XFBPjkCAB7mMBAB5mMBABnmMBBBjmMBABlmMBAB3lMBABpiMBABk8kCxHABoglCEABsuHRABwjlCzD0ECBGAB1"
    "DG+CCBBHBQJFBQLHBQJHBQJFBQMGCQJHBQJBB0EBDBsFBBBoGBBBgIBBBgHBBB8HzBBBQNABpiOSBBQPBBQEABOpLAB3BiBP"
    "BfFABBsaZBzBn6BvBB-CCABBEAB1iVBABviVCECBHABBDABBLiDCBJCCBFABBNlBB-lOCAB-lOGAB-lO0oesBCBUaCBoEMCB"
    "E8BCBLCCBDICBFABBFCCBBABgDDSCBMOCBFCCBHABBGCCBdABB9aAB-5BdvCB-8rCi8UZB-BumBnBBvCpEjBBvC8EKBtCCOB"
    "tCCGBtCCBBtCk4ByBB-Du+CfB-BhsVfB-Bj1ehBBjC";
/* title(c), quand il n'est pas upper(c) */
static const char clair_tx_t_title[] =
    "akOABCBABABABBBABCBABABABBBABCBABABABBlBABCBABABABB92DqBBADCBAh0DHBQBHBABHBQBHBABHBQBHBAEABSJABA"
    "HABSJABAnBABSJABA";
/* lower(c) en plusieurs caracteres */
static const char clair_tx_t_lower_sp[] =
    "BwJCpDnY";
/* upper(c) en plusieurs caracteres (\u00df : SS) */
static const char clair_tx_t_upper_sp[] =
    "mD-GCzCzCqDC8VuCnFCqCsYgND5coYhYgBDldoYhY3OC1pByqBvoGCoCxZBC0CoYBC3CqYBC5CqYBChC+V2FCldzYCDldzYg"
    "YCDldzYhYCDldzYiaqBCo4H5cBCp4H5cBCq4H5cBCr4H5cBCs4H5cBCt4H5cBCu4H5cBCv4H5cBCo4H5cBCp4H5cBCq4H5cB"
    "Cr4H5cBCs4H5cBCt4H5cBCu4H5cBCv4H5cBCo5H5cBCp5H5cBCq5H5cBCr5H5cBCs5H5cBCt5H5cBCu5H5cBCv5H5cBCo5H5"
    "cBCp5H5cBCq5H5cBCr5H5cBCs5H5cBCt5H5cBCu5H5cBCv5H5cBCo7H5cBCp7H5cBCq7H5cBCr7H5cBCs7H5cBCt7H5cBCu7"
    "H5cBCv7H5cBCo7H5cBCp7H5cBCq7H5cBCr7H5cBCs7H5cBCt7H5cBCu7H5cBCv7H5cDC69H5cBCxc5cBCmc5cCCxciaBDxci"
    "a5cFCxc5cGCq+H5cBC3c5cBCpc5cCC3ciaBD3cia5cFC3c5cGD5coYgYBD5coYhYDC5ciaBD5coYiaLDldoYgYBDldoYhYBC"
    "hdzYCCldiaBDldoYiaLC6-H5cBCpd5cBCvc5cCCpdiaBDpdia5cFCpd5ck42BCmCmCBCmCpCBCmCsCBDmCmCpCBDmCmCsCBC"
    "zC0CBCzC0CNCkqBmqBBCkqB1pBBCkqB7pBBCuqBmqBBCkqB9pB";
/* title(c) en plusieurs caracteres */
static const char clair_tx_t_title_sp[] =
    "X-GCzCzDolBC1pBisBrxGC69HlaCCmclaDDxcialaLCq+HlaCCpclaDD3cialarBC6-HlaCCvclaDDpdialap42BCmCmDBCm"
    "CpDBCmCsDBDmCmDpDBDmCmDsDBCzC0DBCzC0DNCkqB2rBBCkqBlrBBCkqBrrBBCuqB2rBBCkqBtrB";
/* minuscules sans majuscule */
static const char clair_tx_t_lower_extra[] =
    "pDqFAQA+DA1CAOAPBPAEAjDATFcADACADCDACADAGBCADBCGCBCADCHECACHDZIBfE2EAiEAkLAoBA07CAkgD4DCCCPCwB9G"
    "BCAyOAOARMuDAEBEAcAFAFADBJDo5CADADGnDA4teBzEB-BIWAHAaAkBACAdCEC2ZiBCGCN3gXADCCpBCIgjzBZbGCRbZbDC"
    "ACGCKbZbZbZbZbZbZbZbZbbdYCFbYCFbYCFbYCFbYCFCA15BJCTHFmI9B";
/* majuscules sans minuscule */
static const char clair_tx_t_upper_extra[] =
    "wByeCupHAFAECDCDAEEHAEAEBDBCALBGA71sDZbZbZbACBDADBDDCHbZbBCDDHCGcBCDCECAEGcZbZbZbZbZbZfYiBYiBYiB"
    "YiBYiBAmrGZHZHZ";
/* les lettres de titre (Lt) */
static const char clair_tx_t_titlecase[] =
    "KlOADADAnBA2sHHJHJHNAQAwBA";
/* Case_Ignorable (le sigma final) */
static const char clair_tx_t_ignorable[] =
    "1NnBAHAMAkBACAoCAFACAFADB4P-FFBFAKBCA8HGwGAGAyBsBCACBCBCAtBAMFLKCAkBALURAmDHCJCDiBACAfa8CK7BKFAD"
    "AZXsBCtBAIBHHqB5B4BACAFHFAEGLBOAQA7BAFDJAVBbADB6BAFBFBDCEAfBEAMB6BAFECBFAVBXFCA7BADACDJAIBMBfA+B"
    "ANAzBAEA4BACCGCCDIBMBeA7BADAHAGBVBdB6BBFDJAVBeApCAICCA7CADGMIjDADIKACGqCBcACACA4BNCECBGKCjBKAnDD"
    "CFCBDBaBFCRDOADBHAQA-CAhTCzdCeBfBfBhCBCGJADKEAGAuBE0BAiCBjBA3DCFBKAHC8GBDA7BACGCACADHHJDAoBAJeyB"
    "DxBACECAGApBINBhBDDBCC5BACBEACC7BHDBhCFzCCCMCGFAHAEBzB+BOAjBkD+NACCMCOCOCOBNEJBLADADEyBECJCAOARM"
    "0BgBs8CByDC+DAQAhDfwBA2OAlBDEEGA+CF+CC34bAjnBFvIAjDDCJCAdDxCBPhBvCAYCoDCEBJAEAFAaBGA4EBbROAnBHaK"
    "vBCxBADDDBSAWBjCFDBDBNAJAkBAMA0BACCDBGBCAcAPBGBCAlDEKC6DADAFAx5TA0EQ+RPEANPjBADAqFAIAHAMAkBACAwB"
    "AuBBkCAWCiQAjHA2EEmgBFCpBCInSCCBGDpBCFAmFB+RDkMBxCCnCKyBD8DA3BOqBADBLCyBDDBDAFALAzBClBECH-BANB1B"
    "ILDDAgDCDACBHADA+EAEHWB6BBEAmBGEEkGHDCCAYA1CFCAFBCBvHDHBCBcB2CHDACBrDACADFCAmDCDDCEkIICBhIBCAFAx"
    "EDDBFAhBJpBFDDJAKFDCvBMCB3MGCFCAzCVDGCBCB7DFEACBCGCApCBEACA8KBMB1BEGACAunFQHO70NE8BGKDsgBAgCQhCB"
    "CBsgQDCGCB-kDBCD9yEtBDWhRCKPDGfD1EC89B2BFxBJAPAXECOxqBGCQDGCBCEG9BiBAhFNxLA+BD8PEhfGuDHw1CEigjYA"
    "f-ChEvH";
/* isalpha */
static const char clair_tx_t_alpha[] =
    "0UhCZHZwBALAFAGWCeCpOFLPEIACAiEECBDDCAHACCCACTCyCCqEJlFClBDAHoBoCaFDuBqBkBBCiDCAQBIBLCDARACde4CM"
    "AZgBKBFAGVFAKAEAYYIKGXCFSpB7B1BEATAIJQPFHDBDVCGCAEDEARAOBCCPBLAJFFBDVCGCBCBCBgBDCAUCRICCCVCGCBCE"
    "EATAQBYAMHDBDVCGCBCEEAfBCCQASACFECCDEBCACBEBECELXA1BHCCCWCPEAbCDADBfAFHCCCWCJCEEAgBBCBQBSICCCoBD"
    "ARAGCJCZFGREXCICADG7BvBCBNG7BBCACECXCACJCBKADECAWDhBAgCHCjBcE0DqBVARFFDEAEBICFMNASlBCAGADqBCsKCD"
    "DGCACDDoBCDDgBCDDGCACDDOC4BCDDiCmBPR1CDFErTDQCZGqCHHIROSPRPMCCQzBkBAFAkC4CIEDhBCAGlCLeyBdDEMrBFZ"
    "3BWK0BzCA+CuBSH3BdOBLrBbjBqBCLjBDIIqBDCqBDCFCBEAG-FhC1IDFDlBDFDHCACACACeD0BCGCAECCGEDDFFMGCCG1DA"
    "OARMmDAFADJCAEEHACACACDCKDDGEFA1BB8zCkHHDEBNlBCAGAD3BIARWKGCGCGCGCGCGCGCGxCA2OBrBEGBF1CHCC5CCDGq"
    "BC9CSfxBPhQ-tGhCs0VkCtBDsIEPLBVuBReDlCyBIDmDD-BGBCACEZPCCCDCWezBPxB-BFEACBMbLWacIuBdARECJLECoBYC"
    "CHVWEAExBCAEBDEDACAZCDKICNFDFDFKGCGCqBCNHyDej9KNWFwBloItLDpDnBGNEGACJCMCECACBCBCrDiBqLT-BD1BpBL1"
    "DECmElBZHZM4CEFDFDFDCkBLCZCSCBCODNjB6DmMcEwBwBfOTCHHlBLdDjBFHxB9ETjBFjBFnBJzBNKCOCGCBCKCOCGCBkC2"
    "JKVLHZFCpBCImCFDACrBCBEADWLWKeiCSCBLVLZnC3BHBhCAQDCCCcrBcEckBHCbc1BLVLSORvDoC4ByBOyBOjB9KpBHBvCc"
    "LAJVrBRvBUcWN0B6BBDAOsBhBYbjBeADAJiBEANvBPDWACAkBRCYUBgCGCACDCOCJIuBnBHDBDVCGCBCEEATANE-E0BTDVCf"
    "vBVBCA5FuBqBDlBvBVA8BqBOAoCamBG6FrB1D-BgBHDADHCBCXQACA-CHDmBRACAdALnBIAWAMtBUAToCoIICkBSAyBdxDGC"
    "BClBWAaFCBCfPAoKSQACMChB9DAwC5cnHjGtyCgDQvhBSF69DmS6tI4RIeSuCSdTvBRDgBUGSxV-BhEqCGAjCMhCBCAd3-FJ"
    "1mBrBIo3IDCGCBCiJQAeCDAPDJrMloCqDGMEIIJn7F0CCmCCBDADBDDCLCACGCgCCDDHCGCbCDCECAEGCzKDYCYCeCYCeCYC"
    "eCYCeCYCH15BeHFmI9BzEsBLGRAiKdTrBlPb1XGCDCBCOCkG8BjCIA1lBDCaCBCADACJCDCACAHAFACACACCCBCADACACACA"
    "CACBCADDCGCDCDCACJCQGCCECQlqE-2pBhB5hEH9GDh0FPwpHQtTjtC9QjvBq6EG-iE";
/* isdecimal */
static const char clair_tx_t_decimal[] =
    "gCwBJnxBJnEJnGJ9MJ3DJ3DJ3DJ3DJ3DJ3DJ3DJ3DJ3DJhDJ3DJnCJ3IJnCJn6BJnBJtJJhEJnFJHJ3FJ3CJnEJHJnuiBJnV"
    "JnBJnGJXJ3CJ3MJ34UJnsBJnkCJtZJhEJ9BJxEJ3IJ3KJ3DJ3LJnDJnDJnNJnDJ3XJ3HJnCJnNJn4SJ3CJnEJ1jbxBhqCJnN"
    "J3PJ3iBJ30EJ";
/* isdigit */
static const char clair_tx_t_digit[] =
    "zCwBJ5DBGAntBJnEJnGJ9MJ3DJ3DJ3DJ3DJ3DJ3DJ3DJ3DJ3DJhDJ3DJnCJ3IJnCJwWIvjBJnBJtJJhEKmFJHJ3FJ3CJnEJH"
    "J3gBAEFHJ3eIMIMI6CALICA3TICICIu0fJnVJnBJnGJXJ3CJ3MJ34UJnsBJ3sBDtXJnJIqPIMJhEJ9BJxEJ3IJ3KJ3DJ3LJn"
    "DJnDJnNJnDJ3XJ3HJnCJnNJn4SJ3CJnEJ1jbxBhqCJnNJ3PJ3iBJn9BKm3CJ";
/* isnumeric */
static const char clair_tx_t_numeric[] =
    "7GwBJ5DBGADCitBJnEJnGJ9MJ3DJFFtDJ3DJ3DJDFvDM0DJJGoDJpDGISuDJhDJ3DJnCTtIJnCJwWTybCwHJHJXJtJJhEKmF"
    "JHJ3FJ3CJnEJHJ3gBAEFHJnGyBDE3W7BvCV3TdqrBAqYAaIPC4KDrEJfHCOhBJoBOmKA+DAndAjZAz1EADAEACAbA5BAvBAI"
    "ACAWATBfAJAlBAjDA5DAdA+HAiCAjBACACA0OACCHA1DD3YA2QAIAhsBAkEBNCCAgeAuBAuCAmkBAsqBAoPAwkBAoXAXA4aA"
    "50BAmdAikCAFA9DA+mBA7UA4LAlBAGAsBA+DAq5DJ9FJhKF7EJnBJnGJXJ3CJ3MJyrTAIAFA6BAfACAqBAzoBJuPsBN4BSB2"
    "KalBDeAJAnEErGJvdHaGoBIsCEXFhFBDPDtBhCI1BBfCsCEpDHZHqBGrKFxBJnJe-EJrBDxDGnEdhEJ9BJxEJIT8HJ3KJ3DJ"
    "3LJnDJnDLlNS+CJ3XckHJnCJnNJnDUshBuDyvRJ3CJnEJCG-YWqhZTNTtDY2iBxBhqCJnNJ3PJueIhEJ4Y6BCCCDtCsBCOje"
    "Mk3CJogBAjDA+DA-BApgCA5CAJAQAuKATAcA3jGAowBAjsEAy6JAjxlBA";
/* --- fin des tables --- */

typedef struct { uint32_t start, end, step; int32_t delta; } ClairTxRun;
typedef struct { uint32_t start, end; } ClairTxRange;
typedef struct { uint32_t c, n, to[3]; } ClairTxSpecial;
typedef struct { void *items; size_t n; } ClairTxTable;

enum { CLAIR_TX_LOWER, CLAIR_TX_UPPER, CLAIR_TX_TITLE, CLAIR_TX_LOWER_SP, CLAIR_TX_UPPER_SP, CLAIR_TX_TITLE_SP,
       CLAIR_TX_LOWER_X, CLAIR_TX_UPPER_X, CLAIR_TX_TITLECASE, CLAIR_TX_IGNORABLE, CLAIR_TX_ALPHA, CLAIR_TX_DECIMAL,
       CLAIR_TX_DIGIT, CLAIR_TX_NUMERIC, CLAIR_TX_TABLE_COUNT };
static ClairTxTable clair_tx_tables[CLAIR_TX_TABLE_COUNT];
static int clair_tx_tables_read = 0;

static uint32_t clair_tx_number(const char **t) {
    uint32_t v = 0;
    for (int shift = 0;; shift += 5) {
        char ch = *(*t)++;
        int x = ch >= 'A' && ch <= 'Z' ? ch - 'A' : ch >= 'a' && ch <= 'z' ? ch - 'a' + 26 :
                ch >= '0' && ch <= '9' ? ch - '0' + 52 : ch == '+' ? 62 : 63;
        v |= (uint32_t)(x & 31) << shift;
        if (!(x & 32)) return v;
    }
}

static void clair_tx_tables_free(void) {
    for (int i = 0; i < CLAIR_TX_TABLE_COUNT; i++) { free(clair_tx_tables[i].items); clair_tx_tables[i].items = NULL; }
    clair_tx_tables_read = 0;
}

static __attribute__((noinline, cold)) void clair_tx_read_tables(void) {
    static const char *const text[CLAIR_TX_TABLE_COUNT] = {
        clair_tx_t_lower, clair_tx_t_upper, clair_tx_t_title, clair_tx_t_lower_sp, clair_tx_t_upper_sp,
        clair_tx_t_title_sp, clair_tx_t_lower_extra, clair_tx_t_upper_extra, clair_tx_t_titlecase,
        clair_tx_t_ignorable, clair_tx_t_alpha, clair_tx_t_decimal, clair_tx_t_digit, clair_tx_t_numeric };
    if (atexit(clair_tx_tables_free) != 0) clair_tx_fail("cannot register memory cleanup", 0);
    for (int k = 0; k < CLAIR_TX_TABLE_COUNT; k++) {
        const char *t = text[k];
        size_t n = clair_tx_number(&t);
        uint32_t prev = 0;
        if (k <= CLAIR_TX_TITLE) {
            ClairTxRun *r = malloc((n + 1) * sizeof(*r));
            if (!r) clair_tx_fail("out of memory", 0);
            for (size_t i = 0; i < n; i++) {
                r[i].start = prev + clair_tx_number(&t);
                r[i].end = r[i].start + clair_tx_number(&t);
                r[i].step = clair_tx_number(&t);
                uint32_t z = clair_tx_number(&t);
                r[i].delta = (z & 1) ? -(int32_t)((z + 1) / 2) : (int32_t)(z / 2);
                prev = r[i].end;
            }
            clair_tx_tables[k].items = r;
        } else if (k <= CLAIR_TX_TITLE_SP) {
            ClairTxSpecial *r = malloc((n + 1) * sizeof(*r));
            if (!r) clair_tx_fail("out of memory", 0);
            for (size_t i = 0; i < n; i++) {
                r[i].c = prev + clair_tx_number(&t);
                r[i].n = clair_tx_number(&t);
                for (uint32_t j = 0; j < r[i].n && j < 3; j++) r[i].to[j] = clair_tx_number(&t);
                prev = r[i].c;
            }
            clair_tx_tables[k].items = r;
        } else {
            ClairTxRange *r = malloc((n + 1) * sizeof(*r));
            if (!r) clair_tx_fail("out of memory", 0);
            for (size_t i = 0; i < n; i++) {
                r[i].start = prev + clair_tx_number(&t);
                r[i].end = r[i].start + clair_tx_number(&t);
                prev = r[i].end;
            }
            clair_tx_tables[k].items = r;
        }
        clair_tx_tables[k].n = n;
    }
    clair_tx_tables_read = 1;
}

/* the last entry starting at or before c (entries sorted, size of one entry: size), or NULL */
static __attribute__((noinline)) const void *clair_tx_find_entry(int k, uint32_t c, size_t size) {
    if (!clair_tx_tables_read) clair_tx_read_tables();
    const unsigned char *items = clair_tx_tables[k].items;
    size_t lo = 0, hi = clair_tx_tables[k].n;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        uint32_t start;
        memcpy(&start, items + mid * size, sizeof(start));
        if (start <= c) lo = mid + 1;
        else hi = mid;
    }
    return lo ? items + (lo - 1) * size : NULL;
}

static __attribute__((noinline)) int clair_tx_in(int k, uint32_t c) {
    const ClairTxRange *r = clair_tx_find_entry(k, c, sizeof(ClairTxRange));
    return r && c <= r->end;
}

/* c's lower (0), upper (1) or title (2) form, of 1 to 3 characters; returns how many */
static __attribute__((noinline)) int clair_tx_map(int which, uint32_t c, uint32_t out[3]) {
    if (c & CLAIR_TX_RAW) { out[0] = c; return 1; }
    if (c < 0x80) {
        out[0] = which == 0 ? (c >= 'A' && c <= 'Z' ? c + 32 : c) : (c >= 'a' && c <= 'z' ? c - 32 : c);
        return 1;
    }
    const ClairTxSpecial *sp = clair_tx_find_entry(CLAIR_TX_LOWER_SP + which, c, sizeof(ClairTxSpecial));
    if (sp && sp->c == c) {
        for (uint32_t j = 0; j < sp->n; j++) out[j] = sp->to[j];
        return (int)sp->n;
    }
    const ClairTxRun *r = clair_tx_find_entry(CLAIR_TX_LOWER + which, c, sizeof(ClairTxRun));
    if (r && c <= r->end && (c - r->start) % r->step == 0) {
        out[0] = (uint32_t)((int32_t)c + r->delta);
        return 1;
    }
    if (which == 2) return clair_tx_map(1, c, out);
    out[0] = c;
    return 1;
}

static __attribute__((noinline)) int clair_tx_changes(int which, uint32_t c) {
    uint32_t o[3];
    return clair_tx_map(which, c, o) != 1 || o[0] != c;
}

static __attribute__((noinline)) int clair_tx_lower_c(uint32_t c) {
    if (c & CLAIR_TX_RAW) return 0;
    return (clair_tx_changes(1, c) && !clair_tx_in(CLAIR_TX_TITLECASE, c)) || clair_tx_in(CLAIR_TX_LOWER_X, c);
}

static __attribute__((noinline)) int clair_tx_upper_c(uint32_t c) {
    if (c & CLAIR_TX_RAW) return 0;
    return (clair_tx_changes(0, c) && !clair_tx_in(CLAIR_TX_TITLECASE, c)) || clair_tx_in(CLAIR_TX_UPPER_X, c);
}

static __attribute__((noinline)) int clair_tx_title_c(uint32_t c) { return !(c & CLAIR_TX_RAW) && clair_tx_in(CLAIR_TX_TITLECASE, c); }

static __attribute__((noinline)) int clair_tx_cased(uint32_t c) { return clair_tx_lower_c(c) || clair_tx_upper_c(c) || clair_tx_title_c(c); }

static __attribute__((noinline)) int clair_tx_ignorable(uint32_t c) { return !(c & CLAIR_TX_RAW) && clair_tx_in(CLAIR_TX_IGNORABLE, c); }

/* the capital sigma at i ends a word: it becomes the final sigma, like Python */
static __attribute__((noinline)) uint32_t clair_tx_sigma(const uint32_t *c, size_t n, size_t i) {
    size_t j = i;
    uint32_t x = 0;
    int found = 0;
    while (j > 0) {
        x = c[--j];
        if (!clair_tx_ignorable(x)) { found = 1; break; }
    }
    int final = found && clair_tx_cased(x);
    if (final && i + 1 < n) {
        for (j = i + 1; j < n; j++) if (!clair_tx_ignorable(c[j])) break;
        final = j == n || !clair_tx_cased(c[j]);
    }
    return final ? 0x3C2 : 0x3C3;
}

/* 0 lower, 1 upper, 2 swapcase, 3 capitalize, 4 title */
static const char *clair_tx_case(const char *s, int mode) {
    const unsigned char *p = (const unsigned char *)s;
    size_t bytes = strlen(s);
    if (clair_tx_is_ascii(p, bytes)) {
        char *r = clair_rt_arena_alloc(bytes + 1);
        int prev = 0;
        for (size_t i = 0; i < bytes; i++) {
            unsigned char c = p[i];
            int up = c >= 'A' && c <= 'Z', low = c >= 'a' && c <= 'z';
            int to_upper = mode == 1 || (mode == 2 && low) || (mode == 3 && i == 0) || (mode == 4 && !prev);
            if (mode == 2 && up) to_upper = 0;
            if (to_upper && low) c = (unsigned char)(c - 32);
            else if (!to_upper && up && mode != 1) c = (unsigned char)(c + 32);
            prev = up || low;
            r[i] = (char)c;
        }
        r[bytes] = '\0';
        return r;
    }
    size_t n;
    uint32_t *c = clair_tx_decode(p, bytes, &n, NULL);
    ClairTxBuf b = { 0 };
    int prev = 0;
    for (size_t i = 0; i < n; i++) {
        uint32_t x = c[i], out[3];
        int which;
        if (mode == 0 || mode == 1) which = mode;
        else if (mode == 2) which = clair_tx_upper_c(x) ? 0 : clair_tx_lower_c(x) ? 1 : -1;
        else if (mode == 3) which = i == 0 ? 2 : 0;
        else { which = prev ? 0 : 2; prev = clair_tx_cased(x); }
        if (which < 0) { clair_tx_add_char(&b, x); continue; }
        int k;
        if (which == 0 && x == 0x3A3) { out[0] = clair_tx_sigma(c, n, i); k = 1; }
        else k = clair_tx_map(which, x, out);
        for (int j = 0; j < k; j++) clair_tx_add_char(&b, out[j]);
    }
    free(c);
    return clair_tx_done(&b);
}

static const char *clair_tx_lower(const char *s) { return clair_tx_case(s, 0); }
static const char *clair_tx_upper(const char *s) { return clair_tx_case(s, 1); }
static const char *clair_tx_swapcase(const char *s) { return clair_tx_case(s, 2); }
static const char *clair_tx_capitalize(const char *s) { return clair_tx_case(s, 3); }
static const char *clair_tx_title(const char *s) { return clair_tx_case(s, 4); }

/* every character is alpha (0), decimal (1), digit (2), numeric (3), alnum (4); not empty */
static bool clair_tx_all(const char *s, int what) {
    const unsigned char *p = (const unsigned char *)s;
    size_t n = strlen(s);
    if (n == 0) return false;
    for (size_t i = 0, w = 0; i < n; i += w) {
        uint32_t c = clair_tx_get(p, n, i, &w);
        if (c & CLAIR_TX_RAW) return false;
        int ok;
        if (c < 0x80) {
            int letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'), digit = c >= '0' && c <= '9';
            ok = what == 0 ? letter : what == 4 ? letter || digit : digit;
        } else if (what == 4) {
            ok = clair_tx_in(CLAIR_TX_ALPHA, c) || clair_tx_in(CLAIR_TX_NUMERIC, c);
        } else {
            ok = clair_tx_in(CLAIR_TX_ALPHA + what, c);
        }
        if (!ok) return false;
    }
    return true;
}

static bool clair_tx_isalpha(const char *s) { return clair_tx_all(s, 0); }
static bool clair_tx_isdecimal(const char *s) { return clair_tx_all(s, 1); }
static bool clair_tx_isdigit(const char *s) { return clair_tx_all(s, 2); }
static bool clair_tx_isnumeric(const char *s) { return clair_tx_all(s, 3); }
static bool clair_tx_isalnum(const char *s) { return clair_tx_all(s, 4); }

/* islower (0), isupper (1), istitle (2), like Python */
static bool clair_tx_case_is(const char *s, int what) {
    const unsigned char *p = (const unsigned char *)s;
    size_t n = strlen(s);
    int cased = 0, prev = 0;
    for (size_t i = 0, w = 0; i < n; i += w) {
        uint32_t c = clair_tx_get(p, n, i, &w);
        int up = clair_tx_upper_c(c), low = clair_tx_lower_c(c), title = clair_tx_title_c(c);
        if (what == 0) {
            if (up || title) return false;
            if (low) cased = 1;
        } else if (what == 1) {
            if (low || title) return false;
            if (up) cased = 1;
        } else if (up || title) {
            if (prev) return false;
            prev = cased = 1;
        } else if (low) {
            if (!prev) return false;
            prev = cased = 1;
        } else {
            prev = 0;
        }
    }
    return cased;
}

static bool clair_tx_islower(const char *s) { return clair_tx_case_is(s, 0); }
static bool clair_tx_isupper(const char *s) { return clair_tx_case_is(s, 1); }
static bool clair_tx_istitle(const char *s) { return clair_tx_case_is(s, 2); }
#endif

/* s * n: n copies, one after the other (n <= 0: "") */
static const char *clair_tx_repeat(const char *s, int64_t n) {
    size_t m = strlen(s);
    if (n <= 0 || m == 0) return "";
    if ((uint64_t)n > (SIZE_MAX - 1) / m) clair_tx_fail("string too large", 0);
    size_t total = m * (size_t)n;
    char *r = clair_rt_arena_alloc(total + 1);
    memcpy(r, s, m);
    for (size_t done = m; done < total;) {
        size_t step = done < total - done ? done : total - done;
        memcpy(r + done, r, step);
        done += step;
    }
    r[total] = '\0';
    return r;
}
