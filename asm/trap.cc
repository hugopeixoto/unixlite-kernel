#include <lib/root.h>
#include <lib/gcc.h>
#include <kern/sched.h>
#include <boot/bootparam.h>
#include "pte.h"
#include "seg.h"
#include "frame.h"

#if 0
void regs_t::dump()
{
	printf("EIP:    %04x:%08lx\nEFLAGS: %08lx\n", 0xffff & cs,eip,eflags);
	printf("eax: %08lx   ebx: %08lx   ecx: %08lx   edx: %08lx\n",
		eax, ebx, ecx, edx);
	printf("esi: %08lx   edi: %08lx   ebp: %08lx   esp: %08lx\n",
		esi, edi, ebp, esp);
	printf("ds: %04x   es: %04x   fs: %04x   gs: %04x   ss: %04x\n",
		ds, es, fs, gs, ss);
	store_TR(i);
	printf("Pid: %d, process nr: %d (%s)\nStack: ", current->pid, 0xffff & i, current->comm);
	for(i=0;i<5;i++)
		printf("%08lx ", get_seg_long(ss,(i+(unsigned long *)esp)));
	printf("\nCode: ");
	for(i=0;i<20;i++)
		printf("%02x ",0xff & get_seg_byte(cs,(i+(char *)eip)));
}

#endif

#define DOTRAP(trapno, signo, str) \
asmlinkage void dotrap##trapno(ulong ebx) \
{ \
	if (booting) \
		panic("unhandled trap : %s\n", str); \
	trace("i386 trap:%s\n", str); \
	curr->force(signo); \
}

DOTRAP(0x00, SIGFPE,  "divide error")
DOTRAP(0x01, SIGSEGV, "debug")
DOTRAP(0x02, SIGSEGV, "donmi")
DOTRAP(0x03, SIGTRAP, "int3")
DOTRAP(0x04, SIGSEGV, "overflow")
DOTRAP(0x05, SIGSEGV, "bounds")
DOTRAP(0x06, SIGILL,  "invalid operand")
/* DOTRAP(0x07, SIGSEGV, "device not available") */
DOTRAP(0x08, SIGSEGV, "double fault")
DOTRAP(0x09, SIGFPE,  "coprocessor segment overrun")
DOTRAP(0x0a, SIGSEGV, "invalid tss")
DOTRAP(0x0b, SIGSEGV, "segment not present")
DOTRAP(0x0c, SIGSEGV, "stack fault")
DOTRAP(0x0d, SIGSEGV, "general protection falut")
/* DOTRAP(0x0e, SIGSEGV, "pagefault") */
DOTRAP(0x0f, SIGSEGV, "intel reserved")
DOTRAP(0x10, SIGSEGV, "invalid TSS")
DOTRAP(0x11, SIGSEGV, "segment not present")
DOTRAP(0x12, SIGSEGV, "stack segment")

asmlinkage void dotrap0x07(ulong ebx)
{
	asm volatile ("clts; fwait");
	if (lastusedmath) {
		allege(curr != lastusedmath);
		asm volatile ("fnsave %0":"=m"(lastusedmath->i387));
	}
	if (curr->flags & TFUSEDMATH)
		asm volatile ("frstor %0"::"m"(curr->i387));
	else {
		asm volatile ("fninit");
		curr->flags |= TFUSEDMATH;
	}
	lastusedmath = curr;
}

asmlinkage void dotrap0x0e(ulong ebx)
{
	regs_t * r = (regs_t*) &ebx;
	ulong cr2;
	ulong e = r->u.errorcode;
	int u = e & PTEU;
	int w = e & PTEW;
	int p = e & PTEP;
	
	asm volatile("movl %%cr2,%0":"=r"(cr2):);
	if (!booting)
		return curr->mm->pagefault(r, cr2, u, w, p); 
	panic("boot time page fault:cr2:%lx,u:%d,w:%d,p:%d,eip:%lx\n",
	cr2, u, w, p, r->eip);
}

extern void prempt();
extern void dosoftirq();
extern void checksignal(regs_t *);
/* rettouser will be called at the end of system call and interrupt/exception handler */
asmlinkage void rettouser(ulong ebx)
{
	regs_t * r = (regs_t *) &ebx;

	assert(r->cs == UCODESEL);
	prempt();
	dosoftirq();
	checksignal(r);
}
