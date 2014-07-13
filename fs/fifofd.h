#ifndef _FSFIFOFD_H
#define _FSFIFOFD_H

#include <kern/fdes.h>

struct fifofd_t : public fdes_t {
	~fifofd_t();
	int read(void * buf, int cnt);
	int write(void * buf, int cnt);
};
extern int openfifofd(int flags, inode_t * inode, fdes_t ** fdes);
#endif
