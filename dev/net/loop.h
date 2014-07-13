#ifndef	_NETLOOP_H
#define _NETLOOP_H

#include "dev.h"
struct loopnetdev_t : public netdev_t {
	loopnetdev_t();
	virtual ~loopnetdev_t();
	int output(pkt_t* pkt, u32_t daddr);
};

extern loopnetdev_t * loopnetdev;
#endif
