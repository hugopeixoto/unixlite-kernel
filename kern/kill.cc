#include <lib/root.h>
#include <lib/gcc.h>
#include <fs/inode.h>
#include "sched.h"
#include "pgrp.h"
#include "signal.h"

#undef trace 
#define trace(...)

void task_t::post(int signo)
{
	if ((this == &idletask) || (state == TZOMBIE))
		return;
	if ((this == inittask) && (signo != SIGCHLD))
		return;
	assert(validsig(signo));
	switch (signo) {
		case SIGKILL: case SIGCONT:
			sigset.del(SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU);
			break;
		case SIGSTOP: case SIGTSTP: case SIGTTIN: case SIGTTOU:
			sigset.del(SIGCONT);
			break;
	}
	sigfunc_t sf = sigvec->get(signo)->func;
	if ((sf == SIGXIGN) && (signo != SIGCHLD))
		return;
	if ((sf == SIGXDFL) && sigdflign(signo))
		return;	
	trace("post %d to %s\n", signo, execname.get());
	sigset.add(signo);
	if (!sigmask.contain(signo) && state == TWAIT)
		wakeup(); /* only wakeup interuptible task */
}

int task_t::kill(int signo)
{
	if (!validsig(signo))
		return EINVAL;
	if ((curr->euid != euid) && (curr->uid != uid) && !suser() &&
	    !(signo == SIGCONT && curr->pgrp->session == pgrp->session))
		return EPERM;
	post(signo);
	return 0;
}

void task_t::force(int signo)
{
	assert(validsig(signo));
	sigmask.del(signo);
	if (sigvec->get(signo)->func == SIGXIGN)
		sigvec->get(signo)->func = SIGXDFL;
	post(signo);
}

/* pid > 0 : some proc
   pid == 0 : any proc in the same pgrp as calling proc
   pid == -1 : any proc
   pid < -1 : any proc in the specified pgrp */
asmlinkage int syskill(pid_t pid, int signo)
{
	int e = 0;
	task_t * task;
	pgrp_t * pgrp;

	if (pid > 0)
		return (task = findtask(pid)) ? task->kill(signo) : ESRCH;
	if (pid == -1) {
		foreach(task, alltaskq)
			e ? task->kill(signo) : (e = task->kill(signo));
		return e;
	}
	if (pid == 0)
		return curr->pgrp->kill(signo);
	return (pgrp = findpgrp(-pid)) ? pgrp->kill(signo) : ESRCH;
}

asmlinkage int syswaitpid(pid_t pid, int * stat, int option)
{
	int e;
	int found = 0;
	task_t * c;
	waitq_t waitq; /* wait on anonymous waitq */

	if (stat && (e = verw(stat, sizeof(*stat))))
		return e;

repeat:	foreach (c, curr->childq) {
		if (pid > 0) { 
			if (c->pid != pid)
				continue;
		} else if (pid == 0) { 
			if (c->pgrp != curr->pgrp)
				continue;
		} else if (pid < -1) {
			if (c->pgrp->pgid != -pid)
				continue;
		}
		found = 1;
		switch (c->state) {
			case TSTOP:
				if (!(option & WUNTRACED))
					continue;
				if (!c->exitcode)/* status has been reported */
					continue;
				if (stat)
					*stat = c->exitcode;
				c->exitcode = 0;
				return c->pid;
			case TZOMBIE:
				pid_t cpid = c->pid;
				if (stat)
					*stat = c->exitcode;
				curr->cutick += c->utick;
				curr->cstick += c->stick;
				delete c;
				return cpid;
		}
	}
	if (!found)
		return ECHILD;
	if (option & WNOHANG)
		return 0;
	curr->flags |= TFWAITCHLDEXIT;
	waitq.wait();
	if (curr->flags & TFWAITCHLDEXIT) {
		curr->flags &= ~TFWAITCHLDEXIT;
		return EINTR;
	}
	/* if the flag TFWAITCHIDEXIT is cleared, we are waken up due 
	   to our child exit */
	curr->flags &= ~TFWAITCHLDEXIT;
	goto repeat;
}

/* under uclibc, wait is implented by calling wait4 */
asmlinkage int syswait4(pid_t pid, int * stat, int opt)
{
	return syswaitpid(pid, stat, opt);
}

asmlinkage int sysexit(int exitcode)
{
	doexit((exitcode&0xff)<<8);
}

static void notify(task_t * parent)
{
	parent->post(SIGCHLD);
	if (parent->flags & TFWAITCHLDEXIT) {
		parent->wakeup();
		parent->flags &= ~TFWAITCHLDEXIT;
	}
}

task_t::~task_t()
{
	allege(state == TZOMBIE);
	dispose(&mm);
	uninstall();
}

extern waitq_t zombietaskq;
__attribute__((noreturn)) void doexit(int exitcode)
{
	if (lastusedmath == curr)
		lastusedmath = NULL;
	curr->alarm.stop();
	curr->exitcode = exitcode;
	curr->mm->unmapuspace();
	curr->fdvec->closeall();
	dispose(&curr->fdvec);
	dispose(&curr->sigvec);
	if (curr->fs) {
		if (curr->fs->cwd)
			curr->fs->cwd->lose();
		if (curr->fs->root)
			curr->fs->root->lose();
		dispose(&curr->fs);
	}
	notify(curr->parent);
	task_t * c;
	foreachsafe(c, curr->childq) {
		curr->childq.unlink(c);
		inittask->childq.enqtail(c);
		c->parent = inittask;
		if (c->state == TZOMBIE || c->state == TSTOP)
			notify(inittask);
	}
	if (curr->pgrp->session->leader == curr)
		curr->pgrp->session->post(SIGHUP);
	if (lastusedmath == curr)
		lastusedmath = NULL;
	zombietaskq.zombie();
	allege(0);
}
