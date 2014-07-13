#ifndef	_FSBUF_H
#define	_FSBUF_H

#include "minix.h"
#include <lib/queue.h>
#include <kern/sched.h>

/* buffer state */
#define BINVAL 0
#define BCLEAN 1
#define BDIRTY 2
#define BERROR 3 /* temporay state during io */

struct buf_t {
	CHAIN(all,buf_t);
	CHAIN(hash,buf_t);
	CHAIN(free,buf_t);	/* busy/free link */
	CHAIN(req,buf_t);	/* request queue */
	short state;
	short refcnt;
	dev_t dev;
	bno_t bno;		/* block number */
	int bsize;
	char * data;
	IOLOCK;

	int infreeq() { return nextfree != NULL; }
	void hold()
	{
		assert(refcnt >= 0);
		refcnt++;
	}
	void lose();
	void lose2() { state = BDIRTY; lose(); }
};
QUEUE(all,buf_t);
QUEUE(hash,buf_t);
QUEUE(free,buf_t);
QUEUE(req,buf_t);

extern buf_t * getb(dev_t dev, bno_t bno);
extern int readb(dev_t dev, bno_t bno, buf_t ** b);
extern void readab(dev_t dev, bno_t bno, int nr);
extern void syncb();
extern void syncb(dev_t dev);
extern inline int getbsize(dev_t dev) { return MINIXBSIZE; }
extern void setbsize(dev_t dev, int bsize);
extern void setbdevsize(dev_t dev, int * bsize);

enum {
	BREAD,
	BWRITE,
};

/* 1) llrwab stands for Low Level Read Write Asynchronously Buffer 
   2) caller must hold the buffer */
extern void llrwab(int rw, buf_t * b[], int nr);
extern inline void llreadab(buf_t * b[], int nr)
{
	llrwab(BREAD, b, nr); 
}
extern inline void llreadb(buf_t * b[], int nr)
{
	llrwab(BREAD, b, nr);
	for (int i = 0; i < nr; i++)
		b[i]->wait();
}
extern inline void llwriteab(buf_t * b[], int nr)
{
	llrwab(BWRITE, b, nr); 
}
extern inline void llwriteb(buf_t * b[], int nr)
{
	llrwab(BWRITE, b, nr);
	for (int i = 0; i < nr; i++)
		b[i]->wait();
}

#endif
