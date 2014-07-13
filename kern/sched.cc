#include <lib/root.h>
#include <lib/gcc.h>
#include <lib/ostream.h>
#include <init/ctor.h>
#include <asm/seg.h>
#include <asm/irq.h>
#include <mm/allockm.h>
#include "sched.h"
#include "signal.h"

static int needresched;
static ulong nlongjump;

#define NRUNTASKQ 32
static ulong mask;
static runtaskq_t runtaskq[NRUNTASKQ];
task_t * curr; /* current task */
static softirq_t * timersoftirq;

extern void timersoftisr(void * obj);
static void timerisr(void * obj, int irqno);
__ctor(PRIKERN,SUBANY,scheduler)
{
	construct(runtaskq, NRUNTASKQ);
	allocirq(timerisr, NULL, CLKIRQ);
	irqon(CLKIRQ);
	timersoftirq = allocsoftirq(timersoftisr, NULL);
}

void dumpsched(ostream_t * os)
{
	os->write("passed tick = %d, context switch = %d\n", passedtick, nlongjump);
}

static void checkmask()
{
#if DEBUG
	psw_t psw;

	psw.save(), cli();
	assert(mask);
	for (int i = 0; i < NRUNTASKQ; i++) {
		if ((mask >> i) & 1) {
			assert(!runtaskq[i].empty());
		} else {
			assert(runtaskq[i].empty());
		}
	}
	psw.restore();
#endif
}

/* interruptible task will be waked up at the time it receive a signal */
static void checkwait()
{
#if DEBUG
	task_t * task;

	foreach (task, alltaskq)
		if (task->state == TWAIT)
			assert(!task->hassig());
#endif
}

static void timerisr(void * obj, int irqno)
{
	timersoftirq->request();
	passedtick++;
	if (curr->state != TRUN)
		return;
	/* uspace(r->eip) ? curr->utick++ : curr->stick++; */
	if (curr->counter == 0) {
		needresched = 1;
		return;
	}
	curr->setcounter(curr->counter - 1);
	needresched = 1; /* unnecessary */
	return;
}

void task_t::enqrun()
{
	assert(between(0, counter, NRUNTASKQ));
	runtaskq[counter].enqtail(this);
	mask |= 1 << counter;
}

void task_t::deqrun()
{
	assert(between(0, counter, NRUNTASKQ));
	//assert(runtaskq[counter].index(this) >= 0);
	unlinkrun();
	if (runtaskq[counter].empty())
		mask &= ~(1 << counter);
}

void task_t::setcounter(int newval)
{
	psw_t psw;

	if (state != TRUN) {
		counter = newval;
		return;
	}
	psw.save(), cli();
	deqrun();
	counter = newval;
	enqrun();
	psw.restore();
}

static task_t * find()
{
	task_t * task;
	int idx;

	checkwait();
	checkmask();
repeat:	asm ("bsrl %1,%0":"=r"(idx):"r"(mask));
	if (!idx) { /* all the running task eat up their time quota */
		foreach (task, alltaskq) {
			int newval = task->priority + (task->counter << 1);
			if (newval >= NRUNTASKQ)
				newval = NRUNTASKQ - 1;
			if (task == &idletask)
				newval = 1;
			task->setcounter(newval);
		}
		goto repeat;
	}
	task = runtaskq[idx].head();
	allege(task->state == TRUN);
	return task;
}

extern int sig;
void sched()
{
	task_t * target;

	cli();
	target = find();
	needresched = 0;
	if ((curr->state == TRUN) && (curr->counter == target->counter)) {
		sti();
		return;
	}
	tss_t * from = &curr->tss, * to = &target->tss;
	curr = target;
	nlongjump++, longjump(from, to);
	if (curr == lastusedmath)
		asm volatile ("clts");
	sti();
}

void prempt()
{
	if (needresched)
		sched();
}

/* called during interrupt */
void task_t::wakeup()
{
	if ((state != TSLEEP) && (state != TWAIT))
		return;
	psw_t psw;
	psw.save(), cli();
	unlinkrun();
	state = TRUN;
	enqrun();
	if (counter >= curr->counter + 2)
		needresched = 1;
	psw.restore();
}

static void sigalarm(void * obj)
{
	((task_t*)obj)->post(SIGALRM);
}

asmlinkage int sysalarm(int seconds)
{
	if (seconds == 0) {
		curr->alarm.stop();
		return 0;
	}
	if (curr->alarm.active())
		return 0;
	curr->alarm.init(timetotick(seconds), sigalarm, curr);
	curr->alarm.start();
	return 0;
}

asmlinkage int sysnice(int inc)
{
	if (inc < 0 && !suser())
		return EPERM; 
	if (between(0, curr->priority - inc, NRUNTASKQ))
		curr->priority -= inc;
	curr->setcounter(0);
	return 0;
}
