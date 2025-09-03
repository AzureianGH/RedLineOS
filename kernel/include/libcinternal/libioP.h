#ifndef LIBIOP_H
#define LIBIOP_H

#include <stddef.h>
#include <libcinternal/_IO_STDIO_H.h>

#if (!defined _IO_USE_OLD_IO_FILE && (!defined _G_IO_NO_BACKWARD_COMPAT || _G_IO_NO_BACKWARD_COMPAT == 0))
#define _IO_JUMPS_OFFSET 1
#else
#define _IO_JUMPS_OFFSET 0
#endif

/* Type of MEMBER in struct type TYPE.  */
#define _IO_MEMBER_TYPE(TYPE, MEMBER) __typeof__(((TYPE){}).MEMBER)

/* Essentially ((TYPE *) THIS)->MEMBER, but avoiding the aliasing
   violation in case THIS has a different pointer type.  */
#define _IO_CAST_FIELD_ACCESS(THIS, TYPE, MEMBER) \
    (*(_IO_MEMBER_TYPE(TYPE, MEMBER) *)(((char *)(THIS)) + offsetof(TYPE, MEMBER)))

#define _IO_JUMPS(THIS) (THIS)->vtable
#define _IO_JUMPS_FILE_plus(THIS) \
    _IO_CAST_FIELD_ACCESS((THIS), struct _IO_FILE_plus, vtable)
#define _IO_WIDE_JUMPS(THIS) \
    _IO_CAST_FIELD_ACCESS((THIS), struct _IO_FILE, _wide_data)->_wide_vtable
#define _IO_CHECK_WIDE(THIS) \
    (_IO_CAST_FIELD_ACCESS((THIS), struct _IO_FILE, _wide_data) != NULL)

#if _IO_JUMPS_OFFSET
#define _IO_JUMPS_FUNC(THIS) \
    (*(struct _IO_jump_t **)((void *)&_IO_JUMPS_FILE_plus(THIS) + (THIS)->_vtable_offset))
#define _IO_vtable_offset(THIS) (THIS)->_vtable_offset
#else
#define _IO_JUMPS_FUNC(THIS) _IO_JUMPS_FILE_plus(THIS)
#define _IO_vtable_offset(THIS) 0
#endif
#define _IO_WIDE_JUMPS_FUNC(THIS) _IO_WIDE_JUMPS(THIS)
#define JUMP_FIELD(TYPE, NAME) TYPE NAME
#define JUMP0(FUNC, THIS) (_IO_JUMPS_FUNC(THIS)->FUNC)(THIS)
#define JUMP1(FUNC, THIS, X1) (_IO_JUMPS_FUNC(THIS)->FUNC)(THIS, X1)
#define JUMP2(FUNC, THIS, X1, X2) (_IO_JUMPS_FUNC(THIS)->FUNC)(THIS, X1, X2)
#define JUMP3(FUNC, THIS, X1, X2, X3) (_IO_JUMPS_FUNC(THIS)->FUNC)(THIS, X1, X2, X3)
#define JUMP_INIT(NAME, VALUE) VALUE
#define JUMP_INIT_DUMMY JUMP_INIT(dummy, 0), JUMP_INIT(dummy2, 0)

#define WJUMP0(FUNC, THIS) (_IO_WIDE_JUMPS_FUNC(THIS)->FUNC)(THIS)
#define WJUMP1(FUNC, THIS, X1) (_IO_WIDE_JUMPS_FUNC(THIS)->FUNC)(THIS, X1)
#define WJUMP2(FUNC, THIS, X1, X2) (_IO_WIDE_JUMPS_FUNC(THIS)->FUNC)(THIS, X1, X2)
#define WJUMP3(FUNC, THIS, X1, X2, X3) (_IO_WIDE_JUMPS_FUNC(THIS)->FUNC)(THIS, X1, X2, X3)

#define _IO_size_t size_t
#define _IO_ssize_t ptrdiff_t
#define _IO_off64_t long int

typedef void (*_IO_finish_t)(_IO_FILE *, int); /* finalize */
#define _IO_FINISH(FP) JUMP1(__finish, FP, 0)
#define _IO_WFINISH(FP) WJUMP1(__finish, FP, 0)

/* The 'overflow' hook flushes the buffer.
   The second argument is a character, or EOF.
   It matches the streambuf::overflow virtual function. */
typedef int (*_IO_overflow_t)(_IO_FILE *, int);
#define _IO_OVERFLOW(FP, CH) JUMP1(__overflow, FP, CH)
#define _IO_WOVERFLOW(FP, CH) WJUMP1(__overflow, FP, CH)

/* The 'underflow' hook tries to fills the get buffer.
   It returns the next character (as an unsigned char) or EOF.  The next
   character remains in the get buffer, and the get position is not changed.
   It matches the streambuf::underflow virtual function. */
typedef int (*_IO_underflow_t)(_IO_FILE *);
#define _IO_UNDERFLOW(FP) JUMP0(__underflow, FP)
#define _IO_WUNDERFLOW(FP) WJUMP0(__underflow, FP)

