#include <lib/root.h>
#include <lib/gcc.h>
#include <lib/limits.h>
#include <dev/chr/dev.h>
#include <dev/blk/dev.h>
#include <kern/sched.h>
#include "inode.h"
#include "regfd.h"

static int doopen(int flags, inode_t * inode)
{
	int e = 0;
	fdes_t * fdes;

	if (flags & OAPPEND)
		flags |= FDAPPEND;
	if (flags & ONONBLOCK)
		flags |= FDNONBLOCK; /* not support yet */
	if (inode->isreg() || inode->isdir())
		e = openregfd(flags, inode, &fdes);
	else if (inode->isblk())
		e = openblkfd(flags, inode, &fdes);
	else if (inode->ischr())
		e = openchrfd(flags, inode, &fdes);
	else {
		warn("invalid mode,can't open inode(dev:%x,ino%x,mode:%x)\n", 
		inode->dev, inode->ino, inode->mode);
		return EINVAL; 
	}
	if (e)
		return e;
	e = curr->fdvec->put(fdes);
	if (e < 0)
		fdes->lose();
	return e;
}

/*
 * Note that while the flag value (low two bits) for sys_open means:
 *	00 - read-only
 *	01 - write-only
 *	10 - read-write
 *	11 - special
 * it is changed into
 *	00 - no permissions needed
 *	01 - read-permission
 *	10 - write-permission
 *	11 - read-write
 * for the internal routines (ie open_namei()/follow_link() etc). 00 is
 * used by symlinks.
 */
extern void dumpb();
asmlinkage int sysopen(const char * pathname, int flags, mode_t mode) 
{
	inode_t * dir = NULL, * leaf = NULL;
	char basename[NAMEMAX+1];
	int e = 0;

	flags++;
	if (!(flags & OCREAT)) {
		if (e = namei(ISTAT, pathname, &leaf))
			goto error;
		goto open;
	}
	/* OCREAT is set */
	if (e = namei(IMKNOD, pathname, &dir, basename))
		goto error;
	if (!(e = dir->mkreg(basename, mode, &leaf)))
		goto open;
	/* IO error */
	if (e != EEXIST)
		goto error;
	/* e == EEXIST */
	if (flags & OEXCL)
		goto error;
	if (e = dir->lookup(basename, &leaf))
		goto error;

open:	e = doopen(flags, leaf);
error:	if (dir) dir->lose();
	if (leaf) leaf->lose();
	return e;	
}

asmlinkage int syscreat(const char * pathname, int mode)
{
	return sysopen(pathname, OCREAT | OWRONLY | OTRUNC, mode);
}
