/* Same program written by hand in C, compiled with the same flags. */
#include <stdint.h>
#include <stdio.h>

static int64_t steps(int64_t n) {
    int64_t count = 0, x = n;
    while (x != 1) {
        if (x / 2 * 2 == x) x = x / 2;
        else x = 3 * x + 1;
        count = count + 1;
    }
    return count;
}

int main(void) {
    int64_t best = 0;
    for (int64_t i = 1; i < 600000; i++) {
        int64_t s = steps(i);
        if (s > best) best = s;
    }
    printf("%lld\n", (long long)best);
    return 0;
}
