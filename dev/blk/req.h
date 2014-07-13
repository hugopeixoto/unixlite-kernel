#ifndef	_DEVBLKREQ_H
#define _DEVBLKREQ_H

#include <kern/sched.h>
#include <lib/queue.h>
#include <fs/buf.h>
#include <mm/allockm.h>

struct req_t {
	CHAIN(,req_t);
	int cmd;
	dev_t dev;
	int bsize;	/* all buf in this req must have same blk size */
	int sectperblk;
	bno_t startsect, nsect;
	Q(req,buf_t) bufq;
	buf_t * curbuf, * endbuf;
	char * cursect, * endsect;

	static int lt(req_t * a, req_t * b)
	{
		return a->startsect < b->startsect;
	}
	req_t(int rw, buf_t * b);
	~req_t();
	void setcur();
	int merge(int rw, buf_t * b);
	void error(int nsect_);
	void error() { error(nsect); }
	void advance(int nsect_);
	int more() { return cursect != NULL; };
	private: void nextsect(int state);
};
QUEUE(,req_t);
typedef Q(,req_t) reqq_t;

extern req_t * newreq(int rw, buf_t * b);
extern void zapreq(req_t * r);

struct scanreq_t {
	int nsect;
	buf_t * curbuf, * endbuf;
	char * cursect, * endsect;

	scanreq_t(req_t * req, int nsect_);
	int more() { return (nsect > 0) && cursect; };
	void next();
};

#define foreachsect(sect, req, nsect) \
for (scanreq_t __scanreq(req, nsect); \
     sect = __scanreq.cursect, __scanreq.more(); \
     __scanreq.next())

#endif
