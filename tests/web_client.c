/*
 * web_client : un petit client HTTP pour tester les serveurs du module web (V0.55).
 *
 *   web_client PORT METHODE CHEMIN [corps=CORPS] [En-tete: valeur]...
 *       envoie une requete a 127.0.0.1:PORT et ecrit le code, quelques en-tetes
 *       (content-type, content-length, location, set-cookie, allow) et le corps ;
 *       dans CORPS, \r et \n sont des fins de ligne.
 *   web_client PORT raw TEXTE
 *       envoie TEXTE tel quel (avec \r et \n) et ecrit la premiere ligne de la reponse.
 *   web_client PORT tete CHEMIN
 *       une requete GET, ecrit tous les en-tetes de la reponse.
 *   web_client PORT repete N CHEMIN
 *       N requetes GET, ecrit combien ont repondu 200.
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

static char *unescape(const char *s, size_t *len) {
    size_t n = strlen(s), w = 0;
    char *d = malloc(n + 1);
    if (!d) exit(2);
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '\\' && i + 1 < n && (s[i + 1] == 'r' || s[i + 1] == 'n' || s[i + 1] == '\\')) {
            d[w++] = s[i + 1] == 'r' ? '\r' : s[i + 1] == 'n' ? '\n' : '\\';
            i++;
        } else {
            d[w++] = s[i];
        }
    }
    d[w] = '\0';
    *len = w;
    return d;
}

static int connect_to(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    if (fd < 0 || connect(fd, (struct sockaddr *)&a, sizeof(a)) != 0) {
        fprintf(stderr, "web_client: pas de serveur sur le port %d\n", port);
        exit(2);
    }
    return fd;
}

static void send_all(int fd, const char *p, size_t n) {
    while (n) {
        ssize_t w = send(fd, p, n, 0);
        if (w <= 0) return;
        p += w;
        n -= (size_t)w;
    }
}

/* the whole answer (the server closes the connection) */
static char *receive(int fd, size_t *len) {
    size_t cap = 65536, n = 0;
    char *b = malloc(cap + 1);
    if (!b) exit(2);
    for (;;) {
        if (n == cap) {
            cap *= 2;
            b = realloc(b, cap + 1);
            if (!b) exit(2);
        }
        ssize_t r = recv(fd, b + n, cap - n, 0);
        if (r <= 0) break;
        n += (size_t)r;
    }
    b[n] = '\0';
    *len = n;
    return b;
}

static char *request(int port, const char *method, const char *path, const char *body, size_t body_len,
                     char **headers, int nh, size_t *len) {
    int fd = connect_to(port);
    char head[8192];
    int own_host = 0;
    for (int i = 0; i < nh; i++) own_host |= strncasecmp(headers[i], "Host:", 5) == 0;
    int k = own_host ? snprintf(head, sizeof(head), "%s %s HTTP/1.1\r\n", method, path)
                     : snprintf(head, sizeof(head), "%s %s HTTP/1.1\r\nHost: 127.0.0.1:%d\r\n", method, path, port);
    for (int i = 0; i < nh; i++) k += snprintf(head + k, sizeof(head) - (size_t)k, "%s\r\n", headers[i]);
    if (body) k += snprintf(head + k, sizeof(head) - (size_t)k, "Content-Length: %zu\r\n", body_len);
    k += snprintf(head + k, sizeof(head) - (size_t)k, "\r\n");
    send_all(fd, head, (size_t)k);
    if (body) send_all(fd, body, body_len);
    char *answer = receive(fd, len);
    close(fd);
    return answer;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fputs("usage: web_client PORT METHODE CHEMIN [corps=CORPS] [En-tete: valeur]...\n", stderr);
        return 2;
    }
    int port = atoi(argv[1]);
    size_t len = 0;
    if (strcmp(argv[2], "raw") == 0) {
        size_t n;
        char *text = unescape(argv[3], &n);
        int fd = connect_to(port);
        send_all(fd, text, n);
        char *answer = receive(fd, &len);
        close(fd);
        char *eol = strstr(answer, "\r\n");
        if (eol) *eol = '\0';
        printf("%s\n", answer);
        return 0;
    }
    if (strcmp(argv[2], "tete") == 0) {                  /* every header of the answer */
        char *answer = request(port, "GET", argv[3], NULL, 0, NULL, 0, &len);
        char *end = strstr(answer, "\r\n\r\n");
        if (end) *end = '\0';
        for (char *p = answer; *p; p++) if (*p == '\r') *p = ' ';
        printf("%s\n", answer);
        return 0;
    }
    if (strcmp(argv[2], "repete") == 0) {
        int count = atoi(argv[3]), ok = 0;
        for (int i = 0; i < count; i++) {
            char *answer = request(port, "GET", argv[4], NULL, 0, NULL, 0, &len);
            if (strncmp(answer, "HTTP/1.1 200 ", 13) == 0) ok++;
            free(answer);
        }
        printf("%d\n", ok);
        return 0;
    }
    const char *body = NULL;
    size_t body_len = 0;
    int first_header = 4;
    if (argc > 4 && strncmp(argv[4], "corps=", 6) == 0) {
        body = unescape(argv[4] + 6, &body_len);
        first_header = 5;
    }
    char *answer = request(port, argv[2], argv[3], body, body_len, argv + first_header, argc - first_header, &len);
    char *end = strstr(answer, "\r\n\r\n");
    if (!end) {
        printf("pas de reponse\n");
        return 1;
    }
    *end = '\0';
    char *line = answer, *next = strstr(line, "\r\n");
    if (next) { *next = '\0'; next += 2; }
    printf("%s\n", strchr(line, ' ') ? strchr(line, ' ') + 1 : line);
    static const char *const shown[] = { "content-type", "content-length", "location", "set-cookie", "allow", NULL };
    for (int s = 0; shown[s]; s++) {
        for (char *h = next; h && *h;) {
            char *nl = strstr(h, "\r\n");
            if (nl) *nl = '\0';
            size_t n = strlen(shown[s]);
            if (strncasecmp(h, shown[s], n) == 0 && h[n] == ':') printf("%s:%s\n", shown[s], h + n + 1);
            if (nl) *nl = '\r';
            h = nl ? nl + 2 : NULL;
        }
    }
    printf("\n");
    fwrite(end + 4, 1, len - (size_t)(end + 4 - answer), stdout);
    printf("\n");
    return 0;
}
