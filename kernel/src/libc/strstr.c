#include <string.h>

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (const char *h = haystack; *h; ++h) {
        const char *p = h;
        const char *n = needle;
        while (*p && *n && (*p == *n)) {
            ++p; ++n;
        }
        if (*n == '\0') return (char *)h;
    }
    return NULL;
}
