#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/sched.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/kdebug.h>

static struct Taskstate ts;

void tf_handler_default(struct Trapframe *);
void tf_handler_brkpt(struct Trapframe *);
void irq_handler_clock(struct Trapframe *);

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t) idt
};

/* traphandler_t idt_handlers[256] = { [0 ... 255] = tf_handler_default }; */
traphandler_t idt_handlers[256];

static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < sizeof(excnames)/sizeof(excnames[0]))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
		return "Hardware Interrupt";
	return "(unknown trap)";
}


void
idt_init(void)
{
	extern struct Segdesc gdt[];
    int i;
	
	// LAB 3: Your code here.
    extern void trap_divide();
    extern void trap_syscall();
    extern void trap_gpflt();
    extern void trap_pgflt();
    extern void trap_brkpt();
    extern void irq_clock();

    SETGATE(idt[0], 0, GD_KT, trap_divide, 3);
    SETGATE(idt[3], 1, GD_KT, trap_brkpt, 3);
    SETGATE(idt[13], 0, GD_KT, trap_gpflt, 0);
    SETGATE(idt[14], 0, GD_KT, trap_pgflt, 0);
    SETGATE(idt[32], 0, GD_KT, irq_clock, 0);
    SETGATE(idt[48], 1, GD_KT, trap_syscall, 3);

    for (i = 0; i < 256; i++) {
        idt_handlers[i] = tf_handler_default;
    }
    idt_handlers[3] = tf_handler_brkpt;
    idt_handlers[14] = page_fault_handler;
    idt_handlers[32] = irq_handler_clock;
    
	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	ts.ts_esp0 = KSTACKTOP;
	ts.ts_ss0 = GD_KD;

	// Initialize the TSS field of the gdt.
	gdt[GD_TSS >> 3] = SEG16(STS_T32A, (uint32_t) (&ts),
					sizeof(struct Taskstate), 0);
	gdt[GD_TSS >> 3].sd_s = 0;

	// Load the TSS
	ltr(GD_TSS);

	// Load the IDT
	asm volatile("lidt idt_pd");
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p\n", tf);
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	cprintf("  err  0x%08x\n", tf->tf_err);
	cprintf("  eip  0x%08x\n", tf->tf_eip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	cprintf("  esp  0x%08x\n", tf->tf_esp);
	cprintf("  ss   0x----%04x\n", tf->tf_ss);
}

void
print_utrapframe(struct UTrapframe *tf)
{
	cprintf("UTrapframe at %p\n", tf);
	cprintf("  fva  0x%08x\n", tf->utf_fault_va);
	cprintf("  err  0x%08x\n", tf->utf_err);
	print_regs(&tf->utf_regs);
	cprintf("  eip  0x%08x\n", tf->utf_eip);
	cprintf("  flag 0x%08x\n", tf->utf_eflags);
	cprintf("  esp  0x%08x\n", tf->utf_esp);
}

void
print_regs(struct PushRegs *regs)
{
	cprintf("  edi  0x%08x\n", regs->reg_edi);
	cprintf("  esi  0x%08x\n", regs->reg_esi);
	cprintf("  ebp  0x%08x\n", regs->reg_ebp);
	cprintf("  oesp 0x%08x\n", regs->reg_oesp);
	cprintf("  ebx  0x%08x\n", regs->reg_ebx);
	cprintf("  edx  0x%08x\n", regs->reg_edx);
	cprintf("  ecx  0x%08x\n", regs->reg_ecx);
	cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	
	// Handle clock interrupts.
	// LAB 4: Your code here.

	// Handle spurious interupts
	// The hardware sometimes raises these because of noise on the
	// IRQ line or other reasons. We don't care.
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_SPURIOUS) {
		cprintf("Spurious interrupt on irq 7\n");
		print_trapframe(tf);
		return;
	}

    /* print_trapframe(tf); */

	// Unexpected trap: The user process or the kernel has a bug.
	/* print_trapframe(tf); */

    if (tf->tf_trapno >= 256) {
		env_destroy(curenv);
		return;
    }
    /* dprintk("trap_dispatch: handler=%p\n", idt_handlers[tf->tf_trapno]); */
    idt_handlers[tf->tf_trapno](tf);
}

