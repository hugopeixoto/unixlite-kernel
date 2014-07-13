#include <lib/root.h>
#include <lib/errno.h>
#include <lib/gcc.h>
#include <lib/ostream.h>
#include <kern/pgrp.h>
#include <kern/sched.h>
#include <net/inet/route.h>

static char * human(int state)
{
	if (state == TRUN)
		return "RUN";
	if (state == TWAIT)
		return "WAIT";
	if (state == TSLEEP)
		return "SLEEP";
	if (state == TSTOP)
		return "STOP";
	if (state == TZOMBIE)
		return "ZOMBIE";
	return "???";
}

static int ps(char * buf, int size)
{
	int e;

	if (e = verw(buf, size))
		return e;
	ostream_t os(buf, size);
	task_t * t;
	os.write("PPID\tPID\tPGID\tSID\tSTAT\tCOMMAND\n");
	foreach (t, alltaskq) {
		pid_t ppid = (t->parent ? t->parent->pid : -1);
		os.write("%d\t%d\t%d\t%d\t%s\t%s\n", ppid, t->pid,
		t->pgrp->pgid, t->pgrp->session->sid, human(t->state), 
		t->execname.get());
	}
	return os.written();
}

extern void dumpmem(ostream_t *);
extern void dumpkm(ostream_t *);
extern void dumpb(ostream_t *);
extern void dumpblkio(ostream_t *);
extern void dumpi(ostream_t *);
extern void dumpsched(ostream_t *);

static int dump(char * buf, int size)
{
	int e;

	if (e = verw(buf, size))
		return e;
	ostream_t os(buf, size);
	dumpmem(&os);
	dumpkm(&os);
	dumpb(&os);
	dumpblkio(&os);
	dumpi(&os);
	dumpsched(&os);
	return os.written();
}

asmlinkage int sysptrace(int req, char * buf, int size)
{
	if (req == 0)
		return ps(buf, size);
	if (req == 1)
		return dump(buf, size);
	if (req == 2)
		return showroute(buf, size);
	warn("invalid argument for ps(1) or ks(1) or route(8)\n");
	return ENOSYS;
}

asmlinkage int sysnosys(long ebx)
{
	regs_t * r = (regs_t *) &ebx;
	printf("THE NO.%ld SYSTEM CALL IS NOT IMPLENTED\n", r->u.origeax);
	return ENOSYS;
}

asmlinkage int syspersonality()
{
	return 0;
}

/* gcc need the following */
asmlinkage int syssetrlimit()
{
	return 0;
}

asmlinkage int sysgetrlimit()
{
	return 0;
}

asmlinkage int sysgetrusage()
{
	return 0;
}
