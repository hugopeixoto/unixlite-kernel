#include <lib/root.h>
#include <lib/gcc.h>
#include <asm/seg.h>
#include <asm/frame.h>
#include <fs/inode.h>
#include <mm/allockm.h>
#include "sched.h"

filesys_t * filesys_t::clone()
{
	filesys_t * x = new filesys_t();
	x->umask = umask;
	if (x->root = root)
		root->hold();
	if (x->cwd = cwd)
		cwd->hold();
	return x; 
}

int task_t::clone(task_t ** result)
{
	int e;
	task_t * y = new task_t();
	*result = y;

	y->execname = execname;
	y->pid = newpid();
	y->pgrp = pgrp; /* join the pgrp later in install() */
	y->uid = uid;
	y->euid = euid;
	y->suid = suid;
	y->gid = gid;
	y->egid = egid;
	y->sgid = sgid;

	y->state = TRUN;
	y->flags = 0;
	y->priority = priority;
	y->counter = counter;

	y->starttick = passedtick;
	y->utick = y->stick = y->cutick = y->cstick = 0;
	if (e = mm->clone(&y->mm)) 
		return e;
	y->fs = fs->clone();
	y->fdvec = fdvec->clone();
	y->sigvec = sigvec->clone();
	y->sigset.clear();
	y->sigmask.clear();
	y->exitcode = 0;
	return 0;
}

static void copytss(regs_t * x, tss_t * y, paddr_t cr3, vaddr_t kstack)
{
	y->eax = 0; /* return value */ 
	y->ebx = x->ebx;
	y->ecx = x->ecx;
	y->edx = x->edx;
	y->esi = x->esi;
	y->edi = x->edi;
	y->esp = x->esp;
	y->ebp = x->ebp;
	y->eflags = x->eflags;
	y->eip = x->eip;

	y->ss0 = KDATASEL;
	y->esp0 = kstack + PAGESIZE;
	y->cs = UCODESEL;
	y->ds = y->es = y->ss = y->fs = y->gs = UDATASEL;

	y->cr3 = cr3;
	y->ldt = LDTSEL;
	y->backlink = 0;
	y->tracebitmap = 0x80000000;
}

extern void dump();
static int dofork(regs_t * regs);
/* sysfork(regs_t regs) may not work */
asmlinkage int sysfork(ulong ebx)
{
	if (lowfreepage(8))
		return ENOMEM;
	return dofork((regs_t*)&ebx);
}

static int dofork(regs_t * regs)
{
	int e;
	task_t * child;

	if (lowfreepage(8))
		return ENOMEM;
	if (e = curr->clone(&child))
		return e;
	copytss(regs, &child->tss, vtop(child->mm->pgtbl), (vaddr_t)child->mm->kstack); 
	child->install();
	return child->pid;
}
