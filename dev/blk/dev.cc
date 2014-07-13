#include <lib/root.h>
#include <asm/io.h>
#include <lib/ostream.h>
#include <fs/inode.h>
#include "dev.h"
#include "fd.h"

blkdev_t * blkdevvec[MAXBLKDEV];
static ulong nrsect, nrtime, nwsect, nwtime;

void dumpblkio(ostream_t * os)
{
	os->write("Read %d sectors in %d block io\n", nrsect, nrtime);
	os->write("Write %d sectors in %d block io\n", nwsect, nwtime);
}

blkdev_t::~blkdev_t()
{
}

int openblkfd(int flags, inode_t * inode, fdes_t ** fdes)
{
	blkdev_t * blkdev = getblkdev(inode->getrdev());
	if (!blkdev)
		return EINVAL;
	return blkdev->open(flags, inode, fdes);
}

int blkdev_t::open(int flags, inode_t * inode, fdes_t ** fdes)
{
	blkfd_t * f = new blkfd_t();
	f->type = BLKFD;
	f->fdflags = flags;
	f->refcnt = 1;
	(f->inode = inode)->hold();
	f->rdev = inode->getrdev();
	f->blkdev = this;
	f->blkdevsize = getsize(inode->getrdev());
	f->curpos = 0;
	*fdes = f;
	return 0;
}

#define X 0
void blkdev_t::addreq(int rw, buf_t * b)
{
	req_t * req;

	cli();
	allege(b->locked());
	foreach (req, reqq) {
		if ((req != curreq) && req->merge(rw, b)) {
			sti();
			return;
		}
	}
#if X 
	if (reqsema.wouldblock())/* start IO immeadiately */
		doreq();
	reqsema.down();
#endif
	reqq.insertlt(new req_t(rw, b));
	sti();
}

void blkdev_t::doreq(req_t * r)
{
	if (r->cmd == BREAD)
		nrtime++, nrsect += r->nsect;
	else
		nwtime++, nwsect += r->nsect;
	curreq = r;
	docurreq();
}

void blkdev_t::doreq()
{
	cli();
	if (reqq.empty() || curreq) {
		sti();
		return;
	}
	doreq(reqq.head());
	sti();
}

/* called during interrupt */
void blkdev_t::endcurreq()
{
	assert(!curreq->more());
	req_t * next = curreq->next;
	reqq.unlink(curreq);
	dispose(&curreq);
#if X
	reqsema.up();
#endif
	if (reqq.empty())
		return;
	doreq((next == reqq.vnode()) ? reqq.head() : next);
}
