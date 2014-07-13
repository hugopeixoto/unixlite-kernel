#include <lib/root.h>
#include <lib/string.h>
#include <init/ctor.h>
#include <kern/fdes.h>
#include <fs/inode.h>
#include <asm/io.h>
#include "mem.h"

struct memfd_t : public fdes_t {
	int read(void * buf, int count);
	int write(void * buf, int count);
};

struct kmemfd_t : public fdes_t {
	int read(void * buf, int count);
	int write(void * buf, int count);
	int lseek();
}; 

struct zerofd_t : public fdes_t {
	int read(void * buf, int count) { memset(buf, 0, count); return count; }
	int write(void * buf, int count) { return count; }
};

struct nullfd_t : public fdes_t {
	int read(void * buf, int count) { return 0; }
	int write(void * buf, int count) { return count; }
};

struct portfd_t : public fdes_t {
	int read(void * buf, int count);
	int write(void * buf, int count);
};

memdev_t::~memdev_t()
{
}

int memdev_t::open(int flags, inode_t * inode, fdes_t ** fdes)
{
	switch (minor(inode->getrdev())) {
		case 0: *fdes = NULL;
			return ENODEV; 
#if 0
		case 1: *fdes = new memfd_t(); 
			break;
		case 2: *fdes = new kmemfd_t();
			break;
#endif
		case 3: *fdes = new nullfd_t();
			break;
#if 0
		case 4: *fdes = new portfd_t();
			break;
#endif
		case 5: *fdes = new zerofd_t();
			break;
		default:return ENODEV;
	}
	(*fdes)->type = CHRFD;
	(*fdes)->fdflags = flags;
	(*fdes)->refcnt = 1;
	return 0;
}

static memdev_t memdev;

__ctor(PRIDEV,SUBANY,memdev)
{
	construct(&memdev);
	chrdevvec[MEMMAJOR] = &memdev;
}
