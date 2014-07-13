#include <lib/root.h>
#include <lib/printf.h>
#include <lib/gcc.h>
#include <lib/string.h>
#include <lib/ostream.h>
#include <kern/sched.h>
#include <kern/pgrp.h>
#include <kern/timer.h>
#include <mm/layout.h>
#include <asm/io.h>
#include <boot/bootparam.h>
#include "condev.h"
#include "confd.h"
#include "vga.h"

condev_t condev;
static int beeping;
static timer_t beeptimer;

static void sysbeepstop(void * data)
{
	/* disable counter 2 */
	outbp(inbp(0x61)&0xFC, 0x61);
	beeping = 0;
}

void coninit()
{
	construct(&condev);
	construct(&beeptimer);
	chrdevvec[CONSOLEMAJOR] = &condev;
	chrdevvec[TTYMAJOR] = &condev;
	/* construct(beeptimer); */
	beeptimer.init(_100ms, sysbeepstop, NULL);
}

void sysbeep()
{
	if (beeping) return;
	beeping = 1;
	/* enable counter 2 */
	outbp(inbp(0x61)|3, 0x61);
	/* set command for counter 2, 2 byte write */
	outbp(0xB6, 0x43);
	/* send 0x637 for 750 HZ */
	outbp(0x37, 0x42);
	outbp(0x06, 0x42);
	beeptimer.start();
}

static inline void erasechar(char_t * from, char_t * to)
{
	while (from < to)
		(from++)->erase();
}

static inline void movechar(char_t * from, char_t * to, int nchar)
{
	if (from < to) {
		from += nchar;
		to += nchar;
		while (nchar-->0)
			*--to = *--from;
	} else {
		while (nchar-->0)
			*to++ = *from++;
	}
}

int condev_t::open(int flags, inode_t * inode, fdes_t ** fdes)
{
	session_t * s = curr->pgrp->session;

	if (!s->tty) { // init task is not leader (s->leader == curr)) {
		s->tty = this;
		s->fg = curr->pgrp;
		s->tty->session = s;
	}
	confd_t * f = new confd_t();
	f->type = CHRFD;
	f->fdflags = flags;
	f->refcnt = 1;
	f->condev = this;
	*fdes = f;
	return 0;
}

void condev_t::beforegetpar(int next)
{
	state = GETPAR;
	aftergetpar = next;
	npar = 0;
	for (int i = 0; i < NPAR; i++)
		par[i] = 0;
}

void condev_t::getpar(char c)
{
	if ('0'<=c && c<='9') {
		par[npar] = 10*par[npar] + c - '0';
		return;
	}
	if (c == ';' && npar < NPAR) {
		npar++;
		return;
	}
	state = aftergetpar;
	outq.ungetc(c);
}

void condev_t::normal(char c)
{
	if (c == 27) { /* ESC */
		state = ESC;
		return;
	} 
	if ((32 <= c) && (c < 127)) {
		cursor->value = c;
		cursor->attr = curattr;
		if (++cursor == eorg)
			scrollup();
		return;
	} 
	if (c == 7) { /* BEL */
		sysbeep();
		return;
	} 
	if (c == 8) { /* BS */
		if (cursor != currow())
			--cursor;
		return;
	}
	if (c == 9) { /* HT */
		int gccbug = curx(); /* see bug/b.cc */
		cursor = min(eorg - 8, currow()+rounddown2(gccbug+8,8));
		return;
	}
	if (c == 10) { /* LF */
		if (lastrow())
			scrollup();
		cursor += ncolumn;
		return;
	}
	if (c == 13) { /* CR */
		cursor = currow();
		return;
	}
	if (c == 127) { /* DEL */
		if (cursor != currow())
			(--cursor)->erase();
		return;
	}
}

/*
 *  ESC D             cursor down - at bottom of region, scroll up
 *  ESC M             cursor up - at top of region, scroll down
 *  ESC E             next line (same as CR LF)
 *  ESC 7             save cursor position(char attr,char set,org)
 *  ESC 8             restore position (char attr,char set,origin)
 *  ESC =             keypad keys in applications mode
 *  ESC >             keypad keys in numeric mode
 *  ESC H             set tab in curr position
 *  ESC Z             request to identify terminal type
 *  ESC N             select G2 set for next character only
 *  ESC O             select G3 set for next character only
 +  ESC c             reset to initial state
 */

