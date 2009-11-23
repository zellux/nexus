/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>
#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* #define GATE_ENTRY(n,entry,type)                \ */
/*     .data 2;                                    \ */
/*     .long entry;                                \ */
/*     .word n;                                    \ */
/*     .word type;                                 \ */
/*     .text */

/* #define EXCEPTION(n,name)                           \ */
/*     GATE_ENTRY(n,name,ACC_PL_K | ACC_TRAP_GATE);    \ */
/* name:;                                              \ */
/*     pushl$(0);                                      \ */
/*     pushl$(n);                                      \ */
/*     jmp _alltraps */

/* The TRAPHANDLER macro defines a globally-visible function for handling
 * a trap.  It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 */ 
#define TRAPHANDLER(name, num)                                \
    .globl name;        /* define global symbol for 'name' */ \
    .type  name, @function;    /* symbol type is function */  \
    .align 2;        /* align function definition */          \
name:            /* function starts here */                   \
 pushl $(num);                                                \
 jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)               \
    .globl name;                                  \
    .type name, @function;                        \
    .align 2;                                     \
name:                                             \
 pushl $0;                                        \
 pushl $(num);                                    \
 jmp _alltraps

.text

 /*
  * Lab 3: Your code here for generating entry points for the different traps.
  */
TRAPHANDLER_NOEC(trap_divide, 0);
TRAPHANDLER(trap_debug, 1);
TRAPHANDLER(trap_nmi, 2);
TRAPHANDLER_NOEC(trap_brkpt, 3);
TRAPHANDLER(trap_oflow, 4);
TRAPHANDLER(trap_bound, 5);
TRAPHANDLER(trap_illop, 6);
TRAPHANDLER(trap_device, 7);
TRAPHANDLER(trap_dblflt, 8);
TRAPHANDLER(trap_coproc, 9);
TRAPHANDLER(trap_tss, 10);
TRAPHANDLER(trap_segnp, 11);
TRAPHANDLER(trap_stack, 12);
TRAPHANDLER(trap_gpflt, 13);
TRAPHANDLER(trap_pgflt, 14);
TRAPHANDLER(trap_res, 15);
TRAPHANDLER(trap_fperr, 16);
TRAPHANDLER(trap_align, 17);
TRAPHANDLER(trap_mchk, 18);
TRAPHANDLER(trap_simderr, 19);

TRAPHANDLER_NOEC(trap_syscall, 48);

 /*
  * Lab 3: Your code here for _alltraps
  */
_alltraps:
        pushl %ds
        pushl %es
        pusha

        movl $GD_KD, %eax
        movl %eax, %ds
        movl %eax, %es
        pushl %esp
        
        call trap

        popl %esp
        popa
        popl %es
        popl %ds
        subl $8, %esp
        iret

.globl sysenter_handler;        /* define global symbol for 'name' */ \
.type  sysenter_handler, @function;    /* symbol type is function */  \
.align 2;        /* align function definition */          \
sysenter_handler:
//		pushl %esi /* return pc */
//		pushl %ebp /* return esp */

		movl %ebp, %ecx
		movl %esi, %edx
		
		pushl %ss
		pushl %ebp
		pushf
		pushl %cs
		pushl %esi
		pushl $0
		pushl $48
        pushl %ds
        pushl %es
		pusha
//        pusha
        pushl %esp

		call do_sysenter

		popl %ecx
		popl %edx

		sysexit

