/* pidigits (The Computer Language Benchmarks Game), the same algorithm as pidigits.cl,
 * in C with GMP (the library of the official C program).  Termux: pkg install libgmp */
#include <gmp.h>
#include <stdio.h>

/* noyau: chiffres */
static void chiffres(long n) {
    mpz_t acc, den, num, t1, t2;
    mpz_init_set_ui(acc, 0);
    mpz_init_set_ui(den, 1);
    mpz_init_set_ui(num, 1);
    mpz_init(t1);
    mpz_init(t2);
    char ligne[11];
    int dans_ligne = 0;
    long k = 0, i = 0;
    while (i < n) {
        k++;
        unsigned long k2 = 2 * (unsigned long)k + 1;
        mpz_addmul_ui(acc, num, 2);
        mpz_mul_ui(acc, acc, k2);
        mpz_mul_ui(den, den, k2);
        mpz_mul_ui(num, num, (unsigned long)k);
        if (mpz_cmp(num, acc) > 0) continue;
        mpz_mul_ui(t1, num, 3);
        mpz_add(t1, t1, acc);
        mpz_tdiv_q(t1, t1, den);
        unsigned long d = mpz_get_ui(t1);
        mpz_mul_ui(t2, num, 4);
        mpz_add(t2, t2, acc);
        mpz_tdiv_q(t2, t2, den);
        if (d != mpz_get_ui(t2)) continue;
        ligne[dans_ligne++] = (char)('0' + d);
        i++;
        if (i % 10 == 0) {
            ligne[dans_ligne] = 0;
            printf("%s\t:%ld\n", ligne, i);
            dans_ligne = 0;
        }
        mpz_submul_ui(acc, den, d);
        mpz_mul_ui(acc, acc, 10);
        mpz_mul_ui(num, num, 10);
    }
    if (i % 10 != 0) {
        while (dans_ligne < 10) ligne[dans_ligne++] = ' ';
        ligne[dans_ligne] = 0;
        printf("%s\t:%ld\n", ligne, i);
    }
    mpz_clear(acc); mpz_clear(den); mpz_clear(num); mpz_clear(t1); mpz_clear(t2);
}

int main(void) {
    long n = 0;
    if (scanf("%ld", &n) != 1 || n < 1) return 1;
    chiffres(n);
    return 0;
}
