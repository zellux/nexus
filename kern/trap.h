/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_TRAP_H
#define JOS_KERN_TRAP_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/trap.h>
#include <inc/mmu.h>

/* The kernel's interrupt descriptor table */
extern struct Gatedesc idt[];

void idt_init(void);
void msr_init(void);
void print_regs(struct PushRegs *regs);
void print_trapframe(struct Trapframe *tf);
void page_fault_handler(struct Trapframe *);
void backtrace(struct Trapframe *);

#define rdmsr(msr,val1,val2)                        \
    __asm__ __volatile__("rdmsr"                    \
                         : "=a" (val1), "=d" (val2) \
                         : "c" (msr))

#define wrmsr(msr,val1,val2)                                    \
    __asm__ __volatile__("wrmsr"                                \
                         : /* no outputs */                     \
                         : "c" (msr), "a" (val1), "d" (val2))


#endif /* JOS_KERN_TRAP_H */
