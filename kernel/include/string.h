#ifndef STRING_H
#define STRING_H

#include <stddef.h>
#include <stdbool.h>
#include <extendedint.h>

void *memcpy(void *restrict dest, const void *restrict src, size_t n);

void *memset(void *s, int c, size_t n);

void *memmove(void *dest, const void *src, size_t n);

int memcmp(const void *s1, const void *s2, size_t n);

void *memchr(const void *s, int c, size_t n);

size_t strlen(const char *s);

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);

char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);

char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);

size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
char *strpbrk(const char *s, const char *accept);
char *strstr(const char *haystack, const char *needle);

char *strdup(const char *s);
char *strndup(const char *s, size_t n);

// GNU extension; declare alongside string helpers
void *memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);

#endif /* STRING_H */