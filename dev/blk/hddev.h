#ifndef	_DEVBLKHD_H
#define	_DEVBLKHD_H

#include <fs/buf.h>
#include "dev.h"
#include "req.h"

struct hddev_t : public blkdev_t {
	isr_t curisr;

	void isr(int irqno);
	void nullisr(int irqno);
	void readisr(int irqno);
	void writeisr(int irqno);
	void setmultisr(int irqno);
	void rwsect(int cmd, int drv, int startsect, int nsect);

	hddev_t();
	~hddev_t();
	void docurreq();
	/* use default open */
	int ioctl(dev_t dev, int cmd, ulong arg);
	ulong getsize(dev_t dev);
};

#endif