/* The 'uflow' hook returns the next character in the input stream
   (cast to unsigned char), and increments the read position;
   EOF is returned on failure.
   It matches the streambuf::uflow virtual function, which is not in the
   cfront implementation, but was added to C++ by the ANSI/ISO committee. */
#define _IO_UFLOW(FP) JUMP0(__uflow, FP)
#define _IO_WUFLOW(FP) WJUMP0(__uflow, FP)

/* The 'pbackfail' hook handles backing up.
   It matches the streambuf::pbackfail virtual function. */
typedef int (*_IO_pbackfail_t)(_IO_FILE *, int);
#define _IO_PBACKFAIL(FP, CH) JUMP1(__pbackfail, FP, CH)
#define _IO_WPBACKFAIL(FP, CH) WJUMP1(__pbackfail, FP, CH)

/* The 'xsputn' hook writes upto N characters from buffer DATA.
   Returns EOF or the number of character actually written.
   It matches the streambuf::xsputn virtual function. */
typedef _IO_size_t (*_IO_xsputn_t)(_IO_FILE *FP, const void *DATA,
                                   _IO_size_t N);
#define _IO_XSPUTN(FP, DATA, N) JUMP2(__xsputn, FP, DATA, N)
#define _IO_WXSPUTN(FP, DATA, N) WJUMP2(__xsputn, FP, DATA, N)

/* The 'xsgetn' hook reads upto N characters into buffer DATA.
   Returns the number of character actually read.
   It matches the streambuf::xsgetn virtual function. */
typedef _IO_size_t (*_IO_xsgetn_t)(_IO_FILE *FP, void *DATA, _IO_size_t N);
#define _IO_XSGETN(FP, DATA, N) JUMP2(__xsgetn, FP, DATA, N)
#define _IO_WXSGETN(FP, DATA, N) WJUMP2(__xsgetn, FP, DATA, N)

/* The 'seekoff' hook moves the stream position to a new position
   relative to the start of the file (if DIR==0), the current position
   (MODE==1), or the end of the file (MODE==2).
   It matches the streambuf::seekoff virtual function.
   It is also used for the ANSI fseek function. */
typedef _IO_off64_t (*_IO_seekoff_t)(_IO_FILE *FP, _IO_off64_t OFF, int DIR,
                                     int MODE);
#define _IO_SEEKOFF(FP, OFF, DIR, MODE) JUMP3(__seekoff, FP, OFF, DIR, MODE)
#define _IO_WSEEKOFF(FP, OFF, DIR, MODE) WJUMP3(__seekoff, FP, OFF, DIR, MODE)

/* The 'seekpos' hook also moves the stream position,
   but to an absolute position given by a fpos64_t (seekpos).
   It matches the streambuf::seekpos virtual function.
   It is also used for the ANSI fgetpos and fsetpos functions.  */
/* The _IO_seek_cur and _IO_seek_end options are not allowed. */
typedef _IO_off64_t (*_IO_seekpos_t)(_IO_FILE *, _IO_off64_t, int);
#define _IO_SEEKPOS(FP, POS, FLAGS) JUMP2(__seekpos, FP, POS, FLAGS)
#define _IO_WSEEKPOS(FP, POS, FLAGS) WJUMP2(__seekpos, FP, POS, FLAGS)

/* The 'setbuf' hook gives a buffer to the file.
   It matches the streambuf::setbuf virtual function. */
typedef _IO_FILE *(*_IO_setbuf_t)(_IO_FILE *, char *, _IO_ssize_t);
#define _IO_SETBUF(FP, BUFFER, LENGTH) JUMP2(__setbuf, FP, BUFFER, LENGTH)
#define _IO_WSETBUF(FP, BUFFER, LENGTH) WJUMP2(__setbuf, FP, BUFFER, LENGTH)

/* The 'sync' hook attempts to synchronize the internal data structures
   of the file with the external state.
   It matches the streambuf::sync virtual function. */
typedef int (*_IO_sync_t)(_IO_FILE *);
#define _IO_SYNC(FP) JUMP0(__sync, FP)
#define _IO_WSYNC(FP) WJUMP0(__sync, FP)

/* The 'doallocate' hook is used to tell the file to allocate a buffer.
   It matches the streambuf::doallocate virtual function, which is not
   in the ANSI/ISO C++ standard, but is part traditional implementations. */
typedef int (*_IO_doallocate_t)(_IO_FILE *);
#define _IO_DOALLOCATE(FP) JUMP0(__doallocate, FP)
#define _IO_WDOALLOCATE(FP) WJUMP0(__doallocate, FP)

/* The following four hooks (sysread, syswrite, sysclose, sysseek, and
   sysstat) are low-level hooks specific to this implementation.
   There is no correspondence in the ANSI/ISO C++ standard library.
   The hooks basically correspond to the Unix system functions
   (read, write, close, lseek, and stat) except that a _IO_FILE*
   parameter is used instead of an integer file descriptor;  the default
   implementation used for normal files just calls those functions.
   The advantage of overriding these functions instead of the higher-level
   ones (underflow, overflow etc) is that you can leave all the buffering
   higher-level functions.  */

