#ifndef CLAIR_V63_STDLIB
#define CLAIR_V63_STDLIB
void *malloc(unsigned long);
void *calloc(unsigned long, unsigned long);
void free(void *);
void exit(int);
#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0
#endif
