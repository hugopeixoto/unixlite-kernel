#ifndef _TIMER_H
#define _TIMER_H

#include <lib/queue.h>
#include "time.h"

/* one shot timer, don't support periodic timer */
struct timer_t {
	CHAIN(,timer_t);
	/* never chanage once init */
	tick_t period;
	vfp_t func;
	void * data;
	/* tempory variable */
	tick_t lefttick, starttick;
	long magic;

	bool active() { return next != NULL; }
	void start();
	void stop();
	void start(tick_t period_) { period = period_; start(); }
	void restart() { stop(); start(); }
	void restart(tick_t period_) { stop(); start(period_); }
	void init(tick_t period_, vfp_t func_, void * data_);
	timer_t() { next = prev = NULL; magic = 0x3721748; }
	~timer_t() { stop(); magic = 0x3721dead; }
};
QUEUE(,timer_t);

#endif
