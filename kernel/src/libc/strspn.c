#include <string.h>

static int in_set(unsigned char c, const char *set) {
    for (const unsigned char *p = (const unsigned char *)set; *p; ++p) {
        if (*p == c) return 1;
    }
    return 0;
}

size_t strspn(const char *s, const char *accept) {
    size_t i = 0;
    for (; s[i]; ++i) {
        if (!in_set((unsigned char)s[i], accept)) break;
    }
    return i;
}
