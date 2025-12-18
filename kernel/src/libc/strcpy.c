#include <string.h>

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++) != '\0') {
        ;
    }
    return dest;
}
