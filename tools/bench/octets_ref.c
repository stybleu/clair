/* Same program written by hand in C, compiled with the same flags. */
/* noyau: moyenne */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void moyenne(const uint8_t *a, const uint8_t *b, uint8_t *c, long n) {
    for (long i = 0; i < n; i++) c[i] = (uint8_t)((a[i] >> 1) + (b[i] >> 1) + (a[i] & b[i] & 1));
}

int main(void) {
    long n = 65536;
    uint8_t *a = malloc(n), *b = malloc(n), *c = malloc(n);
    for (long i = 0; i < n; i++) { a[i] = (uint8_t)(i * 7); b[i] = (uint8_t)(i * 13 + 5); c[i] = 0; }
    for (long r = 0; r < 60000; r++) {
        moyenne(a, b, c, n);
        a[r % 65536] = c[r * 3 % 65536];
    }
    long total = 0;
    for (long i = 0; i < n; i++) total += c[i];
    printf("%ld\n", total);
    return 0;
}
