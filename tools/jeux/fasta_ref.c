/* fasta (The Computer Language Benchmarks Game), the same algorithm as fasta.cl, in C. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void repete(const char *seq, int64_t n, char *ligne) {
    int64_t taille = (int64_t)strlen(seq), pos = 0;
    while (n > 0) {
        int64_t m = n < 60 ? n : 60;
        for (int64_t i = 0; i < m; i++) {
            ligne[i] = seq[pos];
            if (++pos == taille) pos = 0;
        }
        ligne[m] = '\n';
        fwrite(ligne, 1, (size_t)m + 1, stdout);
        n -= m;
    }
}

/* noyau: au_hasard */
static int64_t au_hasard(const char *lettres, const double *cumul, int64_t compte, int64_t n, int64_t g, char *ligne) {
    int64_t dernier = compte - 1;
    while (n > 0) {
        int64_t m = n < 60 ? n : 60;
        for (int64_t i = 0; i < m; i++) {
            g = (g * 3877 + 29573) % 139968;
            double r = (double)g / 139968.0;
            int64_t k = 0;
            while (k < dernier && r >= cumul[k]) k++;
            ligne[i] = lettres[k];
        }
        ligne[m] = '\n';
        fwrite(ligne, 1, (size_t)m + 1, stdout);
        n -= m;
    }
    return g;
}

static void cumule(const double *p, double *c, int n) {
    double s = 0.0;
    for (int i = 0; i < n; i++) {
        s += p[i];
        c[i] = s;
    }
}

int main(void) {
    long long n = 0;
    if (scanf("%lld", &n) != 1) return 1;
    static const char alu[] =
        "GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGA"
        "CCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCA"
        "GCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGGAGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCC"
        "AGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA";
    static const double p_iub[15] = { 0.27, 0.12, 0.12, 0.27, 0.02, 0.02, 0.02, 0.02,
                                      0.02, 0.02, 0.02, 0.02, 0.02, 0.02, 0.02 };
    static const double p_homo[4] = { 0.3029549426680, 0.1979883004921, 0.1975473066391, 0.3015094502008 };
    double c_iub[15], c_homo[4];
    char ligne[61];
    cumule(p_iub, c_iub, 15);
    cumule(p_homo, c_homo, 4);

    puts(">ONE Homo sapiens alu");
    repete(alu, n * 2, ligne);
    puts(">TWO IUB ambiguity codes");
    int64_t g = au_hasard("acgtBDHKMNRSVWY", c_iub, 15, n * 3, 42, ligne);
    puts(">THREE Homo sapiens frequency");
    au_hasard("acgt", c_homo, 4, n * 5, g, ligne);
    return 0;
}
