#ifndef CD_FREESTANDING_STDIO_H
#define CD_FREESTANDING_STDIO_H
#include <stddef.h>
typedef struct CD_FILE FILE;
#define NULL ((void*)0)
int printf(const char* fmt, ...);
int snprintf(char* buf, size_t n, const char* fmt, ...);
int sprintf(char* buf, const char* fmt, ...);
#endif
