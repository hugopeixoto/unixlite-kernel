#ifndef	_DEVCHRTTY_H
#define _DEVCHRTTY_H

#include <lib/fifoq.h>
#include <kern/timer.h>
#include <kern/sched.h>
#include "dev.h"
#include "termios.h"

/* need not cli()/sti() currently */
typedef fifoq_tl<512> ttycharq_t;
/*	intr=^C		quit=^|		erase=del	kill=^U
 *	eof=^D		vtime=\0	vmin=\1		sxtc=\0
 *	start=^Q	stop=^S		susp=^Z		eol=\0
 *	reprint=^R	discard=^U	werase=^W	lnext=^V
 *	eol2=\0 
 */

#define INIT_C_CC "\003\034\177\025\004\0\1\0\021\023\032\0\022\017\027\026\0"

struct session_t;
struct tty_t : public chrdev_t {
	int lnext;
	ulong iflag;		/* input flags */
	ulong oflag;		/* output flags */ 
	ulong cflag;		/* control flags */
	ulong lflag;		/* local mode flags */
	uchar line;		/* line discipine */
	uchar cc[NCCS];		/* control char */
	timer_t timer;
	ttycharq_t rawq;	/* raw input queue */ 
	ttycharq_t cookedq;	/* cooked input queue */ 
	ttycharq_t outq;	/* output queue */ 
	waitq_t readerq;	/* reader wait for more chars avaiable */
	waitq_t writerq;	/* writer wait for more space avaiable */
	session_t * session;

	tty_t();
	void notifyfg(int signo);
	int eol(char c) { return c == '\n' || c == cc[VEOL]; }
	int eof(char c) { return c == cc[VEOF]; }
	int sigchar(char c);
	int editchar(char c);
	void echoerase(char c);
	void flushin();
	void flushout();
	void drain();

	int gettermio(termio_t * t);
	int settermio(termio_t * t);
	int gettermios(termios_t * ts);
	int settermios(termios_t * ts);

	void putchar(char c);
	int waitchar(char * c);
	int waitchar(char * c, tick_t * left);
	int readline(char * buf, int len);
	int readchar(char * buf, int len);
	void opost(char c);
	void cook();
	int read(void * buf, int len);
	int write(void * buf, int len);
	virtual int write2() = 0;
	int ioctl(int cmd, ulong arg);
};

#endif
