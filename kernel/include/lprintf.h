#ifndef LPRINTF_H
#define LPRINTF_H

#include <stdarg.h>
#include <stdbool.h>

int info_printf(const char *str, ...);
int error_printf(const char *str, ...);
int debug_printf(const char *str, ...);
int success_printf(const char *str, ...);
void set_debug_enabled(bool enabled);

// Dump the recent in-memory log ring to stdout for post-mortem debugging.
void log_dump_recent(void);

// Lightweight self-test for the log ring; returns 0 on pass, nonzero on failure.
int log_ring_self_test(void);

#endif /* LPRINTF_H */