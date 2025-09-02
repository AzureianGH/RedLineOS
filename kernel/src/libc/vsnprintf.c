#include <stdarg.h>
#include <stdio.h>
#include <libcinternal/libioP.h>


extern int npf_vsnprintf(char * restrict buffer,
                  size_t bufsz,
                  char const * restrict format,
                  va_list vlist);


int vsnprintf(char * restrict buffer, size_t bufsz, char const * restrict format, va_list vlist)
{
    return npf_vsnprintf(buffer, bufsz, format, vlist); // Cheesy way to avoid including the full vsnprintf implementation here
}