// User-level IPC library routines

#include <inc/lib.h>

#define dprintk(_f, _a...)
/* #define dprintk(_f, _a...) cprintf(_f, ##_a) */

// Receive a value via IPC and return it.
// If 'pg' is nonnull, then any page sent by the sender will be mapped at
//	that address.
// If 'fromenv' is nonnull, then store the IPC sender's envid in *fromenv.
// If 'perm' is nonnull, then store the IPC sender's page permission in *perm
//	(this is nonzero iff a page was successfully transferred to 'pg').
// If the system call fails, then store 0 in *fromenv and *perm (if
//	they're nonnull) and return the error.
//
// Hint:
//   Use 'env' to discover the value and who sent it.
//   If 'pg' is null, pass sys_ipc_recv a value that it will understand
//   as meaning "no page".  (Zero is not the right value.)
int32_t
ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
{
	// LAB 4: Your code here.
    if (pg) {
        sys_ipc_recv(pg);
    } else {
        sys_ipc_recv((void *) -1);
    }
    dprintk("[IPC] from %08x to %08x\n", env->env_ipc_from, env->env_id);
    if (from_env_store)
        *from_env_store = env->env_ipc_from;
    if (perm_store)
        *perm_store = env->env_ipc_perm;
	return env->env_ipc_value;
}

// Send 'val' (and 'pg' with 'perm', assuming 'pg' is nonnull) to 'toenv'.
// This function keeps trying until it succeeds.
// It should panic() on any error other than -E_IPC_NOT_RECV.
//
// Hint:
//   Use sys_yield() to be CPU-friendly.
//   If 'pg' is null, pass sys_ipc_recv a value that it will understand
//   as meaning "no page".  (Zero is not the right value.)
void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
	// LAB 4: Your code here.
    int ret;

    dprintk("[IPC] send from %08x to %08x, value=%d, pg=%p, perm=%x\n",
            sys_getenvid(), to_env, val, pg, perm);
    while (1) {
        if (pg == NULL) {
            /* TODO: allow pg=0 */
            ret = sys_ipc_try_send(to_env, val, (void *) -1, 0);
        } else {
            ret = sys_ipc_try_send(to_env, val, pg, perm);
        }
        if (ret == 0 || ret == 1)
            break;
        if (ret != -E_IPC_NOT_RECV)
            panic("ipc_send error %e", ret);
    }
}

