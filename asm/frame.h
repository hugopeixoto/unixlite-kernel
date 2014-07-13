#ifndef _ASMFRAME_H
#define _ASMFRAME_H

#define EBX	0
#define ECX	4
#define EDX	8
#define ESI	12
#define EDI	16
#define EBP	20 
#define EAX	24
#define DS	28
#define ES	32
#define FS	36
#define GS	40
#define ORIGEAX 44
#define ERRORCODE ORIGEAX
#define IRQNO	ORIGEAX		
#define EIP	48
#define CS	52
#define EFLAGS	56
#define ESP	60
#define SS	64

#define PUSHALL \
	subl $8,%esp; push %es; push %ds; \
	pushl %eax; pushl %ebp; \
	pushl %edi; pushl %esi; pushl %edx; pushl %ecx; pushl %ebx

#define LOADSEG movl $KDATASEL,%eax; mov %ax,%ds; mov %ax,%es;

#define POPALL \
	popl %ebx; popl %ecx; popl %edx; popl %esi; popl %edi; \
	popl %ebp; popl %eax; \
	popl %ds; popl %es; addl $8,%esp \

#ifndef __ASSEMBLY__
#define NOTSYSCALLFRAME 0x19790106
struct regs_t {
	ulong ebx;
	ulong ecx;
	ulong edx;
	ulong esi;
	ulong edi;
	ulong ebp;
	ulong eax;
	ushort ds, __dsu;
	ushort es, __esu;
	ushort fs, __fsu;
	ushort gs, __gsu;
	union {
		long origeax;	/* for syscall frame, auto restart */
		long errorcode;	/* for trap frame */
		long irqno;	/* for irq frame */
	} u;
	ulong eip;
	ushort cs, __csu;
	ulong eflags;
	ulong esp;
	ushort ss, __ssu;
};
#endif

#endif
