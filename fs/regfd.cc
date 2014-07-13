#include <lib/root.h>
#include <lib/errno.h>
#include <fs/inode.h>
#include "regfd.h"

regfd_t::~regfd_t()
{
	inode->lose();
}

int openregfd(int flags, inode_t * inode, fdes_t ** fdes)
{
	int e = 0;

	*fdes = NULL;
	if (inode->isdir() && (flags & FDWRITE))
		return EISDIR;
	if ((flags & OTRUNC) && (e = inode->truncate()))
		return e;
	regfd_t * r = new regfd_t();
	r->type = REGFD;
	r->fdflags = flags;
	r->refcnt = 1;
	(r->inode = inode)->hold();
	r->curpos = 0;
	*fdes = r;
	return 0;
}

int regfd_t::read(void * buf, int count)
{
	int e;

	if ((e = inode->read(curpos, buf, count)) > 0)
		curpos += e;
	return e;
}

int regfd_t::write(void * buf, int count)
{
	int e;

	assert(!inode->isdir());
	if (fdflags & FDAPPEND)
		curpos = inode->size;
	if ((e = inode->write(curpos, buf, count)) > 0)
		curpos += e;
	return e;
}

int regfd_t::lseek(off_t off, int whence)
{
	off_t newpos;

	switch (whence) {
		case 0: newpos = off;
			break;
		case 1: newpos = curpos + off;
			break;
		case 2: newpos = inode->size + off; 
			break;
		default:
			return EINVAL;
	}
	if (newpos < 0)
		return EINVAL;
	curpos = newpos;
	return newpos;
}

int regfd_t::ioctl(int cmd, ulong arg)
{
	return ENOTTY;
}

int regfd_t::getdents(dirent_t * de, int nbyte)
{
	return inode->getdents(&curpos, de, nbyte);
}
