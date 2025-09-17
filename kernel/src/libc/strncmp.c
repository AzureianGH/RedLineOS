#include <string.h>

int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        unsigned char c1 = (unsigned char)s1[i];
        unsigned char c2 = (unsigned char)s2[i];
        if (c1 != c2) {
            return c1 - c2;
        }
        if (c1 == '\0') {
            return 0;
        }
    }
    return 0;
}