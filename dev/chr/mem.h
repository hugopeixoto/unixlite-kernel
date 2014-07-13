#ifndef _CHRDEVMEM_H
#define _CHRDEVMEM_H

#include "dev.h"

struct fdes_t; 
struct inode_t;
struct memdev_t : public chrdev_t {
	~memdev_t();
	int open(int flags, inode_t * inode, fdes_t ** fdes);
};

#endif
