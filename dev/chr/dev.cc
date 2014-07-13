#include <lib/root.h>
#include <fs/inode.h>
#include <kern/sched.h>
#include <kern/pgrp.h>
#include "dev.h"

chrdev_t * chrdevvec[MAXBLKDEV];

extern int openchrfd(int flags, inode_t * inode, fdes_t ** fdes)
{
	dev_t rdev = inode->getrdev();
	chrdev_t * chrdev = getchrdev(rdev);
	if (!chrdev) {
		warn("open invalid chrdev\n");
		return EINVAL;
	}
	if (major(rdev) == TTYMAJOR) { /* open the control term */
		chrdev = (chrdev_t*) curr->session()->tty; 
		if (!chrdev)
			return EPERM;
		rdev = CONSOLEMAJOR; /* ?? */
	}
	return chrdev->open(flags, inode, fdes);
}
