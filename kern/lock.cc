#include <lib/root.h>
#include <asm/system.h>
#include "sched.h"

void waitq_t::waitsleep(int state)
{
	cli();
	curr->deqrun();
	curr->state = state;
	enqtail(curr);
	sti();
	sched();
}

static void alarm(void * data)
{
	task_t * task;
	
	task = (task_t *) data; 
	task->wakeup();
}

#warning "to be continue..."
int waitq_t::waitsleep(int state, int ntick)
{
	timer_t timer;
	timer.init(ntick, alarm, curr);
	timer.start();

	cli();
	curr->deqrun();
	curr->state = state;
	enqtail(curr);
	sti();
	sched();

	timer.stop();
	return timer.lefttick;
}

/*
 * Note that this doesn't need cli-sti pairs: interrupts may not change
 * the wait-queue structures directly, but only call wake_up() to wake
 * a process. The process itself must remove the queue once it has woken.
 */
void waitq_t::signal() 
{
	task_t * task;

	if (task = head())
		task->wakeup();
}

void waitq_t::broadcast()
{
	while (!empty())
		signal();
}

void lock_t::lock()
{
	while (locked_) {
		if (lockowner == curr)
			trace("maybe lead to deadlock\n");
		waitq.sleep();
	}
	locked_ = 1;
	lockowner = curr;
}

void lock_t::unlock()
{
	locked_ = 0;
	waitq.broadcast();
	lockowner = NULL;
}

void iolock_t::lock()
{
	cli();
	while (locked_)
		waitq.sleep();
	locked_ = 1;
	sti();
}

/* called during interrupt */
void iolock_t::unlock() 
{
	psw_t psw;

	psw.save(), cli();
	locked_ = 0;
	waitq.broadcast();
	psw.restore();
}

void iolock_t::wait()
{
	cli();		
	while (locked_)
		waitq.sleep();
	sti();
}

void rwlock_t::rlock()
{
	while (state == WRITING) {
		if (lockowner == curr)
			warn("caller acquire the readlock twice\n");
		waitq.sleep();
	}
	state = READING;
	nreader++;
	lockowner = curr;
}

void rwlock_t::wlock()
{
	while (state != FREE) {
		if (lockowner == curr)
			panic("caller acquire the writelock twice\n");
		waitq.sleep();
	}
	state = WRITING;
	lockowner = curr;
}

void rwlock_t::unlock()
{
	switch (state) {
		case FREE:
			panic("unlock free rwlock\n");
			return;
		case READING:
			if (--nreader > 0)
				return;
			state = FREE;
			waitq.broadcast();
			lockowner = NULL;
			return;
		case WRITING:
			state = FREE;
			waitq.broadcast();
			lockowner = NULL;
			return;
	}
}

void sema_t::down(int n)
{
	assert(n <= maxcnt);
	while ((curcnt - n) < 0)
		waitq.sleep();
	curcnt -= n;
}

void sema_t::up(int n)
{
	assert(curcnt + n <= maxcnt);
	if ((curcnt += n) > 0)
		(n == 1) ? waitq.signal() : waitq.broadcast();
}
