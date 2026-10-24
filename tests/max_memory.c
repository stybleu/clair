/* Runs a program and prints the most memory it used, in MB (tests of V0.28). */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    pid_t pid = fork();
    if (pid < 0) return 2;
    if (pid == 0) {
        int devnull_ok = freopen("/dev/null", "w", stdout) != NULL;
        (void)devnull_ok;
        execvp(argv[1], argv + 1);
        _exit(127);
    }
    int status = 0;
    struct rusage usage;
    if (wait4(pid, &status, 0, &usage) < 0) return 2;
    printf("%ld\n", usage.ru_maxrss / 1024);   /* ru_maxrss is in KB on Linux and Android */
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
