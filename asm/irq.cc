#include <lib/root.h>
#include <lib/gcc.h>
#include "frame.h"
#include "irq.h"

#define NIRQ 16
struct irq_t {
	isr_t isr;
	void * obj;
} irqvec[NIRQ];

int allocirq(isr_t isr, void * obj, int irqno)
{
	irq_t * i = irqvec + irqno;
	if (i->isr)
		panic("IRQ(%02d) has been allocated\n", irqno);
	i->isr = isr;
	i->obj = obj;
	return 0;
}

void freeirq(int irqno)
{
	panic("silly you!");
}

asmlinkage void doirq(ulong ebx)
{
	regs_t * r = (regs_t *) &ebx;
	uint irqno = r->u.irqno;
	assert(r->u.irqno < NIRQ);
	irq_t * i = irqvec + irqno;
	if (!i->isr) {
		warn("magic irq, irqno(%d)\n", irqno);
		return;
	}
	i->isr(i->obj, irqno);
}

#define NSOFTIRQ 32
static ulong requestmask, doingmask;
static softirq_t softirqvec[NSOFTIRQ];
static int nsoftirq;
softirq_t * allocsoftirq(softisr_t softisr, void * obj)
{
	if (nsoftirq >= NSOFTIRQ)
		panic("allocsoftirq fail\n"); 
	softirq_t * i = softirqvec + nsoftirq;
	i->softisr = softisr;
	i->obj = obj;
	nsoftirq++;
	return i;
}

void softirq_t::request()
{
	requestmask |= 1 << (this-softirqvec);
}

/* NOTE:
   when the soft irq handler is executing:
   1) it should not block the current proecsss
   2) all the interrupt is enabled
   3) it is atomic and will not be re-entrant */
void dosoftirq()
{
	if (!requestmask)
		return;
	softirq_t * s = softirqvec;
	softirq_t * end = softirqvec + nsoftirq;
	for (ulong mask = 1; s < end; mask <<= 1, s++) {
		if (!(requestmask & mask) || (doingmask & mask))
			continue;
		requestmask &= ~mask;
		doingmask |= mask;
		assert(s->softisr);
		s->softisr(s->obj);
		doingmask &= ~mask;
	}
}
