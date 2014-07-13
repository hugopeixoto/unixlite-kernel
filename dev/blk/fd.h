#ifndef _DEVBLKFD_H
#define _DEVBLKFD_H

#include <kern/fdes.h>

struct inode_t;
struct blkdev_t;
struct blkfd_t : public fdes_t {
	inode_t * inode;
	dev_t rdev;
	blkdev_t * blkdev;
	off_t blkdevsize;
	off_t curpos;

	~blkfd_t();
	int read(void * buf, int count);
	int write(void * buf, int count);
	int lseek(off_t off, int whence);
	int ioctl(int cmd, ulong arg);
	int readdir(dirent_t * de, int count);
};

#endif
