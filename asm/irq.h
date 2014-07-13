#ifndef _ASMIRQ_H
#define _ASMIRQ_H

#define	M8259EOI	0x20
#define M8259IMR	0x21
#define	S8259EOI	0xa0
#define S8259IMR	0xa1

#define	EOIVALUE 	0x20

/* they can't be combined */
#define CLKIRQ		0x0
#define	KBDIRQ		0x1	
#define HDIRQ		0xe
#define FLPIRQ          0xx

#ifndef __ASSEMBLY__
#include "io.h"

#define shift(irqno) (1 << (irqno))
extern inline void irqon(int irqno)
{
	uchar mask;

	if (irqno < 8) {
		mask = ~shift(irqno); /* enable INT when the bit is off */
		outbp(inbp(M8259IMR) & mask, M8259IMR);
	} else {
		mask = ~shift(2);
		outbp(inbp(M8259IMR) & mask, M8259IMR);
		mask = ~shift(irqno & 7);
		outbp(inbp(S8259IMR) & mask, S8259IMR);
	}
}

extern inline void irqoff(int irqno)
{
	uchar mask;

	if (irqno < 8) {
		mask = shift(irqno); /* disable INT whe the bit is on */
		outbp(inbp(M8259IMR) | mask, M8259IMR);
	} else {
		mask = shift(irqno & 7);
		outbp(inbp(S8259IMR) | mask, S8259IMR);
	}
}
#undef shift

extern inline void allirqon()
{
	outbp(0, M8259IMR);
	outbp(0, S8259IMR);
}

extern inline void allirqoff()
{
	outbp(0xff, M8259IMR);
	outbp(0xff, S8259IMR);
}

typedef void (*isr_t)(void * obj, int irqno);
extern int allocirq(isr_t isr, void * obj, int irqno);

typedef void (*softisr_t)(void * obj);
struct softirq_t {
	softisr_t softisr;
	void * obj;

	void request();
};
extern softirq_t * allocsoftirq(softisr_t softisr, void * obj);

#endif

#endif
