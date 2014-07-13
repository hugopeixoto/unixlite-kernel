#include <lib/root.h>
#include <lib/string.h>
#include <lib/errno.h>
#include <lib/gcc.h>
#include <boot/bootparam.h>
#include <asm/system.h>
#include <asm/seg.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <mm/allockm.h>
#include <mm/mm.h>
#include <kern/sched.h>
#include "ctor.h"

bootparam_t bootparam;
static tss_t boottss;
int booting = 1;

extern void bssinit();
extern void coninit();
extern void allobminit();
extern void kpgtblinit();
extern void i386init();
extern void pagemapinit();
extern void doglobalctors();
extern void freebm();
extern void installidletask();
extern void launchidletask();

extern void readparttab();
extern void mountroot();

extern "C" int main()
{
	bssinit();

	bootparam = *(bootparam_t*)BOOTPARAMPA;
        /* nphysbyte % 4M == 0 */
	nphysbyte = 1024*1024 + bootparam.extmemk*1024;
	nphysbyte = nphysbyte & 0xfff00000;
	nphyspage = pagediv(nphysbyte);
	nphysmeg = nphysbyte >> 20;

	coninit();
	printf("\033[1mUnixLite: A Light Weight Operating System Written in C++\n"
	"COPYRIGHT (C) 2005 NUAA CS DEPT.\033[m\n");

  printf("Total Memory Size is %d Meg\n", nphysmeg);
	i386init(); /* setup gdt,idt,cr0 */
	allocbminit();
	kpgtblinit(); /* setup cr3 */
	pagemapinit();
	allockminit();
	doglobalctors();
	freebm();
  printf("freebm\n");
	launchidletask();
  printf("pokemon\n");
	return 0;
}

asmlinkage int syssetup()
{
  printf("syssetup\n");
	allirqon();
	inittask = curr;
	allege(inittask->pid == 1);
	curr->execname.set("init");
	readparttab();
	mountroot();
	return 0;
}

asmlinkage int sysuname()
{
	return 0;
}

void launchidletask()
{
	task_t * i = curr = &idletask;
	tss_t * t = &i->tss;	

	construct(i);
	i->pid = 0;
	i->uid = i->euid = i->suid = 0;
	i->gid = i->egid = i->sgid = 0;
	i->execname.set("idle");

	i->state = TRUN;
	i->flags = 0;
	i->priority = 16;
	i->counter = 1;
	i->utick = i->stick = i->cutick = i->cstick = 0;

	i->mm = new mm_t(&t->esp, &t->eip);
	i->fs = new filesys_t();
	i->fdvec = new fdvec_t();
	i->sigvec = new sigvec_t();
	i->exitcode = 0;

	t->cs = UCODESEL;
	t->ds = t->es = t->ss = t->fs = t->gs = UDATASEL;
	t->eax = t->ebx = t->ecx = t->edx = t->esi = t->edi = t->ebp = 0x1979;

	t->ss0 = KDATASEL;
	t->esp0 = (vaddr_t)(i->mm->kstack + PAGESIZE);
	t->eflags = EFLAGIF;
	t->cr3 = vtop(i->mm->pgtbl);
	t->ldt = LDTSEL;

	t->backlink = 0;
	t->tracebitmap = 0x80000000;

	installidletask(); /* this must be done after counter is set */

	allege(*(ulong*)bootstack == 0x19790106);
	allege(!interruptflag());
	booting = 0;
	longjump(&boottss, t);
}
