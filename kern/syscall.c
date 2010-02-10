/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>
#include <kern/kdebug.h>

char *syscall_names[] = {
	"cputs",
	"cgetc",
	"getenvid",
	"env_destroy",
	"page_alloc",
	"page_map",
	"page_unmap",
	"exofork",
	"env_set_status",
	"env_set_trapframe",
	"env_set_pgfault_upcall",
	"yield",
	"ipc_try_send",
	"ipc_recv",
    "debug_va_mapping",
};

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.
    user_mem_assert(curenv, s, len, PTE_U);

	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console.
// Returns the character.
static int
sys_cgetc(void)
{
	int c;

	// The cons_getc() primitive doesn't wait for a character,
	// but the sys_cgetc() system call does.
	while ((c = cons_getc()) == 0)
		/* do nothing */;

	return c;
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

    dprintk("[SYSCALL] [%08x] kills [%08x]\n", curenv->env_id, envid);
	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.
	
	// LAB 4: Your code here.
    struct Env *e;

    if (env_alloc(&e, curenv->env_id)) {
        panic("sys_exofork: no more env available.");
    }

    e->env_tf = curenv->env_tf;
    e->env_tf.tf_regs.reg_eax = 0;
    e->env_status = ENV_NOT_RUNNABLE;

    return e->env_id;
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
  	// Hint: Use the 'envid2env' function from kern/env.c to translate an
  	// envid to a struct Env.
	// You should set envid2env's third argument to 1, which will
	// check whether the current environment has permission to set
	// envid's status.
	
	// LAB 4: Your code here.
    struct Env *e;
    int ret;

    if ((ret = envid2env(envid, &e, 1)))
        return ret;

    if ((status != ENV_RUNNABLE) && (status != ENV_NOT_RUNNABLE))
        return -E_INVAL;

    e->env_status = ENV_RUNNABLE;
    return 0;
}

// Set envid's trap frame to 'tf'.
// tf is modified to make sure that user environments always run at code
// protection level 3 (CPL 3) with interrupts enabled.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 4: Your code here.
	// Remember to check whether the user has supplied us with a good
	// address!
	panic("sys_set_trapframe not implemented");
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{
	// LAB 4: Your code here.
    int ret;
    struct Env *e;
    
    if ((ret = envid2env(envid, &e, 1)))
        return ret;

    e->env_pgfault_upcall = func;
    return 0;
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	// Hint: This function is a wrapper around page_alloc() and
	//   page_insert() from kern/pmap.c.
	//   Most of the new code you write should be to check the
	//   parameters for correctness.
	//   If page_insert() fails, remember to free the page you
	//   allocated!

	// LAB 4: Your code here.
    struct Env *e;
    int ret;
    uintptr_t vaddr = (uintptr_t) va;
    struct Page *pp;

    if (!(perm & PTE_P) || !(perm & PTE_U) ||
        (perm & (PTE_PWT | PTE_PCD | PTE_A | PTE_D | PTE_PS | PTE_MBZ))) {
        dprintk("sys_page_alloc: perm is inappropriate.\n");
        return -E_INVAL;
    }

    if ((vaddr >= UTOP) || (vaddr % PGSIZE != 0)) {
        dprintk("sys_page_alloc: incorrect virual address.\n");
        return -E_INVAL;
    }

    if ((ret = envid2env(envid, &e, 1)))
        return ret;

    if (page_alloc(&pp)) {
        dprintk("sys_page_alloc: no more free memory.\n");
        return -E_NO_MEM;
    }
    memset(page2kva(pp), 0, PGSIZE);

    ret = page_insert(e->env_pgdir, pp, va, perm);
    /* dump_va_mapping(e->env_pgdir, vaddr); */
    /* dprintk("[DONE]: Insert a page at va 0x%08x.\n", va); */
    tlbflush();
    return ret;
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	// Hint: This function is a wrapper around page_lookup() and
	//   page_insert() from kern/pmap.c.
	//   Again, most of the new code you write should be to check the
	//   parameters for correctness.
	//   Use the third argument to page_lookup() to
	//   check the current permissions on the page.

	// LAB 4: Your code here.
    struct Env *esrc, *edst;
    int ret;
    uintptr_t srcvaddr, dstvaddr;
    pte_t *srcpte, *dstpte;
    struct Page *pp;

    /* TODO: should we increase pp->ref_count? */
    if ((ret = envid2env(srcenvid, &esrc, 1)))
        return ret;
    if ((ret = envid2env(dstenvid, &edst, 1)))
        return ret;

    srcvaddr = (uintptr_t) srcva;
    dstvaddr = (uintptr_t) dstva;
    if ((srcvaddr >= UTOP) || (srcvaddr % PGSIZE != 0) ||
        (dstvaddr >= UTOP) || (dstvaddr % PGSIZE != 0)) {
        dprintk("sys_page_map: invalid srcva or dstva.\n");
        return -E_INVAL;
    }

    if ((pp = page_lookup(esrc->env_pgdir, srcva, &srcpte)) == 0) {
        dprintk("sys_page_map: 0x%08x is not mapped in src env.\n", srcvaddr);
        return -E_INVAL;
    }
    
    if (!(perm & PTE_P) || !(perm & PTE_U) ||
        (perm & (PTE_PWT | PTE_PCD | PTE_A | PTE_D | PTE_PS | PTE_MBZ))) {
        dprintk("sys_page_map: perm is inappropriate.\n");
        return -E_INVAL;
    }

    if ((perm & PTE_W) && !((*srcpte) & PTE_W)) {
        dprintk("sys_page_map: do not allow write.\n");
        return -E_INVAL;
    }

    ret = page_insert(edst->env_pgdir, pp, dstva, perm);
    tlbflush();
    return ret;
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	// Hint: This function is a wrapper around page_remove().
	
	// LAB 4: Your code here.
    int ret;
    struct Env *e;
    uintptr_t vaddr;

    if ((ret = envid2env(envid, &e, 1)))
        return ret;

    dprintk("sys_page_unmap: page %08x\n", va);
    vaddr = (uintptr_t) va;
    if ((vaddr >= UTOP) || (vaddr % PGSIZE != 0)) {
        return -E_INVAL;
    }

    page_remove(e->env_pgdir, va);
    tlbflush();
    return 0;
}

