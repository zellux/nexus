#include <kern/time.h>

#include <inc/assert.h>

unsigned ticks;

void
time_init() {
	ticks = 0;
}

// this is called once per timer interupt; a timer interupt fires 100 times a
// second
void
time_tick() {
	if (ticks == ~0)
		panic("time_tick: time overflowed");
	ticks++;
}

unsigned 
time_msec() {
	return ticks * 10;
}

