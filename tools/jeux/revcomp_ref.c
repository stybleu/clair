/* reverse-complement (The Computer Language Benchmarks Game), the same algorithm as revcomp.cl, in C. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* noyau: inverse */
static void inverse(const uint8_t *seq, int64_t n, const uint8_t *comp, uint8_t *ligne) {
    int64_t j = n - 1;
    while (j >= 0) {
        int64_t m = j + 1 < 60 ? j + 1 : 60;
        for (int64_t i = 0; i < m; i++) ligne[i] = comp[seq[j - i]];
        ligne[m] = '\n';
        fwrite(ligne, 1, (size_t)m + 1, stdout);
        j -= m;
    }
}

int main(void) {
    size_t capacity = 1 << 16, n = 0;
    uint8_t *data = malloc(capacity);
    for (;;) {
        if (!data) return 1;
        n += fread(data + n, 1, capacity - n, stdin);
        if (n < capacity) break;
        capacity *= 2;
        data = realloc(data, capacity);
    }
    uint8_t comp[256], ligne[61];
    for (int i = 0; i < 256; i++) comp[i] = (uint8_t)i;
    const char *de = "ACBDGHKMNSRUTWVYacbdghkmnsrutwvy", *vers = "TGVHCDMKNSYAAWBRTGVHCDMKNSYAAWBR";
    for (size_t i = 0; de[i]; i++) comp[(uint8_t)de[i]] = (uint8_t)vers[i];
    uint8_t *seq = malloc(n + 1);
    if (!seq) return 1;
    size_t i = 0;
    while (i < n) {
        size_t debut = i;
        while (i < n && data[i] != '\n') i++;
        fwrite(data + debut, 1, i - debut, stdout);
        fputc('\n', stdout);
        i++;
        int64_t k = 0;
        while (i < n && data[i] != '>') {
            size_t fin = i;
            while (fin < n && data[fin] != '\n') fin++;
            for (size_t t = i; t < fin; t++) seq[k + (int64_t)(t - i)] = data[t];
            k += (int64_t)(fin - i);
            i = fin + 1;
        }
        inverse(seq, k, comp, ligne);
    }
    free(seq);
    free(data);
    return 0;
}
