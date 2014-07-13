#include <lib/root.h>
#include <lib/root.h>
#include <lib/ctype.h>
#include <kern/sched.h>
#include <kern/pgrp.h>
#include "tty.h"

/*	intr=^C		quit=^|		erase=del	kill=^U
	eof=^D		vtime=\0	vmin=\1		sxtc=\0
	start=^Q	stop=^S		susp=^Z		eol=\0
	reprint=^R	discard=^U	werase=^W	lnext=^V
	eol2=\0 */
#define ctrl(a) (1 + a - 'A')
tty_t::tty_t()
{
	lnext = 0;
	iflag = ICRNL;
	oflag = OPOST | ONLCR;
	cflag = 0;
	lflag = ICANON | ISIG | ECHO | ECHOE | ECHOKE;
	line = 0;
	cc[VINTR] = ctrl('C');
	cc[VQUIT] = 034; 
	cc[VERASE] = 127;
	cc[VKILL] = ctrl('U');
	cc[VEOF] = ctrl('D');
	cc[VTIME] = 0;
	cc[VMIN] = 1;
	cc[VSWTC] = 0;
	cc[VSTART] = ctrl('Q');
	cc[VSTOP] = ctrl('S');
	cc[VSUSP] = ctrl('Z');
	cc[VEOL] = 0;
	cc[VREPRINT] = 0;
	cc[VDISCARD] = ctrl('U');
	cc[VWERASE] = ctrl('W');
	cc[VLNEXT] = ctrl('V');
	cc[VEOL2] = 0;
	session = NULL;
}
#undef ctrl

void tty_t::notifyfg(int signo)
{
	if (session && session->fg)
		session->fg->post(signo);
}

void tty_t::flushin()
{
	rawq.clear();
	cookedq.clear();
	readerq.broadcast();
}

void tty_t::flushout()
{
	outq.clear();
	writerq.broadcast();
}

void tty_t::echoerase(char c)
{
	if (lflag & ECHO) {
		if (iscntrl(c))
			outq.putc(127);
		/* else if (c == '\t') */
		outq.putc(127);
		write2();
	}
}

int tty_t::sigchar(char c)
{
	if (lnext || !(lflag & ISIG))
		return 0;
	if (c == cc[VINTR]) {
		notifyfg(SIGINT);
		return 1;
	}
	if (c == cc[VQUIT]) {
		notifyfg(SIGQUIT);
		return 1;
	}
#if 0
	if (c == cc[VSUSP]) {
		notifyfg(SIGTSTP);
		return 1;
	}
#endif
	return 0;
}

int tty_t::editchar(char c)
{
	if (lnext || !(lflag & ICANON))
		return 0;
	if (c == cc[VERASE]) {
		if (cookedq.unputc(&c)) { 
			if (eol(c) || eof(c)) {
				cookedq.putc(c);
				return 1;
			}
			echoerase(c);
		}
		return 1;
	}
	if (c == cc[VWERASE]) {
		while (cookedq.unputc(&c)) {
			if (!(c == ' ' || c == '\t')) {
				cookedq.putc(c);
				break;
			}
			echoerase(c);
		}
		while (cookedq.unputc(&c)) { 
			if ((eol(c) || eof(c)) || c == ' ' || c == '\t') {
				cookedq.putc(c);
				return 1;
			}
			echoerase(c);
		}
		return 1;
	}
	if (c == cc[VKILL]) {
		while (cookedq.unputc(&c)) {
			if ((eol(c) || eof(c))) {
				cookedq.putc(c);
				return 1;
			}
			echoerase(c);
		}
		return 1;
	}
	return 0;
}

void tty_t::cook()
{
	char c;

	while (!cookedq.full() && rawq.getc(&c)) {
		if (c == cc[VLNEXT]) {
			lnext = 1;
			continue;
		}
		if (sigchar(c))
			continue;
		if (editchar(c))
			continue;
		if (c == '\r') {
			if (iflag & ICRNL) {
				c = '\n';
			}
		} else if (c == '\n') { 
			if (iflag & INLCR) {
				c = '\r';
			}
		}
		if (iflag & IUCLC)
			c = tolower(c);
		if (lflag & ECHO) {
			if (c == '\n') {
				outq.putc('\n');
				outq.putc('\r');
			} else if ((lflag & ECHOCTL) && iscntrl(c)) {
				outq.putc('^');
				outq.putc(c+32);
			} else
				outq.putc(c);
			write2();
		}
		cookedq.putc(c);
		if (lflag & ICANON) {
			if (eol(c) || eof(c))
				readerq.signal();
		} else {
			readerq.signal();
		}
		lnext = 0;
	}
}

int tty_t::waitchar(char * c)
{
	while (!cookedq.getc(c)) {
		readerq.wait();
		if (curr->hassig())
			return EINTR;
	}
	return 0;
}

int tty_t::waitchar(char * c, tick_t * left)
{
	while (!cookedq.getc(c)) {
		*left = readerq.wait(*left);
		if (curr->hassig())
			return EINTR;
	}
	return 0;
}

int tty_t::readline(char * buf, int len)
{
	int e;
	char c;
	char * b = buf, * end = buf + len; 

	for (;b < end; b++) {
		if ((e = waitchar(&c)) < 0)
			return e;
		if (eof(c))
			return b - buf;
	 	if (eol(c)) {
			*b++ = c;
			return b - buf;
		}
		if (eof(c))
			return b - buf;
		*b = c;
	}
	return b - buf; 
}