// Try to send 'value' to the target env 'envid'.
// If va != 0, then also send page currently mapped at 'va',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target has not requested IPC with sys_ipc_recv.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused ipc_recv system call.
//
// If the sender sends a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc doesn't happen unless no errors occur.
//
// Returns 0 on success where no page mapping occurs,
// 1 on success where a page mapping occurs, and < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{
	// LAB 4: Your code here.
    struct Env *e;
    uintptr_t srcvaddr;
    struct Page *srcpp, *dstpp;
    pte_t *srcpte, *dstpte;
    int r;

    if ((r = envid2env(envid, &e, 0)) < 0) {
        return r;
    }

    if (!e->env_ipc_recving) {
        return -E_IPC_NOT_RECV;
    }

    srcvaddr = (uintptr_t) srcva;
    if (srcvaddr < UTOP && (uintptr_t) e->env_ipc_dstva < UTOP) {
        if (srcvaddr % PGSIZE != 0)
            return -E_INVAL;
        if (!(perm & PTE_P) || !(perm & PTE_U) ||
            (perm & (PTE_PWT | PTE_PCD | PTE_A | PTE_D | PTE_PS | PTE_MBZ)))
            return -E_INVAL;
        if (user_mem_check(curenv, srcva, PGSIZE, PTE_P|PTE_U))
            return -E_INVAL;
        /* TODO: what if the receiver don't assume a page transfer? */
        /* dprintk("[IPC] [%08x]<%08x> mapping to [%08x]<%08x>\n", */
        /*         curenv->env_id, srcva, envid, e->env_ipc_dstva); */
        if ((srcpp = page_lookup(curenv->env_pgdir, srcva, &srcpte)) == 0) {
            dprintk("sys_ipc_try_send: 0x%08x is not mapped in src env.\n", srcva);
            return -E_INVAL;
        }
        page_insert(e->env_pgdir, srcpp, e->env_ipc_dstva, perm);
        /* dump_va_mapping(e->env_pgdir, (uintptr_t) e->env_ipc_dstva); */
        e->env_ipc_perm = perm;
    } else {
        e->env_ipc_perm = 0;
    }
    /* dprintk("[IPC] [%08x] send %d to [%08x]\n", curenv->env_id, value, envid); */
    e->env_ipc_recving = 0;
    e->env_ipc_from = curenv->env_id;
    e->env_ipc_value = value;
    e->env_status = ENV_RUNNABLE;
    e->env_tf.tf_regs.reg_eax = 0;

    return (e->env_ipc_perm != 0);
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{
	// LAB 4: Your code here.
    uintptr_t dstvaddr = (uintptr_t) dstva;
    
    curenv->env_ipc_dstva = (void *) -1;
    if (dstvaddr < UTOP) {
        if (dstvaddr % PGSIZE != 0)
            return -E_INVAL;
        curenv->env_ipc_dstva = dstva;
    }
    curenv->env_ipc_recving = 1;
    curenv->env_status = ENV_NOT_RUNNABLE;
    sched_yield();

	return 0;
}

