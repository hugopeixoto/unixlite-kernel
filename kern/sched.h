#ifndef	_KERNSCHED_H
#define	_KERNSCHED_H

#include <lib/queue.h>
#include <lib/string.h>
#include <asm/seg.h>
#include <asm/pgtbl.h>
#include <mm/mm.h>
#include "time.h"
#include "fdes.h"
#include "signal.h"
#include "timer.h"

/* task state */
enum {
	TRUN,
	TSLEEP,
	TWAIT, /* interruptible sleep  */
	TZOMBIE, /* interruptible sleep  */
	TSTOP,
};

/* task flags */
enum {
	TFWAITCHLDEXIT = 0x01,
	TFUSEDMATH = 0x02,
};

struct filesys_t {
	ushort umask;
	inode_t * root, * cwd;
	filesys_t * clone();

	filesys_t() { umask = 066; root = cwd = NULL; }
}; 

struct mm_t;
struct tty_t;
struct pgrp_t;
struct session_t;
struct inode_t;
struct task_t;
typedef queue_tl<0,sizeof(void*),task_t> Q(sibling,task_t);
struct task_t {
	/* various link */
	CHAIN(sibling,task_t);  /* this field must be first */
	Q(sibling,task_t) childq;
	CHAIN(all,task_t);
	CHAIN(hash,task_t); 	/* pid hash queue */
	CHAIN(run,task_t); 	/* run/sleep queue */
	CHAIN(member,task_t);
	task_t * parent;
	pgrp_t * pgrp;

	/* id */
	pid_t pid;
	uid_t uid, euid, suid;
	gid_t gid, egid, sgid;
	str_tl<16> execname;

	/* sched data */
	volatile int state;
	int flags;
	int priority;
	int counter;
	timer_t alarm;
	tick_t utick, stick, cutick, cstick, starttick;

	mm_t * mm;
	filesys_t * fs;
	fdvec_t * fdvec;
	sigset_t sigset, sigmask;
	sigvec_t * sigvec;
	int exitcode;
	tss_t tss;
	i387_t i387;

	tty_t * tty();
	session_t * session();
	int leader();
	void enter(pgrp_t * pgrp);
	void leave();
	int clone(task_t ** result);
	~task_t();
	void setcounter(int newval);
	void enqrun();
	void deqrun();
	void wakeup();
	void install();
	void uninstall();
	int kill(int signo);
	void post(int signo);
	void force(int signo);
	void maskedsig(sigset_t * masked)
	{
		masked->bits = sigset.bits & sigmask.bits;
	}
	void unmaskedsig(sigset_t * unmasked)
	{
		unmasked->bits = sigset.bits & ~sigmask.bits;
	}
	int hassig()
	{
		sigset_t unmasked(0);
		unmaskedsig(&unmasked);
		return !unmasked.empty();
	}
};

QUEUE(all,task_t);
QUEUE(hash,task_t);
QUEUE(run,task_t);
QUEUE(member,task_t);

extern task_t *	curr; /* current task */
extern task_t idletask, * inittask, * lastusedmath;
extern Q(all,task_t) alltaskq;

#include "lock.h"

extern inline int suser()
{
	return curr->euid == 0;
}

extern inline int fdtofdes(int fd, fdes_t ** fdes)
{
	return curr->fdvec->get(fd, fdes);
}

extern void sched();
extern void prempt();
extern void schedinit();

extern void doexit(int signo) __attribute__((noreturn));
extern Q(all,task_t) alltaskq;
extern task_t * findtask(pid_t pid);
extern pgrp_t * findpgrp(pid_t pgid);
extern session_t * findsession(pid_t sid);
extern pid_t newpid();


#endif