int tty_t::readchar(char * buf, int len)
{
	int e;
	char c;
	char * b = buf, * end = buf + len;
	tick_t left = time1totick(cc[VTIME]);

	for (; b < end; b++) {
		if (cookedq.getc(&c)) {
			*b = c;
			continue;
		}
		if ((b - buf)>=cc[VMIN])
			return b - buf;
		if (!cc[VTIME] || (cc[VMIN]&&(b == buf))) {
			if ((e = waitchar(&c)) < 0)
				return e;
			*b = c;
			continue;
		}
		if ((e = waitchar(&c, &left)) < 0)
			return e;
		if (!left)
			return b - buf;
		if (cc[VMIN]) /* reset the byte internal timer */
			left = time1totick(cc[VTIME]);
		*b = c;
	}
	return b - buf;
}

int tty_t::read(void * buf, int len)
{
	if (lflag & ICANON) 
		return readline((char*)buf, len);
	else
		return readchar((char*)buf, len);
}

#include "condev.h"
#include <lib/printf.h>
void tty_t::putchar(char c)
{
	if (!(oflag & OPOST))
		return;
	if (oflag & OLCUC)
		c = toupper(c);
	switch (c) {
		case '\n':
			if (oflag & ONLRET)
				c = '\r';
			if (oflag & ONLCR)
				outq.putc('\r');
			break;
		case '\r':
			if (oflag & OCRNL)
				c = '\n';
			break;
	}
	outq.putc(c);
}

int tty_t::write(void * buf, int len)
{
	char * b = (char*)buf, * end = (char*)buf + len;

	for (; b < end; ) {
		while ((outq.space() < 8)) {
			writerq.wait();
			if (curr->hassig())
				return EINTR;
		}
		while ((outq.space() >= 8) && (b < end))
			 putchar(*b++);
		write2();
	};
	return b - (char*)buf; 
}

int tty_t::gettermio(termio_t * t)
{
	t->iflag = iflag;
	t->oflag = oflag;
	t->cflag = cflag;
	t->lflag = lflag;
	t->line = line;
	for (int i = 0; i < NCC; i++)
		t->cc[i] = cc[i];
	return 0;
}

/*
 * This only works as the 386 is low-byt-first
 */
int tty_t::settermio(termio_t * t)
{
	iflag = t->iflag;
	oflag = t->oflag;
	cflag = t->cflag;
	lflag = t->lflag;
	line = t->line;
	for (int i = 0; i < NCC; i++)
		cc[i] = t->cc[i];
	return 0;
}

int tty_t::gettermios(termios_t * ts)
{
	ts->iflag = iflag;
	ts->oflag = oflag;
	ts->cflag = cflag;
	ts->lflag = lflag;
	ts->line = line;
	for (int i = 0; i < NCCS; i++)
		ts->cc[i] = cc[i];
	return 0;
}

int tty_t::settermios(termios_t * ts)
{
	iflag = ts->iflag;
	oflag = ts->oflag;
	cflag = ts->cflag;
	lflag = ts->lflag;
	line = ts->line;
	for (int i = 0; i < NCCS; i++)
		cc[i] = ts->cc[i];
	return 0;
}

int tty_t::ioctl(int cmd, ulong arg) 
{
	int e;
	ulong * p = (ulong*) arg;

	switch (cmd) {
		case TCGETS:
			if (e = verw(p, sizeof(termios_t)))
				return e;
			return gettermios((termios_t *)arg);
		case TCSETSF:
			flushin();
		case TCSETSW:
		case TCSETS:
			if (e = verr(p, sizeof(termios_t)))
				return e;
			return settermios((termios_t *)arg);

		case TCGETA:
			if (e = verw(p, sizeof(termio_t)))
				return e;
			return gettermio((termio_t *)arg);
		case TCSETAF:
			flushin();
		case TCSETAW:
		case TCSETA:
			if (e = verr(p, sizeof(termio_t)))
				return e;
			return settermio((termio_t *)arg);
		case TCSBRK:
			return EINVAL;
		case TCXONC:
			return EINVAL; /* not implemented */
		case TCFLSH:
			if (arg==0) 
				flushin();
			else if (arg==1)
				flushout();
			else if (arg==2) {
				flushin();
				flushout();
			} else
				return EINVAL;
			return 0;
		case TIOCEXCL:
			return EINVAL; /* not implemented */
		case TIOCNXCL:
			return EINVAL; /* not implemented */
		case TIOCSCTTY:
			return EINVAL; /* set controlling term NI */
		case TIOCGPGRP:
			if (e = verw(p, sizeof(*p)))
				return e; 
			if (!session || !session->fg)
				return EINVAL;
			return session->getfg((pid_t*)p);
		case TIOCSPGRP:
			if (e = verr(p, sizeof(*p)))
				return e;
			if (!session || !session->fg)
				return EINVAL;
			return session->setfg(*p);
		case TIOCOUTQ:
			if (e = verw(p, sizeof(*p)))
				return e;
			*p = outq.nchar(); 
			return 0;
		case TIOCINQ:
			if (e = verw(p, sizeof(*p)))
				return e;
			*p = rawq.nchar();
			return 0;
		case TIOCSTI:
			return EINVAL; /* not implemented */
		case TIOCGWINSZ:
			return EINVAL; /* not implemented */
		case TIOCSWINSZ:
			return EINVAL; /* not implemented */
		case TIOCMGET:
			return EINVAL; /* not implemented */
		case TIOCMBIS:
			return EINVAL; /* not implemented */
		case TIOCMBIC:
			return EINVAL; /* not implemented */
		case TIOCMSET:
			return EINVAL; /* not implemented */
		case TIOCGSOFTCAR:
			return EINVAL; /* not implemented */
		case TIOCSSOFTCAR:
			return EINVAL; /* not implemented */
	}
	return EINVAL; 
}
