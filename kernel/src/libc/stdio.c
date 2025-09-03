#include <stdio.h>
#include <libcinternal/libioP.h>

#undef stdout
_IO_FILE *stdout = (FILE *) &_IO_2_1_stdout_;
#undef stderr
_IO_FILE *stderr = (FILE *) &_IO_2_1_stderr_;