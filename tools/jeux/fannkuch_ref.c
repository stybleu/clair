/* fannkuch-redux (The Computer Language Benchmarks Game), the same algorithm as fannkuch.cl, in C. */
#include <stdint.h>
#include <stdio.h>

/* noyau: fannkuch */
static int64_t fannkuch(int64_t n) {
    int64_t perm[32], perm1[32], count[32];
    for (int64_t i = 0; i < n; i++) { perm[i] = 0; perm1[i] = i; count[i] = 0; }
    int64_t max_flips = 0, checksum = 0, perm_count = 0, r = n;
    for (;;) {
        while (r != 1) { count[r - 1] = r; r--; }
        for (int64_t i = 0; i < n; i++) perm[i] = perm1[i];
        int64_t flips = 0, k = perm[0];
        while (k != 0) {
            int64_t lo = 0, hi = k;
            while (lo < hi) { int64_t t = perm[lo]; perm[lo] = perm[hi]; perm[hi] = t; lo++; hi--; }
            flips++;
            k = perm[0];
        }
        if (flips > max_flips) max_flips = flips;
        checksum += perm_count % 2 == 0 ? flips : -flips;
        for (;;) {
            if (r == n) { printf("%lld\n", (long long)checksum); return max_flips; }
            int64_t perm0 = perm1[0];
            for (int64_t i = 0; i < r; i++) perm1[i] = perm1[i + 1];
            perm1[r] = perm0;
            count[r]--;
            if (count[r] > 0) break;
            r++;
        }
        perm_count++;
    }
}

int main(void) {
    long long n = 0;
    if (scanf("%lld", &n) != 1 || n < 1 || n > 31) return 1;
    int64_t m = fannkuch(n);
    printf("Pfannkuchen(%lld) = %lld\n", n, (long long)m);
    return 0;
}
