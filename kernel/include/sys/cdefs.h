#ifndef _SYS_CDEFS_H

#include <misc/sys/cdefs.h>

#ifndef _ISOMAC
/* The compiler will optimize based on the knowledge the parameter is
   not NULL.  This will omit tests.  A robust implementation cannot allow
   this so when compiling glibc itself we ignore this attribute.  */
# undef __nonnull
# define __nonnull(params)

void __chk_fail (void);

#endif

#endif