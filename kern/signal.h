#ifndef _LINUX_SIGNAL_H
#define _LINUX_SIGNAL_H

#define _NSIG             32
#define NSIG		_NSIG

#define SIGHUP		 1
#define SIGINT		 2
#define SIGQUIT		 3
#define SIGILL		 4
#define SIGTRAP		 5
#define SIGABRT		 6
#define SIGIOT		 6
#define SIGBUS		 7

#define SIGFPE		 8
#define SIGKILL		 9
#define SIGUSR1		10
#define SIGSEGV		11
#define SIGUSR2		12
#define SIGPIPE		13
#define SIGALRM		14
#define SIGTERM		15

#define SIGSTKFLT	16
#define SIGCHLD		17
#define SIGCONT		18
#define SIGSTOP		19
#define SIGTSTP		20
#define SIGTTIN		21
#define SIGTTOU		22
#define SIGURG		23

#define SIGXCPU		24
#define SIGXFSZ		25
#define SIGVTALRM	26
#define SIGPROF		27
#define SIGWINCH	28
#define SIGIO		29
#define SIGPOLL		SIGIO
#define SIGPWR		30
#define	SIGUNUSED	31

#define SANOCLDSTOP	1
#define SASTACK		0x08000000
#define SARESTART	0x10000000
#define SAINTERRUPT	0x20000000
#define SANOMASK	0x40000000
#define SAONESHOT	0x80000000

#define SIGXBLOCK          0	/* for blocking signals */
#define SIGXUNBLOCK        1	/* for unblocking signals */
#define SIGXSETMASK        2	/* for setting the signal mask */
#define SIGXBLOCKABLE ~(1<<SIGKILL | 1<<SIGSTOP)

#define validsig(signo) ((signo)>0  && (signo)<NSIG)

struct sigset_t {
	ulong bits; /* this field should be private */

	sigset_t() { bits = 0; }
	sigset_t(int silly) { }
	void dump();
	void add(sigset_t * set) { bits |= set->bits; }
	void del(sigset_t * set) { bits &= ~set->bits; }
	int del() /* bit scan forward and clear */
	{
		if (!bits)
			return -1;
		int idx;
		asm ("bsfl %1,%0":"=r"(idx):"r"(bits));
		idx++;
		del(idx);
		return idx;
	}
	int empty() { return bits == 0; }
	void clear() { bits = 0; }
	int contain(int a) { return bits & (1UL << --a); }
	int invalid() { return contain(SIGKILL) || contain(SIGSTOP); }
	void add(int a)
	{
		bits |= (1UL << --a);
	}
	void add(int a, int b)
	{
		bits |= (1UL << --a) | (1UL << --b);
	}
	void add(int a, int b, int c)
	{
		bits |= (1UL << --a) | (1UL << --b) | (1UL << --c);
	}
	void add(int a, int b, int c, int d)
	{
		bits |= (1UL << --a) | (1UL << --b) | (1UL << --c) | (1UL << --d);
	}
	void del(int a)
	{
		bits &= ~(1UL << --a);
	}
	void del(int a, int b)
	{
		bits &= ~(1UL << --a) | ~(1UL << --b);
	}
	void del(int a, int b, int c)
	{
		bits &= ~(1UL << --a) | ~(1UL << --b) | ~(1UL << --c);
	}
	void del(int a, int b, int c, int d)
	{
		bits &= ~(1UL << --a) | ~(1UL << --b) | ~(1UL << --c) | ~(1UL << --d);
	}
};


/* Type of a signal func.  */
typedef void (*sigfunc_t)(int);
#define SIGXDFL	((sigfunc_t)0)	/* default signal handling */
#define SIGXIGN	((sigfunc_t)1)	/* ignore signal */
#define SIGXERR	((sigfunc_t)-1)	/* error return from signal */
struct sigact_t {
	sigfunc_t func;
	sigset_t mask;
	int flags;
	void (*restorer)(void);
};

extern int sigdflign(int signo); /* default action is ignore */

struct sigvec_t {
	sigact_t vec[NSIG];

	sigvec_t * clone();
	sigact_t * get(int signo) 
	{
		assert(validsig(signo));
		return vec + signo;
	}
	void clearonexec();
	sigvec_t();
};

/* The <sys/wait.h> header contains macros related to wait(). The value
 * returned by wait() and waitpid() depends on whether the process 
 * terminated by an exit() call, was killed by a signal, or was stopped
 * due to job control, as follows:
 *
 *				 High byte   Low byte
 *				+---------------------+
 *	exit(status)		|  status  |    0     |
 *				+---------------------+
 *      killed by signal	|    0     |  signal  |
 *				+---------------------+
 *	stopped (job control)	|  signal  |   0177   |
 *				+---------------------+
 */
#define _LOW(v)		( (v) & 0377)
#define _HIGH(v)	( ((v) >> 8) & 0377)

#define WNOHANG         1	/* do not wait for child to exit */
#define WUNTRACED       2	/* for job control; not implemented */

#define WIFEXITED(s)	(_LOW(s) == 0)			    /* normal exit */
#define WEXITSTATUS(s)	(_HIGH(s))			    /* exit status */
#define WTERMSIG(s)	(_LOW(s) & 0177)		    /* sig value */
#define WIFSIGNALED(s)	(((unsigned int)(s)-1 & 0xFFFF) < 0xFF) /* signaled */
#define WIFSTOPPED(s)	(_LOW(s) == 0177)		    /* stopped */
#define WSTOPSIG(s)	(_HIGH(s) & 0377)		    /* stop signal */

#endif
