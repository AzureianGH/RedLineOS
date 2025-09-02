#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

static bool debug_enabled = true;

void set_debug_enabled(bool enabled) {
    debug_enabled = enabled;
}

int info_printf(const char *str, ...) {
    va_list args;
    va_start(args, str);
    
    // Set color to gray with [INFO] then reset to default
    printf("\033[0;37m[INFO] \033[0m");
    
    int num_chars = vfprintf(stdout, str, args);

    va_end(args);
    return num_chars;
}

int error_printf(const char *str, ...) {
    va_list args;
    va_start(args, str);
    
    // Set color to red with [FAIL] then reset to default
    printf("\033[0;31m[FAIL] \033[0m");
    
    int num_chars = vfprintf(stdout, str, args);

    va_end(args);
    return num_chars;
}

int debug_printf(const char *str, ...) {
    if (!debug_enabled) return 0;
    va_list args;
    va_start(args, str);
    
    // Set color to yellow with [DEBG] then reset to default
    printf("\033[0;33m[DEBG] \033[0m");
    
    int num_chars = vfprintf(stdout, str, args);

    va_end(args);
    return num_chars;
}

int success_printf(const char *str, ...) {
    va_list args;
    va_start(args, str);
    
    // Set color to green with [OKAY] then reset to default
    printf("\033[0;32m[OKAY] \033[0m");
    
    int num_chars = vfprintf(stdout, str, args);

    va_end(args);
    return num_chars;
}