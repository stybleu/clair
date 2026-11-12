#ifndef CLAIR_TEXT_METHODS_H
#define CLAIR_TEXT_METHODS_H
/*
 * V0.72: the methods of a text, as in Python: s.lower(), s.split(","), ", ".join(xs)...
 *
 * The semantic pass turns s.name(args) into the call @tx_name(s, args) and checks
 * the arguments with this table; code generation writes clair_tx_name(s, args...)
 * with the missing arguments taken from defaults, and the line when the function
 * can stop the program.  The functions are in src/clair_text_rt.c.
 */
#include <stddef.h>
#include <string.h>

enum {
    TX_TABLES = 1,      /* needs the Unicode tables (lower, upper, isalpha...) */
    TX_LIST = 2,        /* makes or reads a list[string] */
    TX_LINE = 4         /* can stop the program: the line is given last */
};

typedef struct {
    const char *name;
    int min, max;                 /* arguments after the text itself */
    const char *args[3];          /* their types */
    const char *defaults[3];      /* C text of a missing one */
    const char *result;
    unsigned flags;
} TextMethod;

static const TextMethod text_methods[] = {
    { "lower", 0, 0, { 0 }, { 0 }, "string", TX_TABLES },
    { "upper", 0, 0, { 0 }, { 0 }, "string", TX_TABLES },
    { "swapcase", 0, 0, { 0 }, { 0 }, "string", TX_TABLES },
    { "capitalize", 0, 0, { 0 }, { 0 }, "string", TX_TABLES },
    { "title", 0, 0, { 0 }, { 0 }, "string", TX_TABLES },
    { "strip", 0, 1, { "string" }, { "NULL" }, "string", 0 },
    { "lstrip", 0, 1, { "string" }, { "NULL" }, "string", 0 },
    { "rstrip", 0, 1, { "string" }, { "NULL" }, "string", 0 },
    { "split", 0, 2, { "string", "int" }, { "NULL", "-1" }, "list[string]", TX_LIST | TX_LINE },
    { "rsplit", 0, 2, { "string", "int" }, { "NULL", "-1" }, "list[string]", TX_LIST | TX_LINE },
    { "splitlines", 0, 1, { "bool" }, { "false" }, "list[string]", TX_LIST },
    { "join", 1, 1, { "list[string]" }, { 0 }, "string", TX_LIST },
    { "replace", 2, 3, { "string", "string", "int" }, { 0, 0, "-1" }, "string", 0 },
    { "startswith", 1, 3, { "string", "int", "int" }, { 0, "0", "INT64_MAX" }, "bool", 0 },
    { "endswith", 1, 3, { "string", "int", "int" }, { 0, "0", "INT64_MAX" }, "bool", 0 },
    { "find", 1, 3, { "string", "int", "int" }, { 0, "0", "INT64_MAX" }, "int", 0 },
    { "rfind", 1, 3, { "string", "int", "int" }, { 0, "0", "INT64_MAX" }, "int", 0 },
    { "index", 1, 3, { "string", "int", "int" }, { 0, "0", "INT64_MAX" }, "int", TX_LINE },
    { "rindex", 1, 3, { "string", "int", "int" }, { 0, "0", "INT64_MAX" }, "int", TX_LINE },
    { "count", 1, 3, { "string", "int", "int" }, { 0, "0", "INT64_MAX" }, "int", 0 },
    { "partition", 1, 1, { "string" }, { 0 }, "tuple[string,string,string]", TX_LINE },   /* V0.76 */
    { "rpartition", 1, 1, { "string" }, { 0 }, "tuple[string,string,string]", TX_LINE },
    { "removeprefix", 1, 1, { "string" }, { 0 }, "string", 0 },
    { "removesuffix", 1, 1, { "string" }, { 0 }, "string", 0 },
    { "ljust", 1, 2, { "int", "string" }, { 0, "\" \"" }, "string", TX_LINE },
    { "rjust", 1, 2, { "int", "string" }, { 0, "\" \"" }, "string", TX_LINE },
    { "center", 1, 2, { "int", "string" }, { 0, "\" \"" }, "string", TX_LINE },
    { "zfill", 1, 1, { "int" }, { 0 }, "string", 0 },
    { "isalpha", 0, 0, { 0 }, { 0 }, "bool", TX_TABLES },
    { "isalnum", 0, 0, { 0 }, { 0 }, "bool", TX_TABLES },
    { "isdecimal", 0, 0, { 0 }, { 0 }, "bool", TX_TABLES },
    { "isdigit", 0, 0, { 0 }, { 0 }, "bool", TX_TABLES },
    { "isnumeric", 0, 0, { 0 }, { 0 }, "bool", TX_TABLES },
    { "isspace", 0, 0, { 0 }, { 0 }, "bool", 0 },
    { "isascii", 0, 0, { 0 }, { 0 }, "bool", 0 },
    { "islower", 0, 0, { 0 }, { 0 }, "bool", TX_TABLES },
    { "isupper", 0, 0, { 0 }, { 0 }, "bool", TX_TABLES },
    { "istitle", 0, 0, { 0 }, { 0 }, "bool", TX_TABLES },
    { NULL, 0, 0, { 0 }, { 0 }, NULL, 0 }
};

/* the method of that name (s.name(...)), or NULL */
static inline const TextMethod *text_method(const char *name) {
    for (size_t i = 0; name && text_methods[i].name; i++)
        if (strcmp(text_methods[i].name, name) == 0) return &text_methods[i];
    return NULL;
}

/* @tx_lower -> its method; other calls (and @tx_at, @tx_len, @tx_repeat) -> NULL */
static inline const TextMethod *text_method_call(const char *callee) {
    return callee && strncmp(callee, "@tx_", 4) == 0 ? text_method(callee + 4) : NULL;
}

#endif
