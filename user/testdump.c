#include <inc/lib.h>

void
umain(void)
{
    sys_dump_env();
    sys_dump_env();
    cprintf("hello");
    sys_dump_env();
}

