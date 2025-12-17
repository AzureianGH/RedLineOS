#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <spinlock.h>

#define LOG_RING_SIZE 128
#define LOG_MSG_MAX   256

static bool debug_enabled = true;
static char log_ring[LOG_RING_SIZE][LOG_MSG_MAX];
static size_t log_ring_head;
static bool log_ring_full;
static spinlock_t log_lock = {0};

static void log_ring_reset(void) {
    memset(log_ring, 0, sizeof(log_ring));
    log_ring_head = 0;
    log_ring_full = false;
}

static size_t log_ring_count(void) {
    return log_ring_full ? LOG_RING_SIZE : log_ring_head;
}

static void log_ring_append_raw(const char *msg) {
    strncpy(log_ring[log_ring_head], msg, LOG_MSG_MAX - 1);
    log_ring[log_ring_head][LOG_MSG_MAX - 1] = '\0';
    log_ring_head = (log_ring_head + 1) % LOG_RING_SIZE;
    if (log_ring_head == 0) {
        log_ring_full = true;
    }
}

static void log_store(const char *level, const char *fmt, va_list args) {
    char formatted[LOG_MSG_MAX];
    int n = vsnprintf(formatted, sizeof(formatted), fmt, args);
    if (n < 0) return;

    // Prefix with level; truncate if needed.
    snprintf(log_ring[log_ring_head], LOG_MSG_MAX, "[%s] %s", level, formatted);
    log_ring_head = (log_ring_head + 1) % LOG_RING_SIZE;
    if (log_ring_head == 0) {
        log_ring_full = true;
    }
}

static int log_emit(const char *level, const char *color_prefix, const char *fmt, va_list args) {
    spin_lock(&log_lock);
    va_list copy;
    va_copy(copy, args);
    log_store(level, fmt, copy);
    va_end(copy);

    printf("%s", color_prefix);
    int num_chars = vfprintf(stdout, fmt, args);
    spin_unlock(&log_lock);
    return num_chars;
}

void set_debug_enabled(bool enabled) {
    debug_enabled = enabled;
}

int info_printf(const char *str, ...) {
    va_list args;
    va_start(args, str);

    // Set color to gray with [INFO] then reset to default
    int num_chars = log_emit("INFO", "\033[0;37m[INFO] \033[0m", str, args);
    va_end(args);
    return num_chars;
}

int error_printf(const char *str, ...) {
    va_list args;
    va_start(args, str);

    // Set color to red with [FAIL] then reset to default
    int num_chars = log_emit("FAIL", "\033[0;31m[FAIL] \033[0m", str, args);
    va_end(args);
    return num_chars;
}

int debug_printf(const char *str, ...) {
    if (!debug_enabled) return 0;
    va_list args;
    va_start(args, str);

    // Set color to yellow with [DEBG] then reset to default
    int num_chars = log_emit("DEBG", "\033[0;33m[DEBG] \033[0m", str, args);
    va_end(args);
    return num_chars;
}

int success_printf(const char *str, ...) {
    va_list args;
    va_start(args, str);

    // Set color to green with [OKAY] then reset to default
    int num_chars = log_emit("OKAY", "\033[0;32m[OKAY] \033[0m", str, args);
    va_end(args);
    return num_chars;
}

void log_dump_recent(void) {
    size_t count = log_ring_full ? LOG_RING_SIZE : log_ring_head;
    size_t start = log_ring_full ? log_ring_head : 0;

    printf("\n-- Recent log ring (%zu entries) --\n", count);
    for (size_t i = 0; i < count; ++i) {
        size_t idx = (start + i) % LOG_RING_SIZE;
        printf("%s\n", log_ring[idx]);
    }
    printf("-- End log ring --\n\n");
}

int log_ring_self_test(void) {
    log_ring_reset();

    // Fill beyond ring size to force wrap.
    for (size_t i = 0; i < LOG_RING_SIZE + 5; ++i) {
        char msg[32];
        snprintf(msg, sizeof(msg), "test-%03zu", i);
        char entry[LOG_MSG_MAX];
        snprintf(entry, sizeof(entry), "[TST] %s", msg);
        log_ring_append_raw(entry);
    }

    size_t count = log_ring_count();
    if (count != LOG_RING_SIZE) {
        return -1;
    }

    // Verify the last entry matches the final test message.
    const char *expected_tail = "[TST] test-132"; // LOG_RING_SIZE + 5 -> last index 132
    size_t tail_idx = log_ring_head ? log_ring_head - 1 : LOG_RING_SIZE - 1;
    if (strncmp(log_ring[tail_idx], expected_tail, strlen(expected_tail)) != 0) {
        return -2;
    }

    // Simple integrity check: ensure head advanced after wrap.
    if (!log_ring_full) {
        return -3;
    }

    log_ring_reset();
    return 0;
}