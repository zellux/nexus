#ifndef JOS_KERN_KDEBUG_H
#define JOS_KERN_KDEBUG_H

#include <inc/types.h>
#include <inc/memlayout.h>

#define DEBUG 0

#if DEBUG == 1
#define MAGIC_BREAK                                                 \
    do {                                                            \
        cprintf("Magic break at %s[%d]\n", __FUNCTION__, __LINE__); \
        __asm__("xchg %%bx, %%bx": :);                              \
    } while (0)
#else
#define MAGIC_BREAK
#endif

struct Trapframe;

// Debug information about a particular instruction pointer
struct Eipdebuginfo {
	const char *eip_file;		// Source code filename for EIP
	int         eip_line;		// Source code linenumber for EIP

	const char *eip_fn_name;	// Name of function containing EIP
					//  - Note: not null terminated!
	int         eip_fn_namelen;	// Length of function name
	uintptr_t   eip_fn_addr;	// Address of start of function
	int         eip_fn_narg;	// Number of function arguments
};

extern int debuginfo_eip(uintptr_t eip, struct Eipdebuginfo *info);
extern void dump_tf(struct Trapframe *tf);
extern void dump_va_mapping(pde_t *pgdir, uintptr_t va);

#if DEBUG == 1
#define dprintk(_f, _a...) cprintf(_f, ## _a)
#else
#define dprintk(_f, _a...)
#endif

#define dprintfunc() dprintk("%s [%d]\n", __FUNCTION__, __LINE__)

#endif