/* The 'sysread' hook is used to read data from the external file into
   an existing buffer.  It generalizes the Unix read(2) function.
   It matches the streambuf::sys_read virtual function, which is
   specific to this implementation. */
typedef _IO_ssize_t (*_IO_read_t)(_IO_FILE *, void *, _IO_ssize_t);
#define _IO_SYSREAD(FP, DATA, LEN) JUMP2(__read, FP, DATA, LEN)
#define _IO_WSYSREAD(FP, DATA, LEN) WJUMP2(__read, FP, DATA, LEN)

/* The 'syswrite' hook is used to write data from an existing buffer
   to an external file.  It generalizes the Unix write(2) function.
   It matches the streambuf::sys_write virtual function, which is
   specific to this implementation. */
typedef _IO_ssize_t (*_IO_write_t)(_IO_FILE *, const void *, _IO_ssize_t);
#define _IO_SYSWRITE(FP, DATA, LEN) JUMP2(__write, FP, DATA, LEN)
#define _IO_WSYSWRITE(FP, DATA, LEN) WJUMP2(__write, FP, DATA, LEN)

/* The 'sysseek' hook is used to re-position an external file.
   It generalizes the Unix lseek(2) function.
   It matches the streambuf::sys_seek virtual function, which is
   specific to this implementation. */
typedef _IO_off64_t (*_IO_seek_t)(_IO_FILE *, _IO_off64_t, int);
#define _IO_SYSSEEK(FP, OFFSET, MODE) JUMP2(__seek, FP, OFFSET, MODE)
#define _IO_WSYSSEEK(FP, OFFSET, MODE) WJUMP2(__seek, FP, OFFSET, MODE)

/* The 'sysclose' hook is used to finalize (close, finish up) an
   external file.  It generalizes the Unix close(2) function.
   It matches the streambuf::sys_close virtual function, which is
   specific to this implementation. */
typedef int (*_IO_close_t)(_IO_FILE *); /* finalize */
#define _IO_SYSCLOSE(FP) JUMP0(__close, FP)
#define _IO_WSYSCLOSE(FP) WJUMP0(__close, FP)

/* The 'sysstat' hook is used to get information about an external file
   into a struct stat buffer.  It generalizes the Unix fstat(2) call.
   It matches the streambuf::sys_stat virtual function, which is
   specific to this implementation. */
typedef int (*_IO_stat_t)(_IO_FILE *, void *);
#define _IO_SYSSTAT(FP, BUF) JUMP1(__stat, FP, BUF)
#define _IO_WSYSSTAT(FP, BUF) WJUMP1(__stat, FP, BUF)

/* The 'showmany' hook can be used to get an image how much input is
   available.  In many cases the answer will be 0 which means unknown
   but some cases one can provide real information.  */
typedef int (*_IO_showmanyc_t)(_IO_FILE *);
#define _IO_SHOWMANYC(FP) JUMP0(__showmanyc, FP)
#define _IO_WSHOWMANYC(FP) WJUMP0(__showmanyc, FP)

/* The 'imbue' hook is used to get information about the currently
   installed locales.  */
typedef void (*_IO_imbue_t)(_IO_FILE *, void *);
#define _IO_IMBUE(FP, LOCALE) JUMP1(__imbue, FP, LOCALE)
#define _IO_WIMBUE(FP, LOCALE) WJUMP1(__imbue, FP, LOCALE)

#define _IO_CHAR_TYPE char /* unsigned char ? */
#define _IO_INT_TYPE int

struct _IO_jump_t
{
    JUMP_FIELD(size_t, __dummy);
    JUMP_FIELD(size_t, __dummy2);
    JUMP_FIELD(_IO_finish_t, __finish);
    JUMP_FIELD(_IO_overflow_t, __overflow);
    JUMP_FIELD(_IO_underflow_t, __underflow);
    JUMP_FIELD(_IO_underflow_t, __uflow);
    JUMP_FIELD(_IO_pbackfail_t, __pbackfail);
    /* showmany */
    JUMP_FIELD(_IO_xsputn_t, __xsputn);
    JUMP_FIELD(_IO_xsgetn_t, __xsgetn);
    JUMP_FIELD(_IO_seekoff_t, __seekoff);
    JUMP_FIELD(_IO_seekpos_t, __seekpos);
    JUMP_FIELD(_IO_setbuf_t, __setbuf);
    JUMP_FIELD(_IO_sync_t, __sync);
    JUMP_FIELD(_IO_doallocate_t, __doallocate);
    JUMP_FIELD(_IO_read_t, __read);
    JUMP_FIELD(_IO_write_t, __write);
    JUMP_FIELD(_IO_seek_t, __seek);
    JUMP_FIELD(_IO_close_t, __close);
    JUMP_FIELD(_IO_stat_t, __stat);
    JUMP_FIELD(_IO_showmanyc_t, __showmanyc);
    JUMP_FIELD(_IO_imbue_t, __imbue);
#if 0
    get_column;
    set_column;
#endif
};

extern struct _IO_FILE_plus _IO_2_1_stdout_;
extern struct _IO_FILE_plus _IO_2_1_stderr_;

#endif /* LIBIOP_H */