#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>

#if defined(DEBUG_SCHED)
#undef dprintk
#define dprintk(_f, _a...)
#endif

// Choose a user environment to run and run it.
void
sched_yield(void)
{
	// Implement simple round-robin scheduling.
	// Search through 'envs' for a runnable environment,
	// in circular fashion starting after the previously running env,
	// and switch to the first such environment found.
	// It's OK to choose the previously running env if no other env
	// is runnable.
	// But never choose envs[0], the idle environment,
	// unless NOTHING else is runnable.

	// LAB 4: Your code here.
    struct Env *e;

    if (curenv == NULL)
        curenv = &envs[0];
    
    for (e = curenv + 1; e < &envs[NENV]; e++) {
        if (e->env_status == ENV_RUNNABLE) {
            dprintk("Now switch to env[%08x]\n", e->env_id);
            env_run(e);
            return;
        }
    }

    for (e = &envs[1]; e <= curenv; e++) {
        if (e->env_status == ENV_RUNNABLE) {
            dprintk("Now switch to env[%08x]\n", e->env_id);
            env_run(e);
            return;
        }
    }

	// Run the special idle environment when nothing else is runnable.
	if (envs[0].env_status == ENV_RUNNABLE) {
        dprintk("Scheduler: have nothing to run but idle\n");
		env_run(&envs[0]);
	} else {
		cprintf("Destroyed all environments - nothing more to do!\n");
		while (1)
			monitor(NULL);
	}
}