/* this is what the terminal answers to a ESC-Z or csi0c
   query (= vt100 response).  */
#define RESPONSE "\033[?1;2c"
void condev_t::esc(char c)
{
	if (c == '[') {
		beforegetpar(ESCBRACKET);
		return;
	}
	if (c == '(') {
		beforegetpar(ESCBRACE);
		return;
	}
	if (c == '#') {
		beforegetpar(ESCNUMBER);
		return;
	}
	state = NORMAL;
	if (c == 'E') {
		if (lastrow())
			scrollup();
		cursor = ecurrow();
		return;
	}
	if (c == 'M') {
		if (firstrow())
			scrolldown();
		cursor -= ncolumn;
		return;
	}
	if (c == 'D') {
		if (lastrow())
			scrollup();
		cursor += ncolumn;
		return;
	}
	if (c == 'Z') {
		int len = strlen(RESPONSE);
		if (rawq.space() < len)
			return;
		rawq.putc(RESPONSE, len);
	}
	if (c == '7') {
		savedx = curx();
		savedy = cury();
		return;
	}
	if (c == '8') {
		gotoxy(savedx, savedy);
		return;
	}
}

/*
 *  ESC [ m           turn off attributes - normal video
 *  ESC [ 0 m         turn off attributes - normal video
!*  ESC [ 4 m         turn on underline mode
 *  ESC [ 7 m         turn on inverse video mode
 *  ESC [ 1 m         highlight
 *  ESC [ 5 m         blink
 *  ESC [ g           clear tab stop in curr position
 *  ESC [ 0 g         clear tab stop in curr position
 *  ESC [ 3 g         clear all tab stops
 *  ESC [ 5 n         request for terminal status
    ESC [ 0 n         report - no malfunction
 *  ESC [ 6 n         request for cursor position report
    ESC [ pl;pc R     report - cursor at line pl, &#38; column pc
 *  ESC [ c           request to identify terminal type
 *  ESC [ 0 c         request to identify terminal type
 *  ESC [ i           print page
 *  ESC [ 0 i         print page
 *  ESC [ 1 i         print line
 +  ESC [ 4 i         print controller off
 +  ESC [ 5 i         print controller on
 +  ESC [ 2 ; 1 y     power up test
 +  ESC [ 2 ; 2 y     loop back test
 +  ESC [ 2 ; 9 y     power up test till failure or power down
 +  ESC [ 2 ; 10 y    loop back test till failure or power down
 +  ESC [ 2 0 h       newline mode LF, FF, VT, CR = CR/LF)
 +  ESC [ 2 0 l       line feed mode (LF, FF, VT = LF ; CR = CR)
!*  ESC [ 0 q         turn off LED 1-4
!*  ESC [ 1 q         turn on LED #1
!*  ESC [ 2 q         turn on LED #2
!*  ESC [ 3 q         turn on LED #3
!*  ESC [ 4 q         turn on LED #4
 */

/*  ESC [ K           erase to end of line (inclusive)
 *  ESC [ 0 K         erase to end of line (inclusive)
 *  ESC [ 1 K         erase to beginning of line (inclusive)
 *  ESC [ 2 K         erase entire line (cursor doesn't move) */
void condev_t::escbracketK()
{
	switch (par[0]) {
		case 0: erasechar(cursor, ecurrow());
			break;
		case 1: erasechar(currow(), cursor); 
			break;
		case 2: erasechar(currow(), ecurrow());
			break;
	}
}

/*  ESC [ J           erase to end of screen (inclusive)
 *  ESC [ 0 J         erase to end of screen (inclusive)
 *  ESC [ 1 J         erase to beginning of screen (inclusive)
 *  ESC [ 2 J         erase entire screen (cursor doesn't move) */
void condev_t::escbracketJ()
{
	switch (par[0]) {
		case 0: erasechar(cursor, eorg);
			break;
		case 1: erasechar(org, cursor);
			break;
		case 2: erasechar(org, eorg);
			break;
	}
}

static void insertchar(char_t * from, char_t * to, char_t * end)
{
	movechar(from, to, end - to);
	erasechar(from, to);
}

static void deletechar(char_t * from, char_t * to, char_t * end)
{
	movechar(to, from, end - to);
	erasechar(from + (end - to), end);
}

void condev_t::escbracketL()
{
	int nr = max(min(1, par[0]), bottom - cury());
	char_t * from = currow();
	char_t * to = from + nr * ncolumn;
	char_t * end = eorg;
	insertchar(from, to, end);
}

