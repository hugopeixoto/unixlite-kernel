#include <lib/root.h>
#include <lib/errno.h>
#include <lib/gcc.h>
#include <init/ctor.h>
#include <mm/allockm.h>
#include <dev/chr/tty.h>
#include "pgrp.h"

static Q(,session_t) allsessionq;
typedef Q(hash,pgrp_t) hashq_t; 
static int hsize, hmask;
static hashq_t * hashtab;

static inline hashq_t * hashfunc(pid_t pgid)
{
	return hashtab + (pgid & hmask);
}

__ctor(PRIKERN, SUBANY, pgrpsession)
{
	construct(&allsessionq);
	hsize = roundup2p2(nphysmeg * 8);
	hmask = hsize - 1;
	hashtab = (hashq_t*) allocbm(hsize * sizeof(hashq_t));
	construct(hashtab, hsize);
}

pgrp_t * findpgrp(pid_t pgid)
{
	pgrp_t * pgrp;
	hashq_t * hashq = hashfunc(pgid);
	foreach (pgrp, *hashq)
		if (pgrp->pgid == pgid)
			return pgrp;
	return NULL;
}

session_t * findsession(pid_t sid)
{
	session_t * s;
	foreach (s, allsessionq)
		if (s->sid == sid)
			return s;
	return NULL;
}

pgrp_t::pgrp_t(session_t * s, task_t * t)
{
	next = prev = NULL;
	s->pgrpq.enqtail(this);

	nexthash = prevhash = NULL;
	hashfunc(pgid)->enqtail(this);

	t->pgrp = this;
	taskq.enqtail(t);

	session = s;
	pgid = t->pid;
}

pgrp_t::~pgrp_t()
{
	unlink();
	unlinkhash();
}

tty_t * task_t::tty()
{
	return pgrp->session->tty;
}

session_t * task_t::session()
{
	return pgrp->session;
}

int task_t::leader()
{
	return this == pgrp->session->leader;
}

void task_t::enter(pgrp_t * pgrp_)
{
	assert(!pgrp);
	pgrp = pgrp_;
	pgrp->taskq.enqtail(this);
}

void task_t::leave()
{
	assert(pgrp);
	pgrp_t * g = pgrp;
	session_t * s = pgrp->session;
	pgrp = NULL;

	if (this == s->leader)
		s->leader = NULL;
	g->taskq.unlink(this);
	if (!g->taskq.empty())
		return;

	/* pgrp is empty */
	g->session = NULL;
	if (g == s->fg)
		s->fg = NULL;
	delete g;
	if (!s->pgrpq.empty())
		return;

	/* session is empty */
	delete s;
}

void pgrp_t::post(int signo)
{
	task_t * task;

	foreach (task, taskq)
		task->post(signo);
}

int pgrp_t::kill(int signo)
{
	task_t * task;
	int e = 0;
	
	foreach (task, taskq)
		e ? task->kill(signo) : (e = task->kill(signo));
	return e;
}

/* 1) A process grounp is not "orphaned" if in this grounp there exist 
      at least one process whose parent is in a different process group 
      which belongs to the same session.
   2) Newly orphaned process grounps are to receive a SIGHUP and a 
      SIGCONT. */
int pgrp_t::orphaned()
{
	task_t * task;

	foreach (task, taskq) {
		if (task->state == TZOMBIE)
			continue;
		if ((task->parent->pgrp->session == session) &&
		    (task->parent->pgrp != this))
			return 0;
	}
	return 1;
}

void pgrp_t::neworphaned()
{
	task_t * task;

	foreach (task, taskq) {
		if (task->state == TSTOP) {
			kill(SIGHUP);
			kill(SIGCONT);
		}
	}
}

session_t::session_t(task_t * t)
{
	assert(!t->pgrp);
	sid = t->pid;
	leader = t;
	new pgrp_t(this, t);
	fg = NULL;
	tty = NULL;
	next = prev = NULL;
	allsessionq.enqtail(this);
}

session_t::~session_t()
{
	assert(pgrpq.empty() && !leader && !fg);
	if (tty) tty->session = NULL, tty = NULL;
	allsessionq.unlink(this);
}

void session_t::post(int signo)
{
	pgrp_t * pgrp;

	foreach (pgrp, pgrpq)
		pgrp->post(signo);
}

int session_t::kill(int signo)
{
	pgrp_t * pgrp;
	int e = 0;

	foreach (pgrp, pgrpq)
		e ? pgrp->kill(signo) : (e = pgrp->kill(signo)); 
	return e;
}

int session_t::getfg(pid_t * pgid)
{
	if (!fg)
		return EINVAL;
	*pgid = fg->pgid;
	return 0;
}

int session_t::setfg(pid_t pgid)
{
	pgrp_t * pgrp;
	if (!(pgrp = findpgrp(pgid)))
		return ESRCH;
	fg = pgrp;
	return 0;
}

asmlinkage int syssetsid()
{
	if (lowfreepage())
		return ENOMEM;
	if (curr->pgrp->pgid == curr->pid)
		return EPERM;
	curr->leave();
	new session_t(curr);
	return curr->pid;
}

asmlinkage int sysgetsid(pid_t pid)
{
	if (!pid)
		return curr->pgrp->session->sid;
	task_t * task = findtask(pid);
	if (!task)
		return ESRCH;
	return task->pgrp->session->sid;
}

static int mychild(task_t * t)
{
	for (; t != &idletask; t = t->parent)
		if (t->parent == curr)
			return 1;
	return 0;
}

#warning "JJJJJJJJJJJJJJJJ"
asmlinkage int syssetpgid(pid_t pid, pid_t pgid)
{
	task_t * task;
	pgrp_t * pgrp;

	if (lowfreepage())
		return ENOMEM;
	if (pid == 0 || pid == curr->pid) {
		pid = curr->pid;
		task = curr;
	} else {
		task = findtask(pid);
		if (task == NULL)
			return ESRCH;
		if (!mychild(task) || task->pgrp->session != curr->pgrp->session)
			return EPERM;
	}
	if (task->pgrp->session->leader == task)
		return EPERM;
	if (pgid == 0)
		pgid = pid;
	if (pid == pgid) { /* create a new pgrp */
		pgrp = findpgrp(pgid);
		if (pgrp)
			return EPERM;
		session_t * s = task->pgrp->session;
		task->leave();
		new pgrp_t(s, task);
	} else { /* join an already exist pgrp */
		pgrp = findpgrp(pgid);
		if (pgrp == NULL)
			return ESRCH;
		if (task->pgrp == pgrp)
			return 0;
		task->leave();
		task->enter(pgrp);
	}
	return 0;
}

asmlinkage int sysgetpgid(pid_t pid)
{
	if (!pid)
		return curr->pgrp->pgid;
	task_t * task = findtask(pid);
	if (!task)
		return ESRCH;
	return task->pgrp->pgid;
}
