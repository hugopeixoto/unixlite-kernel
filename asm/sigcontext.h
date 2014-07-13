#ifndef _ASMSIGCONTEXT_H
#define _ASMSIGCONTEXT_H

#include <kern/signal.h>
struct sigcontext_t {
	u32_t retaddr;
	int signo;
	u16_t gs, __gsh;
	u16_t fs, __fsh;
	u16_t es, __esh;
	u16_t ds, __dsh;
	u32_t edi;
	u32_t esi;
	u32_t ebp;
	u32_t esp;
	u32_t ebx;
	u32_t edx;
	u32_t ecx;
	u32_t eax;
	u32_t eip;
	u16_t cs, __csh;
	u32_t eflags;
	u16_t ss, __ssh;
	sigset_t oldsigmask;
	char asmcode[8];
};
#endif
