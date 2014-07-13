#ifndef	_DEVBLKDEV_H
#define _DEVBLKDEV_H

#include <dev/lib/root.h>
#include <kern/sched.h>
#include "req.h"

#define SECTSIZE 512
#define SECTBITS 9
#define MAXBUFPERIO 32

struct buf_t;
struct blkdev_t {
	sema_t reqsema;
	reqq_t reqq; /* requeset queue */
	req_t * curreq; /* current request */

	blkdev_t() : reqsema(MAXBUFPERIO * 2) { curreq = NULL; }
	virtual ~blkdev_t();
	void addreq(int rw, buf_t * b);
	void doreq(req_t * r);
	void doreq();
	void endcurreq();
	virtual void docurreq() = 0;
	virtual int open(int flags, inode_t * inode, fdes_t ** fdes);
	virtual int ioctl(dev_t dev, int cmd, ulong arg) = 0;
	virtual ulong getsize(dev_t dev) = 0;
};
	
extern blkdev_t * blkdevvec[MAXBLKDEV];
extern inline blkdev_t * getblkdev(dev_t dev)
{
	return (major(dev) < MAXBLKDEV) ?  blkdevvec[major(dev)] : NULL;
}
struct inode_t;
struct fdes_t;
extern int openblkfd(int flags, inode_t * inode, fdes_t ** fdes);
#endif
