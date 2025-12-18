#include <string.h>

static int in_set_c(unsigned char c, const char *set) {
    for (const unsigned char *p = (const unsigned char *)set; *p; ++p) {
        if (*p == c) return 1;
    }
    return 0;
}

size_t strcspn(const char *s, const char *reject) {
    size_t i = 0;
    for (; s[i]; ++i) {
        if (in_set_c((unsigned char)s[i], reject)) break;
    }
    return i;
}
