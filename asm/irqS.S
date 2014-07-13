#include <lib/gcc.h>
#include "irq.h"
#include "frame.h"
#include "seg.h" /* for KCODESEL */

#define HEAD(irqno) \
	ENTRY(irq##irqno) \
	pushl $irqno; \
	PUSHALL; \
	LOADSEG; \
	call doirq

#define TAIL(irqno) \
	cmpl $KCODESEL,CS(%esp); \
	je 1f; \
	call rettouser; \
1:	POPALL; \
	addl $4,%esp; \
	iret

#define MASTER(irqno) \
HEAD(irqno); \
movb $EOIVALUE,%al; outb %al,$M8259EOI; .word 0x00eb,0x00eb; \
TAIL(irqno)

#define SLAVER(irqno) \
HEAD(irqno); \
movb $EOIVALUE,%al; outb %al,$S8259EOI; .word 0x00eb,0x00eb; \
movb $EOIVALUE,%al; outb %al,$M8259EOI; .word 0x00eb,0x00eb; \
TAIL(irqno)

MASTER(0x00)
MASTER(0x01)
MASTER(0x02)
MASTER(0x03)
MASTER(0x04)
MASTER(0x05)
MASTER(0x06)
MASTER(0x07)

SLAVER(0x08)
SLAVER(0x09)
SLAVER(0x0a)
SLAVER(0x0b)
SLAVER(0x0c)
SLAVER(0x0d)
SLAVER(0x0e)
SLAVER(0x0f)
