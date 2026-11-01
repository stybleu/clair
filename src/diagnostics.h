#ifndef CLAIR_DIAGNOSTICS_H
#define CLAIR_DIAGNOSTICS_H
#include <stddef.h>
void clair_diag_init(const char *, const char *, int);
void clair_diag_show(char, const char *, size_t, size_t, const char *);
#endif
