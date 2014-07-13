#include <lib/root.h>
#include <init/ctor.h>
#include <mm/allockm.h>
#include "sched.h"
#include "pgrp.h"

Q(all,task_t) alltaskq;
waitq_t zombietaskq;
static pid_t lastpid;
task_t idletask, * inittask, * lastusedmath;

static int hsize;
static int hmask;
typedef Q(hash,task_t) hashq_t;
static hashq_t * hashtab;
static inline hashq_t * hashfunc(pid_t pid)
{
	return hashtab + (pid & hmask);
}

__ctor(PRIKERN,SUBANY,taskdatabase)
{
	construct(&alltaskq);
	construct(&zombietaskq);

	hsize = min(64, roundup2p2(8 * nphysmeg));
	hmask = hsize - 1;
	hashtab = (hashq_t*) allocbm(hsize * sizeof(hashq_t));
	construct(hashtab, hsize);
}

void installidletask()
{
	task_t * i = &idletask;
	i->nextall = i->prevall = NULL;
	alltaskq.enqtail(i);
	i->nexthash = i->prevhash = NULL;
	hashfunc(i->pid)->enqtail(i);
	i->nextrun = i->prevrun = NULL;
	i->enqrun();
	i->nextmember = i->prevmember = NULL;
	new session_t(i);
	i->nextsibling = i->prevsibling = NULL;
	i->parent = NULL;
}

void task_t::install()
{
	nextall = prevall = NULL;
	alltaskq.enqtail(this);
	nexthash = prevhash = NULL;
	hashfunc(pid)->enqtail(this);
	nextrun = prevrun = NULL;
	enqrun();
	nextmember = prevmember = NULL;
	pgrp = NULL;
	enter(curr->pgrp);
	nextsibling = prevsibling = NULL;
	(parent = curr)->childq.enqtail(this);
}

void task_t::uninstall()
{
	allege(state == TZOMBIE);
	alltaskq.unlink(this);
	unlinkhash();
	zombietaskq.unlink(this);
	leave();
	allege(childq.empty());
	parent->childq.unlink(this);
	parent = NULL;
}

task_t * findtask(pid_t pid)
{
	task_t * t;

	foreach (t, *hashfunc(pid))
		if (t->pid == pid)
			return t;
	return	NULL;
}

pid_t newpid()
{
	for (;;)
		if (!findtask(++lastpid))
			return lastpid;
	return 0xdeadbeef;
}
