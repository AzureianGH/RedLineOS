#include <stddef.h>

char *strncat(char *dest, const char *src, size_t n) {
    if (!dest || !src) return dest;
    char *d = dest;
    while (*d) ++d;
    size_t i = 0;
    for (; i < n && src[i]; ++i) *d++ = src[i];
    *d = '\0';
    return dest;
}
