#include <stdarg.h>
#include <stdio.h>
#include <libcinternal/libioP.h>


extern int npf_vsnprintf(char * restrict buffer,
                  size_t bufsz,
                  char const * restrict format,
                  va_list vlist);


int fprintf(FILE *stream, const char *format, ...) {
    // Simple implementation that only supports stdout for now.
    if (stream == NULL) {
        return -1;
    }

    if (stream == stdout)
    {
        va_list args;
        va_start(args, format);
        int num_chars = vfprintf(stream, format, args);
        va_end(args);
        return num_chars;
    }
    else
    {
        // Unsupported stream
        return -1;
    }
    return 0;
}