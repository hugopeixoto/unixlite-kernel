#include <lib/root.h>
#include <init/ctor.h>
#include <asm/irq.h>
#include <asm/system.h>
#include "timer.h"

#define NTICK 512
static int cur;
static Q(,timer_t) below[NTICK];
static Q(,timer_t) above;

__ctor(PRIKERN, SUBANY, timer)
{
	construct(below, NTICK);
	construct(&above);
}

#warning "+1-1"
static inline void insert(tick_t lefttick, timer_t * t)
{
	below[(cur + lefttick) & (NTICK-1)].enqtail(t);
}

void timer_t::init(tick_t period_, vfp_t func_, void * data_)
{
	magic = 0x3721748;
	next = prev = NULL;
	period = period_;
	func = func_; 
	data = data_; 
}

void timer_t::start()
{
	if (active()) {
		trace("timer has been already start\n");
		return;
	}
	starttick = passedtick;
	lefttick = period;
	assert(period);
	if (lefttick < NTICK)
		insert(lefttick, this);
	else
		above.enqtail(this);
}

void timer_t::stop()
{
	if (active())
		unlink();
}

/* NOTE: the timer itself may be destroyed or stopped during executing t->func(t->data) */
void timersoftisr(void * object)
{
	timer_t * t;

	while (t = below[cur].deqhead()) {
		assert(t->magic == 0x3721748); 
		t->func(t->data); 
		/* can't access t */
	}
	if (cur = (cur + 1) & (NTICK - 1))
		return;
	foreachsafe (t, above) {
		if ((t->lefttick -= NTICK) > NTICK)
			continue;
		t->unlink();
		insert(t->lefttick, t);
	}
}
