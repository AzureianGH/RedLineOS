#define NANOPRINTF_IMPLEMENTATION
#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS 1 // char, short
#define NANOPRINTF_USE_ALT_FORM_FLAG 1
#include <nanoprintf.h>
#include <stddef.h>
#include <typewriter.h>
#include <stdarg.h>
#include <displaystandard.h>
#include <stdio.h>



int printf(const char *str, ...) {
    va_list args;
    va_start(args, str);

    int num_chars = vfprintf(stdout, str, args);

    va_end(args);
    return num_chars;
}

