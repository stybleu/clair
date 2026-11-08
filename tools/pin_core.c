/*
 * Used by rapport_code_machine.sh so that Clair and C run on the same core.
 * A phone moves a program between its fast and slow cores, which changed a
 * measured time by up to 25 % from one run to the next.
 *
 *   pin_core best          prints "<core> <max kHz>": the fastest core this
 *                          process may use (the highest number on a tie)
 *   pin_core <core> cmd... runs cmd on that core only
 */
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static long max_khz(int core) {
    char path[96];
    long khz = 0;
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", core);
    FILE *f = fopen(path, "r");
    if (f) {
        if (fscanf(f, "%ld", &khz) != 1) khz = 0;
        fclose(f);
    }
    return khz;
}

int main(int argc, char **argv) {
    if (argc == 2) {
        cpu_set_t allowed;
        if (sched_getaffinity(0, sizeof(allowed), &allowed) != 0) return 1;
        int best = -1;
        long best_khz = -1;
        for (int core = 0; core < CPU_SETSIZE; core++) {
            if (!CPU_ISSET(core, &allowed)) continue;
            long khz = max_khz(core);
            if (khz >= best_khz) {
                best = core;
                best_khz = khz;
            }
        }
        if (best < 0) return 1;
        printf("%d %ld\n", best, best_khz);
        return 0;
    }
    if (argc < 3) {
        fprintf(stderr, "usage: pin_core best | pin_core <core> cmd...\n");
        return 2;
    }
    cpu_set_t one;
    CPU_ZERO(&one);
    CPU_SET(atoi(argv[1]), &one);
    if (sched_setaffinity(0, sizeof(one), &one) != 0) {
        perror("pin_core");
        return 126;
    }
    execvp(argv[2], argv + 2);
    perror("pin_core");
    return 127;
}
