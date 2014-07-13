#include <lib/root.h>
#include <lib/string.h>
#include <lib/gcc.h>
#include <lib/errno.h>
#include <lib/unistd.h>
#include <asm/frame.h>
#include <asm/sigcontext.h>
#include <asm/seg.h>
#include "sched.h"
#include "signal.h"

#undef debug
#define debug(...)

static char *signame[] = {
"ZERO", "HUP", "INT", "QUIT", "ILL", "TRAP", "IOT", "BUS",
"FPE", "KILL", "USR1", "SEGV", "USR2", "PIPE", "ALARM", "TERM",
"STKFLT", "CHLD", "CONT", "STOP", "TSTP", "TTIN", "TTOU", "URG",
"XCPU", "XFSZ", "VTALRM", "PROF", "WINCH", "IO", "PWR", "31"
};

void sigset_t::dump()
{
	printf("sigset contain:");
	for (int i = 1; i < 32; i++)
		if (contain(i))
			printf("SIG%s  ", signame[i]);
	printf("\n");
}

sigvec_t::sigvec_t()
{
	memset(this, 0, sizeof(*this));
}

sigvec_t * sigvec_t::clone()
{
	sigvec_t * x = new sigvec_t();
	for (int i = 0; i < NSIG; i++)
		x->vec[i] = vec[i];
	return x;
}

void sigvec_t::clearonexec()
{
	for (sigact_t * sa = vec, * end = vec + NSIG; sa < end; sa++) {
		sa->mask.clear();
		sa->flags = 0;
		if (sa->func != SIGXIGN)
			sa->func = NULL;
	}
}

asmlinkage int syssigprocmask(int how, sigset_t * set, sigset_t * oset)
{
	int e;

	if (oset) {
		if (e = verw(oset, sizeof(sigset_t)))
			return e;
		*oset = curr->sigmask;
	}
	if (e = verr(set, sizeof(sigset_t)))
		return e;
	if (set->invalid())
		return EINVAL;
	switch (how) {
		case SIGXBLOCK:
			curr->sigmask.add(set);
			break;
		case SIGXUNBLOCK:
			curr->sigmask.del(set);
			break;
		case SIGXSETMASK:
			curr->sigmask = *set;
			break;
		default:
			return EINVAL;
	}
	return 0;
}

asmlinkage int syssgetmask(void)
{
	return curr->sigset.bits; 
}

#define _mask(s) (1<<(s-1))
asmlinkage int sysssetmask(int newmask)
{
	if (newmask & (_mask(SIGKILL)|_mask(SIGSTOP)))
		return EINVAL;
	int oldmask = curr->sigmask.bits;
	curr->sigmask.bits = newmask;
	return oldmask; 
}
#undef _mask

asmlinkage int syssigpending(sigset_t * set)
{
	int e;

	if (e = verw(set, sizeof(sigset_t)))
		return e;
	curr->maskedsig(set);
	return 0;
}

#warning "obsolete sigsuspend"
/* uclibc/libc/linux/common/syscalls.c

   _syscall1(int, _sigsuspend, unsigned long int, mask);
   int sigsuspend (const sigset_t *set)
   {
	return _sigsuspend(set->__val[0]);
   } */

asmlinkage int syssigsuspend(ulong bits)
{
	int e;
	waitq_t waitq;
	sigset_t newmask;

	newmask.bits = bits;
	if (newmask.invalid())
		return EINVAL;
	sigset_t oldmask = curr->sigmask;
	curr->sigmask = newmask;
	waitq.wait();
	curr->sigmask = oldmask;
	return EINTR;
}

asmlinkage int syspause()
{
	waitq_t waitq;
	waitq.wait();
	return EINTR;
}

/*
 * POSIX 3.3.1.3:
 *  "Setting a signal action to SIG_IGN for a signal that is pending
 *   shall cause the pending signal to be discarded, whether or not
 *   it is blocked" (but SIGCHLD is unspecified: linux leaves it alone).
 *
 *  "Setting a signal action to SIG_DFL for a signal that is pending
 *   and whose default action is to ignore the signal (for example,
 *   SIGCHLD), shall cause the pending signal to be discarded, whether
 *   or not it is blocked"
 *
 * Note the silly behaviour of SIGCHLD: SIG_IGN means that the signal
 * isn't actually ignored, but does automatic child reaping, while
 * SIG_DFL is explicitly said by POSIX to force the signal to be ignored..
 */