void condev_t::escbracketM()
{
	int nr = max(min(1, par[0]), bottom - cury());
	char_t * from = currow();
	char_t * to = from + nr * ncolumn;
	char_t * end = eorg;
	insertchar(from, to, end);
}

void condev_t::escbracketP()
{
	int nr = max(min(1, par[0]), ncolumn);
	char_t * from = cursor;
	char_t * to = from + nr;
	char_t * end = ecurrow();
	deletechar(from, to, end);
}

void condev_t::escbracketAT()
{
	int nr = max(min(1, par[0]), ncolumn);
	char_t * from = cursor;
	char_t * to = cursor + nr; 
	char_t * end = ecurrow();
	insertchar(from, to, end);
}

void condev_t::escbracketm()
{
	switch (par[0]) {
		case 0:curattr=0x07;break;
		case 1:curattr=0x0f;break;
		case 4:curattr=0x0f;break;
		case 7:curattr=0x70;break;
		case 27:curattr=0x07;break;
	}
}

/*  ESC [ pt ; pb r   set scroll region */
void condev_t::escbracketr()
{
	if (par[0]) par[0]--;
	if (!par[1]) par[1] = nrow;
	if (par[0] < par[1] && par[1] <= nrow) {
		top = par[0];
		bottom = par[1];
	}
}

void condev_t::escbracket(char c)
{
	if (c == '?') {
		beforegetpar(ESCBRACKETQUEST);
		return;
	}
	state = NORMAL;
	int x = curx(), y = cury();

	if (c == 'G' || c == '`') {
		if (par[0]) par[0]--;
		gotoxy(par[0],y);
		return;
	}
/*  ESC [ pn A        cursor up pn times - stop at top
 *  ESC [ pn B        cursor down pn times - stop at bottom
 *  ESC [ pn C        cursor right pn times - stop at far right
 *  ESC [ pn D        cursor left pn times - stop at far left */
	if (c == 'A') {
		if (!par[0]) par[0]++;
		gotoxy(x,y-par[0]);
		return;
	}
	if (c == 'B' || c == 'e') {
		if (!par[0]) par[0]++;
		gotoxy(x,y+par[0]);
		return;
	}
	if (c == 'C' || c == 'a') {
		if (!par[0]) par[0]++;
		gotoxy(x+par[0],y);
		return;
	}
	if (c == 'D') {
		if (!par[0]) par[0]++;
		gotoxy(x-par[0],y);
		return;
	}

	if (c == 'E') {
		if (!par[0]) par[0]++;
		gotoxy(0,y+par[0]);
		return;
	}
	if (c == 'F') {
		if (!par[0]) par[0]++;
		gotoxy(0,y-par[0]);
		return;
	}
	if (c == 'd') {
		if (par[0]) par[0]--;
		gotoxy(x,par[0]);
		return;
	}

/*  ESC [ pl ; pc H   set cursor position - pl Line, pc Column
 *  ESC [ H           set cursor home
 *  ESC [ pl ; pc f   set cursor position - pl Line, pc Column
 *  ESC [ f           set cursor home */
	if (c == 'H' || c == 'f') {
		if (par[0]) par[0]--;
		if (par[1]) par[1]--;
		gotoxy(par[1],par[0]);
		return;
	}
	if (c == 'J') {
		escbracketJ();
		return;
	}
	if (c == 'K') {
		escbracketK();
		return;
	}
	if (c == 'L') {
		escbracketL();
		return;
	}
	if (c == 'M') {
		escbracketM();
		return;
	}
	if (c == 'P') {
		escbracketP();
		return;
	}
	if (c == '@') {
		escbracketAT();
		return;
	}
	if (c == 'm') {
		escbracketm();
		return;
	}
	if (c == 'r') {
		escbracketr();
		return;
	}
	if (c == 's') {
		savedx = x;
		savedy = y;
		return;
	}
	if (c == 'u') {
		gotoxy(savedx, savedy);
		return;
	}
}

void condev_t::escbrace(char c)
{
}

void condev_t::escnumber(char c)
{
}

void condev_t::escbracketquest(char c)
{
}

/* call this function in emergency state! */
#warning "directx may overflow"
void condev_t::directx(const char * str)
{
	char oldattr = curattr;
	curattr = ERRORCHAR;
	for (; *str; str++, cursor++) {
		cursor->value = *str;
		cursor->attr = curattr;
	}
	curattr = oldattr;
}

