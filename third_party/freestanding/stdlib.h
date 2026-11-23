#ifndef CD_FREESTANDING_STDLIB_H
#define CD_FREESTANDING_STDLIB_H
#include <stddef.h>
void* malloc(size_t n);
void* calloc(size_t n, size_t sz);
void* realloc(void* p, size_t n);
void free(void* p);
void exit(int code) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));
static inline int abs(int v) { return v < 0 ? -v : v; }
static inline long labs(long v) { return v < 0 ? -v : v; }
#endif
