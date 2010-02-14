// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800
#define DEBUG_FORK  0
#if DEBUG_FORK == 1
#define MAGIC_BREAK                                                 \
    do {                                                            \
        cprintf("Magic break at %s[%d]\n", __FUNCTION__, __LINE__); \
        __asm__("xchg %%bx, %%bx": :);                              \
    } while (0)
#else
#define MAGIC_BREAK
#endif

#if DEBUG_FORK == 1
#define dprintk(_f, _a...) cprintf(_f, ## _a)
#else
#define dprintk(_f, _a...)
#endif

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;
    int pdx, ptx;
    int *pt;
    pte_t pte;
    uintptr_t va;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at vpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
    dprintk("[PGFAULT] addr=%p, err=%d\n", addr, err);
    pdx = PDX(addr);
    ptx = PTX(addr);
    pt = (int *) ((PDX(UVPT) << PDXSHIFT) | (pdx << PTXSHIFT));
    pte = (pte_t) pt[ptx];
    
    if (!(err & FEC_WR) | !(pte & PTE_COW)) {
        panic("not a copy-on-write page fault");
    }

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.
	
	// LAB 4: Your code here.
    /* TODO: check whether sys_page_alloc here will remove previous pages */
    va = (uintptr_t) addr & ~(PGSIZE - 1);
    dprintk("Read-only page:\n");
    /* sys_debug_va_mapping(va); */
    sys_page_alloc(0, PFTEMP, PTE_W|PTE_U|PTE_P);
    memmove((void *) PFTEMP, (const void *) va, PGSIZE);
    sys_page_map(0, (void *) PFTEMP, 0, (void *) va, PTE_U|PTE_W|PTE_P);
    sys_page_unmap(0, (void *) PFTEMP);
    dprintk("COW page:\n");
    /* sys_debug_va_mapping(va); */
    dprintk("[PGFAULT] Copy on write page at %p\n", va);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why mark ours copy-on-write again
// if it was already copy-on-write?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
// 
static int
duppage(envid_t envid, unsigned pn)
{
	int r, pdx, ptx;
	void *addr;
	pte_t pte;
    int *pt;

	// LAB 4: Your code here.
    /* TODO: why mark COW again? */
    addr = (void *) (pn * PGSIZE);
    dprintk("[COW] Now dup addr 0x%08x.\n", addr);
    pdx = PDX(addr);
    ptx = PTX(addr);
    pt = (int *) ((PDX(UVPT) << PDXSHIFT) | (pdx << PTXSHIFT));
    pte = (pte_t) pt[ptx];
    if (pte & PTE_COW) {
        sys_page_map(0, (void *) addr, envid, (void *) addr,
                     PTE_U|PTE_P|PTE_COW);
        return 0;
    }
    if ((pte & PTE_W) || (pte & PTE_COW)) {
        pte &= !PTE_W;
        pte |= PTE_COW;
    }
    sys_page_map(0, (void *) addr, envid, (void *) addr, PTE_U|PTE_P|PTE_COW);
    sys_page_map(0, (void *) addr, 0, (void *) addr, PTE_U|PTE_P|PTE_COW);
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use vpd, vpt, and duppage.
//   Remember to fix "env" and the user exception stack in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
    envid_t envid;
    uint32_t addr;
    int ptx, i, j;

    dprintk("[FORK] Setting pgfault handler for env[%x]\n", env->env_id);
    set_pgfault_handler(pgfault);
    envid = sys_exofork();

    if (envid != 0) {
        for (i = 0; i < PDX(UTOP); i++) {
            if (!(vpd[i] & PTE_P))
                continue;
        
            addr = (PDX(UVPT) << PDXSHIFT) | (i << PTXSHIFT);
            for (j = 0; j < NPTENTRIES; j++) {
                /* Skip exeption stack page */
                if (i == PDX(UXSTACKTOP-PGSIZE) && j == PTX(UXSTACKTOP-PGSIZE))
                    continue;
                if (vpt[i * NPTENTRIES + j] & PTE_P) {
                    duppage(envid, i * NPTENTRIES + j);
                }
            }
        }
        
        extern void *_pgfault_upcall(void);
        sys_page_alloc(envid, (void *) UXSTACKTOP - PGSIZE, PTE_U|PTE_P|PTE_W);
        sys_env_set_pgfault_upcall(envid, _pgfault_upcall);
        sys_env_set_status(envid, ENV_RUNNABLE);
    } else {
		env = &envs[ENVX(sys_getenvid())];
    }
    
    return envid;
}


// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
