#include <libcinternal/libioP.h>
#include <displaystandard.h>
#include <lock.h>
#include <stdio.h>
#include <lprintf.h>

// __write for stdout
static _IO_ssize_t stdout_write(_IO_FILE *file, const void *buf, _IO_ssize_t len) {
    (void)file; // Unused parameter
    const char *cbuf = (const char *)buf;
    for (_IO_ssize_t i = 0; i < len; i++) {
        // Output each character using the displaystandard_putc function.
        displaystandard_putc(cbuf[i]);
    }
    return len; // Return number of bytes written.
}

static _IO_ssize_t stderr_write(_IO_FILE *file, const void *buf, _IO_ssize_t len) {
    (void)file; // Unused parameter
    const char *cbuf = (const char *)buf;
    error_printf("");
    for (_IO_ssize_t i = 0; i < len; i++) {
        // Output each character using the displaystandard_putc function.
        displaystandard_putc(cbuf[i]);
    }
    return len; // Return number of bytes written.
}

static void dummy_finish(_IO_FILE *file, int flag) {
    (void)file;
    (void)flag;
    // No special finalization needed for stdout.
}

// File descriptor for STDOUT.
struct _IO_jump_t _IO_stdout = {
    .__write = stdout_write,
    .__finish = dummy_finish
};

struct _IO_jump_t _IO_stderr = {
    .__write = stderr_write,
    .__finish = dummy_finish
};

static _IO_lock_t stdout_lock;
static _IO_lock_t stderr_lock;

struct _IO_FILE_plus _IO_2_1_stdout_ = {
    .file = {
        ._flags = 0x00000001, // _IO_NO_WRITES
        ._fileno = 1, // File descriptor for stdout
        ._lock = &stdout_lock,
        ._vtable_offset = 0,
    },
    .vtable = &_IO_stdout
};

struct _IO_FILE_plus _IO_2_1_stderr_ = {
    .file = {
        ._flags = 0x00000001, // _IO_NO_WRITES
        ._fileno = 2, // File descriptor for stderr
        ._lock = &stderr_lock,
        ._vtable_offset = 0,
    },
    .vtable = &_IO_stderr
};

void flockfile(FILE *stream) {
    if (!stream) return;
    struct _IO_FILE_plus* fp = (struct _IO_FILE_plus*)stream;
    if (!fp->file._lock) return;
    spin_lock(fp->file._lock);
}

void funlockfile(FILE *stream) {
    if (!stream) return;
    struct _IO_FILE_plus* fp = (struct _IO_FILE_plus*)stream;
    if (!fp->file._lock) return;
    spin_unlock(fp->file._lock);
}

int ftrylockfile(FILE *stream) {
    if (!stream) return 0;
    struct _IO_FILE_plus* fp = (struct _IO_FILE_plus*)stream;
    if (!fp->file._lock) return 1; // treat as unlocked if no lock present
    return spin_trylock(fp->file._lock);
}

int fischlocked(FILE *stream) {
    if (!stream) return 0;
    struct _IO_FILE_plus* fp = (struct _IO_FILE_plus*)stream;
    if (!fp->file._lock) return 0;
    // If value is 1, someone holds it. This is a best-effort check.
    return __atomic_load_n(&fp->file._lock->v, __ATOMIC_RELAXED) != 0;
}