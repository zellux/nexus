#ifndef _NEXUS_DEBUG_H
#define _NEXUS_DEBUG_H

/* Debug flags, debug information for specific module is on if and only if
 * DEBUG == 1 and DEBUG_MODULE == 1
 */
#define DEBUG         1
#define DEBUG_SCHED   0
#define DEBUG_SYSCALL 0
#define DEBUG_IPC     1
#define DEBUG_FS      1

#if DEBUG == 1
#define MAGIC_BREAK                                                 \
    do {                                                            \
        cprintf("Magic break at %s[%d]\n", __FUNCTION__, __LINE__); \
        __asm__("xchg %%bx, %%bx": :);                              \
    } while (0)
#define dprintk(_f, _a...) cprintf(_f, ## _a)
#define dprintfunc() dprintk("%s [%d]\n", __FUNCTION__, __LINE__)

#else

#define MAGIC_BREAK
#define dprintk(_f, _a...)
#define dprintfunc()
#endif


#endif /* _NEXUS_DEBUG_H */
