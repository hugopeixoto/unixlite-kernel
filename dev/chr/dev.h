#ifndef	_CHRDEV_H
#define _CHRDEV_H

#include <dev/lib/root.h>

struct inode_t;
struct fdes_t;
struct chrdev_t {
	virtual ~chrdev_t() {}
	virtual int open(int flags, inode_t * inode, fdes_t ** fdes) = 0;
};
extern chrdev_t * chrdevvec[MAXBLKDEV];
extern inline chrdev_t * getchrdev(dev_t dev)
{
	return (major(dev) < MAXCHRDEV) ?  chrdevvec[major(dev)] : NULL;
}
extern int openchrfd(int flags, inode_t * inode, fdes_t ** fdes);
#endif
