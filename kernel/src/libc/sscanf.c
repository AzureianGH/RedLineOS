
#include <stdio.h>
#include <stdarg.h>
#include <string.h>


int sscanf(const char* str, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vsscanf(str, format, args);
    va_end(args);
    return ret;
}