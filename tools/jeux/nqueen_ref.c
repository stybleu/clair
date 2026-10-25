/* nqueen (PLB2, attractivechaos/plb2), the same algorithm as nqueen.cl, in C. */
#include <stdint.h>
#include <stdio.h>

#define NQ_MAX 31

/* noyau: reines */
static int64_t reines(int n) {
    int a[NQ_MAX];
    uint32_t l[NQ_MAX], c[NQ_MAX], r[NQ_MAX];
    uint32_t y0 = (1U << n) - 1;
    int64_t solutions = 0;
    for (int k = 0; k < n; k++) a[k] = -1, l[k] = c[k] = r[k] = 0;
    for (int k = 0; k >= 0;) {
        uint32_t y = (l[k] | c[k] | r[k]) & y0;
        if ((y ^ y0) >> (a[k] + 1)) {
            int i;
            for (i = a[k] + 1; i < n; i++)
                if ((y & 1U << i) == 0) break;
            if (k < n - 1) {
                uint32_t z = 1U << i;
                a[k++] = i;
                l[k] = (l[k - 1] | z) << 1;
                c[k] = c[k - 1] | z;
                r[k] = (r[k - 1] | z) >> 1;
            } else {
                solutions++, k--;
            }
        } else {
            a[k] = -1, k--;
        }
    }
    return solutions;
}

int main(void) {
    int n = 0;
    if (scanf("%d", &n) != 1 || n < 1 || n > NQ_MAX) return 1;
    printf("%lld\n", (long long)reines(n));
    return 0;
}
