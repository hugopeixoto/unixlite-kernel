#include <lib/root.h> 
#include <init/ctor.h>
#include <asm/system.h>
#include "dev.h"
#include "req.h"

req_t::req_t(int rw, buf_t * b)
{
	next = prev = NULL;
	cmd = rw;
	dev = b->dev;
	bsize = b->bsize;
	sectperblk = bsize >> SECTBITS;
	startsect = b->bno * sectperblk;
	nsect = sectperblk; 
	bufq.enqtail(b);
	setcur();
}

void req_t::setcur()
{
	curbuf = bufq.head();
	endbuf = bufq.vnode();
	cursect = curbuf->data;
	endsect = curbuf->data + curbuf->bsize;
}

req_t::~req_t()
{
	buf_t * b;
	while (b = bufq.deqhead()) {
		if (b->state == BERROR)
			b->state = BINVAL;
		else
			b->state = BCLEAN;
		b->unlock();	
	}
}

/* if b has been merged into this req, return 1 */
int req_t::merge(int rw, buf_t * b)
{
	if ((rw != cmd) || (b->dev != dev) || (b->bsize != bsize))
		return 0;
	if (nsect + sectperblk > 255) {
		warn("exceed hardware limit\n");
		return 0;
	}
	assert(!b->nextreq && !b->prevreq);
	if (startsect + nsect == b->bno * sectperblk) {
		nsect += sectperblk;
		bufq.enqtail(b);
		setcur();
		return 1;
	}
	if (b->bno * sectperblk + sectperblk == startsect) {
		startsect -= sectperblk;
		nsect += sectperblk;
		bufq.enqhead(b);
		setcur();
		return 1;
	}
	return 	0;
}

void req_t::nextsect(int state)
{
	assert(more());
	if (state == BERROR)
		curbuf->state = BERROR;
	if ((cursect += SECTSIZE) != endsect)
		return;
	if ((curbuf = curbuf->nextreq) != endbuf) {
		cursect = curbuf->data;
		endsect = curbuf->data + curbuf->bsize;
		return;
	}	
	curbuf = endbuf = NULL;
	cursect = endsect = NULL;
}

void req_t::error(int nsect_)
{
	warn("io error\n");
	while ((nsect_-->0) && more())
		nextsect(BERROR);
}

void req_t::advance(int nsect_)
{
	while ((nsect_-->0) && more())
		nextsect(BCLEAN);
}

scanreq_t::scanreq_t(req_t * req, int nsect_)
{
	curbuf = req->curbuf;
	endbuf = req->endbuf;
	cursect = req->cursect; 
	endsect = req->endsect; 
	nsect = nsect_;
}

void scanreq_t::next()
{
	assert(more());
	if (!--nsect)
		return;
	if ((cursect += SECTSIZE) != endsect)
		return;
	if ((curbuf = curbuf->nextreq) != endbuf) {
		cursect = curbuf->data;
		endsect = curbuf->data + curbuf->bsize;
		return;
	}	
	curbuf = endbuf = NULL;
	cursect = endsect = NULL;
}