static void checkpending(sigact_t * sa, int signo)
{
	if (sa->func == SIGXIGN) {
		if (signo == SIGCHLD)
			return;
		curr->sigset.del(signo);
		return;
	}
	if (sa->func == SIGXDFL) {
		if (!sigdflign(signo))
			return;
		curr->sigset.del(signo);
	}
}

static int verfunc(sigfunc_t func)
{
	if (func == SIGXIGN || func == SIGXDFL)
		return 0;
	return verr((void*)func, 1);
}

asmlinkage int syssignal(int signo, sigfunc_t func)
{
	int e;
	sigfunc_t ofunc;
	sigact_t * sa;

	if (!validsig(signo) || signo == SIGKILL || signo == SIGSTOP)
		return EINVAL; 
	if (e = verfunc(func)) 
		return e;
	sa = curr->sigvec->get(signo);
	ofunc = sa->func;
	sa->func = func;
	sa->mask.clear();
	sa->flags = SAONESHOT | SANOMASK;
	sa->restorer = NULL;
	checkpending(sa, signo);
	return (int)ofunc;
}

asmlinkage int syssigaction(int signo, sigact_t * act, sigact_t * oact)
{
	int e;
	sigact_t * sa;

	if (!validsig(signo) || signo == SIGKILL || signo == SIGSTOP)
		return EINVAL; 
	sa = curr->sigvec->get(signo);
	if (oact) {
		if (e = verw(oact, sizeof(sigact_t)))
			return e;
		*oact = *sa;
	}
	if (!act)
		return 0;
	if (e = verr(act, sizeof(sigact_t)))
		return e;
	if (e = verfunc(act->func))
		return e;
	if (act->mask.invalid())
		return EINVAL;
	*sa = *act;
	if (act->flags & SANOMASK)
		sa->mask.clear();
	else
		sa->mask.add(signo);
	checkpending(sa, signo);
	return 0;
}

static void ignore(int signo)
{
}

static void terminate(int signo)
{
	doexit(signo);
}

static void coredump(int signo)
{
	terminate(signo);
}

static void stop(int signo)
{
	waitq_t waitq;

	if (!(curr->parent->sigvec->get(SIGCHLD)->flags & SANOCLDSTOP))
		curr->parent->post(SIGCHLD);
	curr->exitcode = (signo << 8) | 0377;
	waitq.stop();
}

typedef void (*sigdflact_t) (int signo);
sigdflact_t sigdflact[NSIG] = {
ignore,			// SIGZERO	 	 0
terminate,		// SIGHUP		 1
terminate,		// SIGINT		 2
coredump,		// SIGQUIT		 3
coredump,		// SIGILL		 4
coredump,		// SIGTRAP		 5
coredump,		// SIGABRT SIGIOT	 6
ignore,			// SIGUNUSED	 	 7
coredump,		// SIGFPE		 8
terminate,		// SIGKILL		 9
terminate,		// SIGUSR1		10
coredump,		// SIGSEGV		11
terminate,		// SIGUSR2		12
terminate,		// SIGPIPE		13
terminate,		// SIGALRM		14
terminate,		// SIGTERM		15
terminate,		// SIGSTKFLT		16
ignore,			// SIGCHLD		17
ignore,			// SIGCONT		18
stop,			// SIGSTOP		19
stop,			// SIGTSTP		20
stop,			// SIGTTIN		21
stop,			// SIGTTOU		22
terminate,		// SIGIO SIGPOLL	23
ignore,			// SIGURG		SIGIO
coredump,		// SIGXCPU		24
coredump,		// SIGXFSZ		25
terminate,		// SIGVTALRM		26
ignore,			// SIGPROF		27
ignore,			// SIGWINCH		28
};

