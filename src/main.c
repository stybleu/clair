#include "parser.h"
#include "codegen_c.h"
#include "semantic.h"
#include "modules.h"
#include "units.h"
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

/*
 * Floating point: by default every + - * / is rounded on its own, like Python.
 * Clang may otherwise fuse a * b + c into one instruction (FMA) where it wants,
 * so the same formula could give a different last digit in two loops or on two
 * machines.  --fma allows that fusion (a little faster on ARM).
 */
static int allow_fma = 0;

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
            "-Wno-implicit-const-int-float-conversion",   /* let x: float32 = 16777217 rounds, like NumPy */
            "-fno-math-errno",   /* Clair never reads errno: lets Clang vectorize sqrt() and friends */
            allow_fma ? "-ffp-contract=on" : "-ffp-contract=off",
            "-falign-loops=64",  /* hot loops start on a 64-byte line: same speed whatever the code layout */
            (char *)cpath,
            "-o",
            (char *)output,
            "-lm",
            "-pthread",          /* big matrix products and dense layers use every core */
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
    const char *input = NULL, *output = NULL;
    int unchecked = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) output = argv[++i];
        else if (strcmp(argv[i], "--sans-verif") == 0 || strcmp(argv[i], "--unchecked") == 0) unchecked = 1;
        else if (strcmp(argv[i], "--fma") == 0) allow_fma = 1;
        else if (!input && argv[i][0] != '-') input = argv[i];
        else { input = NULL; break; }
    }
    if (!input || !output) {
        fprintf(stderr, "usage: %s input.cl -o output [--sans-verif] [--fma]\n", argv[0]);
        return 2;
    }
    codegen_set_unchecked(unchecked);

    char *source = read_file(input);
    if (!source) {
        fprintf(stderr, "clair: cannot read %s\n", input);
        return 1;
    }

    Parser parser;
    ast_current_file = NULL;
    parser_init(&parser, source);
    AstNode *program = parser_parse_program(&parser);
    ast_current_line = 0;                 /* V0.49: nodes made by the compiler have no line */
    if (!program || parser.had_error) {
        ast_free(program);
        free(source);
        return 1;
    }

    /* V0.47: import math as m, import orbite (orbite.cl next to the program)... */
    if (!modules_resolve(program, input)) {
        ast_free(program);
        free(source);
        return 1;
    }

    if (!semantic_check(program)) {
        ast_free(program);
        free(source);
        return 1;
    }

    /* V0.48: float[m], float[m/s]... checked here, then forgotten */
    if (!units_check(program)) {
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
