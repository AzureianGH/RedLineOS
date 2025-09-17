#include <stdio.h>

int snprintf(char * restrict buffer, size_t bufsz, char const * restrict format, ...) {
    if (bufsz == 0) return 0; // Nothing can be written
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buffer, bufsz, format, args);
    va_end(args);
    return ret;
}