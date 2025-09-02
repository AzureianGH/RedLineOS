#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <extendedint.h>

void *malloc(size_t size);

void free(void *ptr);

void *realloc(void *ptr, size_t size);

void *calloc(size_t nmemb, size_t size);

#endif /* STDLIB_H */