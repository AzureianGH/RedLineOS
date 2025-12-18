#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <extendedint.h>
#include <misc/sys/cdefs.h>


void *malloc(size_t size);

void free(void *ptr);

void *realloc(void *ptr, size_t size);

void *calloc(size_t nmemb, size_t size);

extern char **environ;

long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
long long strtoll(const char *nptr, char **endptr, int base);
unsigned long long strtoull(const char *nptr, char **endptr, int base);

int atoi(const char *nptr);
long atol(const char *nptr);
long long atoll(const char *nptr);

int abs(int j);
long labs(long j);

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

extern int setenv (const char *__name, const char *__value, int __replace)
     __THROW __nonnull ((2));

extern char *getenv (const char *__name) __THROW __nonnull ((1)) __wur;

extern int unsetenv (const char *__name) __THROW __nonnull ((1));

extern int putenv (char *__string) __THROW __nonnull ((1));

#endif /* STDLIB_H */