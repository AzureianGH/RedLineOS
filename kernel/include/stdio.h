#ifndef STDIO_H
#define STDIO_H

#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <extendedint.h>


typedef struct _IO_FILE FILE;

extern struct _IO_FILE *stdout;
extern struct _IO_FILE *stderr;

#define stdout stdout
#define stderr stderr

int printf(const char *str, ...);

int fprintf(FILE *stream, const char *format, ...);

int vfprintf(FILE *stream, const char *format, va_list args);

int vsnprintf(char * restrict buffer, size_t bufsz, char const * restrict format, va_list vlist);

// Simple FILE locking API (freestanding subset)
void flockfile(FILE *stream);
void funlockfile(FILE *stream);
int ftrylockfile(FILE *stream); // returns 0 on failure, non-zero on success
int fischlocked(FILE *stream);  // non-zero if locked; best-effort (no ownership tracking)


#endif /* STDIO_H */