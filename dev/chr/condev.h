#ifndef	_CHRDEV_CONSOLE_H
#define _CHRDEV_CONSOLE_H

#include "tty.h"

typedef struct {
	char value;
	char attr;
	void erase() { value = ' '; attr = 0x07; }
} char_t;

struct inode_t;
struct fdes_t;
struct softirq_t;
struct condev_t : public tty_t {
	condev_t();
	int open(int flags, inode_t * inode, fdes_t ** fdes);
	/* vt100 state */
	enum {  NORMAL,
		ESC,			/* ESC */
		ESCBRACKET,		/* ESC [ */
		ESCBRACE,		/* ESC ( */
		ESCNUMBER, 		/* ESC # */
		ESCBRACKETQUEST,	/* ESC [ ? */
		GETPAR,
	};
	enum { NPAR = 4 };
	int addrport, valport;
	int npar;		/* number of parameter */
	int par[NPAR];		/* parameters */
	int aftergetpar;	/* state after get parameter */
	int savedx, savedy;
	int state;
	int nrow, ncolumn;
	int top, bottom;	/* scroll range */
	char_t * video, * evideo;
	char_t * org, * eorg;
	char_t * cursor;
	char curattr;

	int curx() { return (cursor - org) % ncolumn; }
	int cury() { return (cursor - org) / ncolumn; }
	char_t * currow() { return cursor - curx(); } 
	char_t * ecurrow() { return currow() + ncolumn; }
	int firstrow() { return curx() == 0; }
	int lastrow() { return cury() == nrow - 1; }
	void gotoxy(int newx, int newy);
	void updatecursor();
	void setorg();

	void beforegetpar(int next);
	void getpar(char c); 
	void normal(char c);
	void esc(char c);
	void escbracket(char c);
	void escbracketK();
	void escbracketJ();
	void escbracketL();
	void escbracketM();
	void escbracketP();
	void escbracketAT();
	void escbracketm();
	void escbracketr();
	void escbrace(char c);
	void escnumber(char c);
	void escbracketquest(char c);

	int write2();
	void directx(const char * str);
	void scrollup();
	void scrolldown();

	/* keyboard state */
	softirq_t * softirq;
	char capslock;
	char numlock;
	char scrolllock;
	char ctrling;
	char alting;
	char shifting;

	void initkbd();
	void kbdisr(int irqno);
	void nullkey(char code);
	void light();
	void capslockkey(char code);
	void numlockkey(char code);
	void scrollockkey(char code);
	void ctrlkey(char code);
	void altkey(char code);
	void shiftkey(char code);
	void charkey(char code);
	void stringkey(char code);
	void numpadkey(char code);
};
extern void sysbeep();
extern condev_t condev;

#endif