static int
sys_dump_env()
{
    struct Env *e = curenv;

    cprintf("env_id = %08x\n", e->env_id);
    cprintf("env_parent_id = %08x\n", e->env_parent_id);
    cprintf("env_runs = %d\n", e->env_runs);
    cprintf("env_pgdir = %08x\n", e->env_pgdir);
    cprintf("env_cr3 = %08x\n", e->env_cr3);
    cprintf("env_syscalls = %d\n", e->env_syscalls);

    return 0;
}

static int
sys_debug_va_mapping(uintptr_t va)
{
	pte_t *pt, pte;
    pde_t *pd, pde;

    pd = curenv->env_pgdir;

    cprintf("[DEBUG] pgdir=%p, va=%p\n", pd, va);
    pde = pd[PDX(va)];
	if (!(pde & PTE_P)) {
        cprintf("[DEBUG] page directory entry not present.\n");
		return 0;
    }
    pt = (pte_t *) KADDR(PTE_ADDR(pde));
    pte = pt[PTX(va)];
	cprintf("[DEBUG] pde=%p, pte=%p\n", pde, pte);
    return 0;
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
    int32_t ret;
    
    curenv->env_syscalls ++;
    /* dprintk("[SYSCALL] %s, a1 %08x, a2 %08x, a3 %08x, a4 %08x a5 %08x\n", */
    /*         syscall_names[syscallno], a1, a2, a3, a4, a5); */
    /* dump_va_mapping(curenv->env_pgdir, (unsigned) syscall); */
    /* dump_va_mapping(curenv->env_pgdir, (unsigned) user_mem_check); */

    switch (syscallno) {
    case SYS_cputs:
        sys_cputs((const char *) a1, a2);
        return 0;
    case SYS_cgetc:
        return sys_cgetc();
    case SYS_getenvid:
        ret = sys_getenvid();
        return ret;
    case SYS_env_destroy:
        return sys_env_destroy(a1);
    case SYS_yield:
        sys_yield();
        return 0;
    case SYS_debug_va_mapping:
        return sys_debug_va_mapping(a1);
    case SYS_exofork:
        return sys_exofork();
    case SYS_env_set_status:
        return sys_env_set_status(a1, a2);
    case SYS_page_alloc:
        return sys_page_alloc(a1, (void *) a2, a3);
    case SYS_page_map:
        return sys_page_map(a1, (void *) a2, a3, (void *) a4, a5);
    case SYS_page_unmap:
        return sys_page_unmap(a1, (void *) a2);
    case SYS_env_set_pgfault_upcall:
        return sys_env_set_pgfault_upcall(a1, (void *) a2);
    case SYS_ipc_recv:
        return sys_ipc_recv((void *) a1);
    case SYS_ipc_try_send:
        return sys_ipc_try_send(a1, a2, (void *) a3, a4);
    }
    
	panic("syscall not implemented");
}

int32_t
do_sysenter(struct Trapframe *tf)
{
    struct PushRegs *r = &tf->tf_regs;
    int32_t ret;
    int *a5ptr;

    curenv->env_tf = *tf;
	curenv->env_tf.tf_ds = GD_UD | 3;
	curenv->env_tf.tf_es = GD_UD | 3;
	curenv->env_tf.tf_ss = GD_UD | 3;
	curenv->env_tf.tf_cs = GD_UT | 3;
    curenv->env_tf.tf_esp = tf->tf_regs.reg_ebp;

    a5ptr = (int *) curenv->env_tf.tf_esp;
    user_mem_assert(curenv, (const void *) a5ptr, 4, PTE_U);
    /* print_trapframe(&curenv->env_tf); */
    
    /* MAGIC_BREAK; */
    ret = syscall(r->reg_eax, r->reg_edx, r->reg_ecx, r->reg_ebx, r->reg_edi, *a5ptr);

    /* Prepare for sysexit  */
    tf->tf_regs.reg_ecx = tf->tf_regs.reg_ebp;
    tf->tf_regs.reg_edx = tf->tf_regs.reg_esi;
    tf->tf_regs.reg_eax = ret;
    
    /* MAGIC_BREAK; */
    asm volatile("movl %0, %%esp\n\t"
                 "popa\n\t"
                 "popl %%ds\n\t"
                 "popl %%es\n\t"
                 "sti\n\t"
                 "sysexit\n\t"
                 :
                 : "g" (tf)
                 );
    return 0;
}

