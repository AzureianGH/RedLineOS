#ifndef LPRINTF_H
#define LPRINTF_H

#include <stdarg.h>

int info_printf(const char *str, ...);
int error_printf(const char *str, ...);
int debug_printf(const char *str, ...);
int success_printf(const char *str, ...);
void set_debug_enabled(bool enabled);

#endif /* LPRINTF_H */