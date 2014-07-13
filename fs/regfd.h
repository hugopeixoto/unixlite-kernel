#ifndef _FSREGFD_H
#define _FSREGFD_H

#include <kern/fdes.h>
struct regfd_t : public fdes_t {
	inode_t * inode; /* inode maybe regular or dir */
	off_t curpos;

	~regfd_t();
	int read(void * buf, int count);
	int write(void * buf, int count);
	int lseek(off_t off, int whence);
	int ioctl(int cmd, ulong arg);
	int getdents(dirent_t * de, int nbyte); /* defined in dir.cc */
};
extern int openregfd(int flags, inode_t * inode, fdes_t ** fdes);
#endif
