#include <sys/cdefs.h>
#include <lprintf.h>


void __chk_fail()
{
   error_printf("Buffer overflow detected in kernel process!\n");
   asm volatile ("cli");
   for (;;)
     {
       asm volatile ("hlt");
     }
}