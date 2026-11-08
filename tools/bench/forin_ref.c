/* Same program written by hand in C, compiled with the same flags. */
/* noyau: au_dessus */
#include <stdio.h>
#include <stdlib.h>

static long au_dessus(const long *x, long n, long seuil) {
    long total = 0;
    for (long i = 0; i < n; i++) {
        if (x[i] > seuil) total += x[i];
    }
    return total;
}

int main(void) {
    long n = 4096;
    long *x = malloc(n * sizeof(long));
    for (long i = 0; i < n; i++) x[i] = i * 7919 % 1000;
    long s = 0;
    for (long r = 0; r < 300000; r++) s += au_dessus(x, n, r % 1000);
    printf("%ld\n", s);
    return 0;
}