int condev_t::write2()
{
	char c;

	while (outq.getc(&c)) {
		if (!between(org, cursor, eorg))
			panic("out of range:%lx %lx %lx\n", org, cursor, eorg);
		switch (state) {
			case NORMAL:
				normal(c);
				break;
			case ESC: /* ESC */
				esc(c);
				break;
			case ESCBRACKET: /* ESC [ */
				escbracket(c); 
				break;
			case ESCBRACE: /* ESC ( */
				escbrace(c);
				break;
			case ESCNUMBER: /* ESC # */
				escnumber(c);
				break;
			case ESCBRACKETQUEST: /* ESC [ ? */
				escbracketquest(c);
				break;
			case GETPAR:
				getpar(c);
				break;
		}
	}
	updatecursor();
	/* writerq.broadcast(); */
	return 0;
}

void condev_t::scrollup()
{
	if ((top == 0) && (bottom == nrow)) {
		if (eorg == evideo) {
			movechar(org, video, nrow * ncolumn);
			cursor = video + (cursor - org);
			org = video;
			eorg = video + nrow * ncolumn;
		}
		/* hardware scroll */
		org += ncolumn;
		eorg += ncolumn;
		setorg();
		erasechar(eorg-ncolumn, eorg);
		return;
	}
	char_t * from = org + top * ncolumn;
	char_t * to = from + ncolumn;
	char_t * end = org + bottom * ncolumn;
	deletechar(from, to, end);
}

void condev_t::scrolldown()
{
	if ((top == 0) && (bottom == nrow)) {
		if (org == video) {
			char_t * neworg = evideo - nrow * ncolumn;
			movechar(org, neworg, nrow * ncolumn);
			cursor = neworg + (cursor - org);
			org = neworg; 
			eorg = evideo;
		}
		/* hardware scroll */
		org -= ncolumn;
		eorg -= ncolumn;
		setorg();
		erasechar(org, org+ncolumn);
		return;
	}
	char_t * from = org + top * ncolumn;
	char_t * to = from + ncolumn;
	char_t * end = org + bottom * ncolumn;
	insertchar(from, to, end);
}

#define VGASIZE (32*1024)
condev_t::condev_t()
{
	addrport = 0x3d4;
	valport = 0x3d5;
	state = NORMAL;

	nrow = 25;
	ncolumn = 80;
	top = 0; 
	bottom = nrow;

	video = (char_t*) ptov(0xb8000);
	evideo = video + nrow * ncolumn * 8;
	org = video;
	eorg = org + nrow * ncolumn;
	cursor = org;
	curattr = 0x07;

	erasechar(org, eorg);
	initkbd();
}

void condev_t::gotoxy(int newx, int newy)
{
	if (!(0 <= newx) || !(newx < ncolumn) || !(0 <= newy) || !(newy < nrow))
		return;
	cursor = org + (newx + newy * ncolumn);
}

void condev_t::updatecursor()
{
	int offset = cursor - video;
	psw_t psw;
	psw.save(), cli();
	outbp(14, addrport);
	outbp(0xff&(offset>>8), valport);
	outbp(15, addrport);
	outbp(0xff&offset, valport);
	psw.restore();
}

void condev_t::setorg()
{
	psw_t psw;
	psw.save(), cli();
	outbp(12, addrport);
	outbp(0xff&((org - video)>>8), valport);
	outbp(13, addrport);
	outbp(0xff&(org - video), valport);
	psw.restore();
}

/* not include trailing zero */ 
static int print(const char *s, int length)
{
	static int printing;

	if (printing) {
		condev.directx(s);
		return -1;
	}
	printing = 1;
	condev.write((char*)s, length);
	printing = 0;
	return 0x19790106;
}

/* puts/putchar must be defined, gcc-3.2 will substiute 
 * printf into puts/putchar automatically */ 
asmlinkage int putchar(int c)
{
	char buf[2];
	buf[0] = c;
	buf[1] = 0;
	return print(buf, 1);
}

asmlinkage int puts(const char * s)
{
	print(s, strlen(s));
	putchar('\n');
	return 0;
}

void vprintf(const char * fmt, va_list ap)
{
	static char printbuf[1024];

	int length = vsprintf(printbuf, fmt, ap);
	printbuf[length] = 0;
	print(printbuf, length);
}

int printf(const char * fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(args);
	return 19790106;
}
