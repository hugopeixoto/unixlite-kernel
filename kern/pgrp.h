#ifndef _KERNPGRP_H
#define _KERNPGRP_H

#include <lib/queue.h>
#include "sched.h"
#include "signal.h"

struct session_t;
struct pgrp_t {
	CHAIN(,pgrp_t); /* next/prev member in the same session */
	CHAIN(hash,pgrp_t);
	pid_t pgid;
	Q(member,task_t) taskq;
	session_t * session;

	pgrp_t(session_t * s, task_t * t);
	~pgrp_t();
	void post(int signo);
	int kill(int signo);
	/* 1) A process grounp is not "orphaned" if in this grounp there exist 
	      at least one process whose parent is in a different process group 
	      which belongs to the same session.
	   2) Newly orphaned process grounps are to receive a SIGHUP and a 
	      SIGCONT. */
	int orphaned();
	void neworphaned();
};
QUEUE(,pgrp_t);
QUEUE(hash,pgrp_t);

struct tty_t;
struct session_t {
	CHAIN(,session_t);
	pid_t sid;
	task_t * leader;
	pgrp_t * fg; /* foreground pgrp */
	tty_t * tty; /* control term */
	Q(,pgrp_t) pgrpq;

	session_t(task_t * task);
	~session_t();
	void post(int signo);
	int kill(int signo);
	int getfg(pid_t * pgid);
	int setfg(pid_t pgid);
};
QUEUE(,session_t);

#endif
