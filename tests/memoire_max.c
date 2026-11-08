/* memoire_max prog : lance prog, puis ecrit sur stderr sa memoire maximale en Kio.
 * (ulimit -v ne marche pas sur Android : l'allocateur reserve beaucoup d'adresses d'avance) */
#define _GNU_SOURCE   /* wait4 */
#include <stdio.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    pid_t pid = fork();
    if (pid < 0) return 2;
    if (pid == 0) {
        execv(argv[1], argv + 1);
        _exit(127);
    }
    int status = 0;
    struct rusage usage;
    if (wait4(pid, &status, 0, &usage) < 0) return 2;
    fprintf(stderr, "%ld\n", usage.ru_maxrss);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}
