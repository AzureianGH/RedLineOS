#include <stdio.h>
#include <libcinternal/libioP.h>

#undef stdout

_IO_FILE *stdout = (FILE *) &_IO_2_1_stdout_;