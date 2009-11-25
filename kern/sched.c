#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>


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
    int i;

    for (i = 1; i < NENV; i++) {
        if (&envs[i] == curenv) 
            continue;
        if (envs[i].env_status == ENV_RUNNABLE) {
            dprintk("Now switch to env[%d]\n", i);
            env_run(&envs[i]);
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
