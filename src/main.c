#include "parser.h"
#include "codegen_c.h"
#include "semantic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long size = ftell(f);
    if (size < 0) { fclose(f); return NULL; }
    rewind(f);

    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) { free(buf); return NULL; }
    buf[size] = '\0';
    return buf;
}

static int run_clang(const char *cpath, const char *output) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("clair: fork");
        return 1;
    }

    if (pid == 0) {
        char *const args[] = {
            "clang",
            "-std=c11",
            "-O2",
            "-fwrapv",
            "-Wall",
            "-Wextra",
            "-Wno-unused-function",
            "-fno-math-errno",   /* Clair never reads errno: lets Clang vectorize sqrt() and friends */
            (char *)cpath,
            "-o",
            (char *)output,
            "-lm",
            NULL
        };
        execvp("clang", args);
        perror("clair: clang");
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("clair: waitpid");
        return 1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "clair: clang failed\n");
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4 || strcmp(argv[2], "-o") != 0) {
        fprintf(stderr, "usage: %s input.cl -o output\n", argv[0]);
        return 2;
    }

    const char *input = argv[1];
    const char *output = argv[3];

    char *source = read_file(input);
    if (!source) {
        fprintf(stderr, "clair: cannot read %s\n", input);
        return 1;
    }

    Parser parser;
    parser_init(&parser, source);
    AstNode *program = parser_parse_program(&parser);
    if (!program || parser.had_error) {
        ast_free(program);
        free(source);
        return 1;
    }

    if (!semantic_check(program)) {
        ast_free(program);
        free(source);
        return 1;
    }
    semantic_mangle_c_identifiers(program);

    size_t cpath_len = strlen(output) + strlen(".generated.c") + 1;
    char *cpath = malloc(cpath_len);
    if (!cpath) {
        ast_free(program);
        free(source);
        return 1;
    }
    snprintf(cpath, cpath_len, "%s.generated.c", output);

    if (!codegen_c_file(program, cpath)) {
        fprintf(stderr, "clair: C generation failed\n");
        free(cpath);
        ast_free(program);
        free(source);
        return 1;
    }

    int rc = run_clang(cpath, output);

    free(cpath);
    ast_free(program);
    free(source);
    return rc;
}
