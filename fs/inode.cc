#include <lib/root.h>
#include <lib/errno.h>
#include <kern/sched.h>
#include "inode.h"
#include "stat.h"

int inode_t::denyunlink(inode_t * inode)
{
	assert(isdir());
	if (mode & SISVTX)
		return deny(IW);
	if (suser() || (curr->euid == uid) & (curr->euid == inode->uid))
		return 0;
	else
		return EACCES;
}

int inode_t::deny(mode_t request)
{
	mode_t granted = mode;
	
	/* if (suser()) return 0; */
	if (curr->euid == uid)
		granted >>= 6;
	else if (curr->egid == gid)
		granted >>= 3;
	granted &= 7;
	return ((request & granted) == request) ? 0 : EACCES;
}

int inode_t::denyuid(mode_t request)
{
	mode_t granted = mode;

	if (curr->uid == uid)
		granted >>= 6;
	else if (curr->gid == gid)
		granted >>= 3;
	return ((request & granted & 7) == request) ? 0 : EACCES;
}
