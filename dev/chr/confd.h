#ifndef _DEVCHRCONFD_H
#define _DEVCHRCONFD_H

#include <kern/fdes.h>

struct condev_t;
struct confd_t : public fdes_t {
	condev_t * condev;

	~confd_t();
	int read(void * buf, int count);
	int write(void * buf, int count);
	int ioctl(int cmd, ulong arg);
};

#endif
