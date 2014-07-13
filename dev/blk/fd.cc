#include <lib/root.h>
#include <lib/errno.h>
#include <fs/inode.h>
#include <dev/blk/dev.h>
#include <dev/blk/fd.h> 

blkfd_t::~blkfd_t()
{
	inode->lose();
}

/* read - general block-dev read */
int blkfd_t::read(void * buf, int count)
{
	int e;

	count = max(count, blkdevsize - curpos);
	if ((e = inode->read(curpos, buf, count)) > 0)
		curpos += e;
	return e;
}

/* write - general block-dev write */
int blkfd_t::write(void * buf, int count)
{
	int e;

	count = max(count, blkdevsize - curpos);
	if ((e = inode->write(curpos, buf, count)) > 0)
		curpos += e;
	return e;
}

int blkfd_t::lseek(off_t off, int whence)
{
	off_t newpos;

	switch (whence) {
		case 0: newpos = off;
			break;
		case 1: newpos = curpos + off;
			break;
		case 2: newpos = blkdevsize + off; 
			break;
		default:
			return EINVAL;
	}
	if (newpos < 0)
		return EINVAL;
	curpos = newpos;
	return newpos;
}

int blkfd_t::ioctl(int cmd, ulong arg)
{
	return blkdev->ioctl(rdev, cmd, arg);
}

int blkfd_t::readdir(dirent_t * de, int count)
{
	return ENOTDIR;
}