void
trap(struct Trapframe *tf)
{
	/* cprintf("Incoming TRAP frame(%s) at %p\n", trapname(tf->tf_trapno), tf); */

	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		assert(curenv);
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf;
	}
	
	// Dispatch based on what type of trap occurred
	trap_dispatch(tf);

	// If we made it to this point, then no other environment was
	// scheduled, so we should return to the current environment
	// if doing so makes sense.
	if (curenv && curenv->env_status == ENV_RUNNABLE)
		env_run(curenv);
	else
		sched_yield();
}


void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	cprintf("[%08x] user fault va %08x ip %08x\n",
            curenv->env_id, fault_va, tf->tf_eip);
    /* print_trapframe(tf); */

	// Handle kernel-mode page faults.
    if (tf->tf_cs == GD_KT) {
        dump_va_mapping(curenv->env_pgdir, fault_va);
        panic("page fault at kernel mode");
    }

	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.

	// Call the environment's page fault upcall, if one exists.  Set up a
	// page fault stack frame on the user exception stack (below
	// UXSTACKTOP), then branch to curenv->env_pgfault_upcall.
	//
	// The page fault upcall might cause another page fault, in which case
	// we branch to the page fault upcall recursively, pushing another
	// page fault stack frame on top of the user exception stack.
	//
	// The trap handler needs one word of scratch space at the top of the
	// trap-time stack in order to return.  In the non-recursive case, we
	// don't have to worry about this because the top of the regular user
	// stack is free.  In the recursive case, this means we have to leave
	// an extra word between the current top of the exception stack and
	// the new stack frame because the exception stack _is_ the trap-time
	// stack.
	//
	// If there's no page fault upcall, the environment didn't allocate a
	// page for its exception stack, or the exception stack overflows,
	// then destroy the environment that caused the fault.
	//
	// Hints:
	//   user_mem_assert() and env_run() are useful here.
	//   To change what the user environment runs, modify 'curenv->env_tf'
	//   (the 'tf' variable points at 'curenv->env_tf').
	
	// LAB 4: Your code here.
    struct UTrapframe *utf;
    uint32_t uesp = curenv->env_tf.tf_esp;

    if (curenv->env_pgfault_upcall == NULL) {
        print_trapframe(tf);
        env_destroy(curenv);
    }

    if ((uesp >= UXSTACKTOP - PGSIZE) && (uesp < UXSTACKTOP)) {
        /* nested page fault */
        curenv->env_tf.tf_esp = uesp - 4 - sizeof(struct UTrapframe);
        * (int *) (uesp - 4) = MAGIC_BLANK;
    } else {
        curenv->env_tf.tf_esp = UXSTACKTOP - sizeof(struct UTrapframe);
    }

    user_mem_assert(curenv, curenv->env_pgfault_upcall, 4, PTE_U);
    user_mem_assert(curenv, (void *) (curenv->env_tf.tf_esp - 4), PGSIZE, PTE_U);
    /* MAGIC_BREAK; */

    utf = (struct UTrapframe *) curenv->env_tf.tf_esp;
    utf->utf_fault_va = fault_va;
    utf->utf_err = tf->tf_err;
    utf->utf_regs = tf->tf_regs;
    utf->utf_eip = tf->tf_eip;
    utf->utf_eflags = tf->tf_eflags;
    utf->utf_esp = uesp;
    /* print_utrapframe(utf); */

    curenv->env_tf.tf_eip = (uintptr_t) curenv->env_pgfault_upcall;
}

void
tf_handler_default(struct Trapframe *tf)
{
    int retval;
    
    if (tf->tf_trapno == T_SYSCALL) {
        retval = syscall(tf->tf_regs.reg_eax, tf->tf_regs.reg_edx,
                         tf->tf_regs.reg_ecx, tf->tf_regs.reg_ebx,
                         tf->tf_regs.reg_edi, tf->tf_regs.reg_esi);
        tf->tf_regs.reg_eax = retval;
        return;
    }
    
	if (tf->tf_cs == GD_KT) {
		panic("unhandled trap in kernel");
		env_destroy(curenv);
    } else {
		env_destroy(curenv);
		return;
	}
}

void
tf_handler_brkpt(struct Trapframe *tf)
{
    monitor(tf);
}

void
irq_handler_clock(struct Trapframe *tf)
{
    if (tf->tf_cs == GD_KT) {
        panic("Timer interrupt at kernel");
    }
    sched_yield();
}

