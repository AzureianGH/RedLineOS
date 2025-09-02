#include <stdarg.h>
#include <stdio.h>
#include <libcinternal/libioP.h>


int vfprintf(FILE *stream, const char *format, va_list args) {
    if (stream == NULL) {
        return -1;
    }

    struct _IO_FILE_plus* fpstream = (struct _IO_FILE_plus*) stream; // Assume stream is actually _IO_FILE_plus

    char buf[1024];
    int len = vsnprintf(buf, sizeof(buf), format, args);
    if (len < 0) return -1;
    if (len >= (int)sizeof(buf)) len = sizeof(buf) - 1;
    buf[len] = '\0';

    if (fpstream->vtable && fpstream->vtable->__write) {
        int we_locked = 0;
        if (fpstream->file._lock) {
            we_locked = ftrylockfile(stream);
        }
        int written = fpstream->vtable->__write(stream, buf, len);
        if (we_locked) {
            funlockfile(stream);
        }
        return written;
    }

    return -1;
}