int sigdflign(int signo)
{
	return sigdflact[signo] == ignore;
}

asmlinkage int syssigreturn(ulong ebx)
{
	sigcontext_t * sc;
	regs_t * r;

	debug("%s sigreturn:\n", curr->execname.get());
	r = (regs_t*) &ebx;
	if (verr((void*) r->esp, sizeof(sigcontext_t)))
		doexit(SIGSEGV);
	sc = (sigcontext_t*) r->esp;
	curr->sigmask = sc->oldsigmask;
	curr->sigmask.del(SIGKILL, SIGSTOP);
	r->cs = UCODESEL;
	r->ss = r->ds = r->es = r->fs = r->gs = UDATASEL;
	if (verr((void*) sc->eip, 1))
		doexit(SIGSEGV);
	r->ebx = sc->ebx; /* may not work */
	r->ecx = sc->ecx;
	r->edx = sc->edx;
	r->esi = sc->esi;
	r->edi = sc->edi;
	r->esp = sc->esp;
	r->ebp = sc->ebp;
	r->eip = sc->eip;
	r->eflags = sc->eflags;
	r->u.origeax = NOTSYSCALLFRAME;
	return sc->eax;
}

static char asmcode[8] = {
	0x50, /* pushl %eax, push the fake return address */
	0xb8, __NR_sigreturn, 0x00, 0x00, 0x00, /* movl $NR_sigreturn,%eax */
	0xcd, 0x80 /* int $0x80 */
};
static ulong setupframe(regs_t * r, int signo, sigset_t * oldsigmask)
{
	sigcontext_t * sc;

	sc = (sigcontext_t *) (r->esp - sizeof(sigcontext_t));
	if (verw(sc, sizeof(sigcontext_t)))
		doexit(SIGSEGV);
	sc->retaddr = (ulong) &sc->asmcode[0];
	sc->signo = signo;
	sc->gs = r->gs;
	sc->fs = r->fs;
	sc->es = r->es;
	sc->ds = r->ds;
	sc->edi = r->edi;
	sc->esi = r->esi;
	sc->ebp = r->ebp;
	sc->esp = r->esp;
	sc->ebx = r->ebx;
	sc->edx = r->edx;
	sc->ecx = r->ecx;
	sc->eax = r->eax;
	/* sc->trapno = curr->tss.trapno; 
	   sc->errorcode = curr->tss.errorcode;
	   sc->cr2 = curr->tss.cr2; */
	sc->eip = r->eip; 
	sc->cs = r->cs;
	sc->eflags = r->eflags;
	sc->ss = r->ss;
	sc->oldsigmask = *oldsigmask; 
	memcpy(sc->asmcode, asmcode, 8);
	return (ulong)sc;
}

asmlinkage int syswaitpid(pid_t pid, int * stat,int flags);

/* action for SIGCHLD is strange: 
   SIGIGN means wait the child
   SIGDFL means ignore the signal */
void checksignal(regs_t * regs)
{
	int signo;
	sigact_t * sa;
	sigset_t active(0);
	curr->unmaskedsig(&active);

	if (curr == &idletask)
		return;
	while ((signo = active.del()) > 0) {
		debug("%s recive sig %d:\n", curr->execname.get(), signo);
		curr->sigset.del(signo);
		if ((curr == inittask) && (signo != SIGCHLD))
			continue;
		sa = curr->sigvec->get(signo);
		if (sa->func == SIGXIGN) {
			if (signo != SIGCHLD)
				continue;
			while (syswaitpid(-1,NULL,WNOHANG) > 0)/* reap child */
				/* nothing */;
			continue;
		}
		if (sa->func == SIGXDFL) {
			(sigdflact[signo])(signo);
			continue;
		}
		ulong esp = setupframe(regs, signo, &curr->sigmask);
		ulong eip = (ulong) sa->func;
		if (sa->flags & SAONESHOT)
			sa->func = SIGXDFL;
		curr->sigmask.add(&sa->mask);
		regs->esp = esp; 
		regs->eip = eip;
		return;
	}
}
