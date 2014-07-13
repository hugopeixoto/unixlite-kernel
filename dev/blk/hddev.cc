#include <lib/config.h>
#include <lib/root.h>
#include <lib/string.h>
#include <init/ctor.h>
#include <asm/io.h>
#include <asm/irq.h>
#include "hddev.h"
#include "hdreg.h"
#include "parttab.h"

#undef trace 
#define trace(...)

#define LBA 1
#define DEFAULTMULTSECT 4
#define MAXHD 2
static int multcount[MAXHD];
static hddev_t hddev;

static inline int whichdrv(dev_t dev)
{
	return minor(dev)/PARTPERHD; 
}

hddev_t::hddev_t()
{
	curisr = &hddev_t::nullisr;
	allocirq(&hddev_t::isr, this, HDIRQ);
	/* bochs doesn't work if lack the following line,
	   see bochs/iodev/harddrv.cc */
	outbp(0,0x3f6);
}

hddev_t::~hddev_t()
{
}

__ctor(PRIDEV, SUBANY, harddisk)
{
	construct(&hddev);
	blkdevvec[HDMAJOR] = &hddev;
}

extern int readlatch();
static inline void readsect(char * buf)
{
	insw(HDDATA, buf, SECTSIZE/2);
}

static inline void writesect(char * buf)
{
	outsw(HDDATA, buf, SECTSIZE/2);
}

static char * string[] = {"err","idx","ecc","drq","seek","werr","rdy","bsy"};
static void showstat()
{
	int stat = inbp(HDSTAT);
	for (int i = 0; i < 8; i++, stat >>= 1)
		printf("%s:%s ", string[i], (stat & 1) ? "ON" : "__"); 
	printf("\n");
}

static int ioerror()
{
	int mask = BSYSTAT | RDYSTAT | SEEKSTAT | ERRSTAT;
	int good = RDYSTAT | SEEKSTAT;
	int num = 1024;
	while (num--)
		if ((inbp(HDSTAT) & mask) == good)
			return 0;
	showstat();
	return 1;
}

static void waitdrq()
{
	for(int i = 0; i < 1000; i++)
		if (inbp(HDSTAT) & DRQSTAT)
			return;
	showstat();
	warn("waitdrq failed\n");
}

static void waitrdy()
{
	int retry = 1024;

	while (retry--) {
		int stat = inbp(HDSTAT);
		if (!stat) return;
		if ((stat & (BSYSTAT|RDYSTAT)) == RDYSTAT) return;
	}
	showstat();
	warn("waitrdy failed\n");	
}

/* sector start from 1 not 0 */
void hddev_t::rwsect(int cmd, int drv, int s, int n)
{
	trace("hdrw: drv:%d startsect:%d nsect:%d\n", drv, s, n);
	waitrdy();
	outbp(s&0xff, HD0700);
	outbp((s>>8)&0xff, HD1508);
	outbp((s>>16)&0xff, HD2316);
	outbp(0xE0|(drv<<4)|((s>>24)&0x0f), HDDRV2724);
	outbp(n, HDNSECT);
	outbp(cmd, HDCMD);
}

void hddev_t::isr(int irqno)
{
	(*curisr)(this, irqno);
}

void hddev_t::nullisr(int irqno)
{
	warn("unexpected isr\n");
}

static int mmm = 1;
/* IO INT RD */ 
void hddev_t::readisr(int irqno)
{
	trace("readisr\n");

	assert(curreq && curreq->cmd == BREAD);
	if (ioerror()) {
		curreq->error(mmm);
		return;
	}
	waitdrq();
	char * sect;
	foreachsect(sect, curreq, mmm) {
		readsect(sect);
	}
	curreq->advance(mmm);
	if (!curreq->more()) {
		curisr = &hddev_t::nullisr;
		endcurreq();
	}
}

/* WR IO INT */
void hddev_t::writeisr(int irqno)
{
	trace("writeisr\n");
	assert(curreq && curreq->cmd == BWRITE);
	if (ioerror()) {
		curreq->error(mmm);
		return;
	}
	curreq->advance(mmm);
	if (!curreq->more()) {
		curisr = &hddev_t::nullisr;
		endcurreq();
		return;
	}
	waitdrq();
	char * sect;
	foreachsect(sect, curreq, mmm)
		writesect(sect);
}

void hddev_t::setmultisr(int irqno)
{
	int stat = inbp(HDSTAT);

	if (stat & (BSYSTAT | ERRSTAT)) {
		printf("harddisk set multiple mode failed\n");
		return;
	}
	int dev = 0;
	printf("hd%c enabled %d-sector multiple mode\n", 
		dev+'a', multcount[dev]);
}

void hddev_t::docurreq()
{
	int cmd, drv, s, n;
	parttab_t * p;

	if (curreq->cmd == BREAD) {
		cmd = WINREAD;
		curisr = &hddev_t::readisr;
	} else {
		cmd = WINWRITE;
		curisr = &hddev_t::writeisr;
	}

	dev_t dev = curreq->dev;
	if (minor(dev) >= MAXHD * PARTPERHD) {
		warn("invalid dev:%x\n", dev);
		goto error;
	}
	p = parttab + minor(dev);
	drv = whichdrv(dev);
	s = curreq->startsect + p->startsect;
	n = curreq->nsect;
	allege(n <= 255);
	if ((s < p->startsect) || (p->startsect + p->nsect < s + n)) {
		warn("start:%d,nsect:%d out of range on dev %x start:%ld,"
		"nsect:%ld)\n", s, n, curreq->dev, p->startsect, p->nsect);
		goto error;
	}

        rwsect(cmd, drv, s, n);
	if (cmd == WINWRITE) {
		waitdrq();
		char * sect;
		foreachsect(sect, curreq, mmm)
			writesect(sect);
	}
	return;

error:  curreq->error();
	endcurreq();
	curisr = &hddev_t::nullisr;
}

int hddev_t::ioctl(dev_t dev, int req, ulong arg)
{
	return -1;
}

ulong hddev_t::getsize(dev_t dev)
{
	if (minor(dev) > hdnum * PARTPERHD)
		return 0;
	return parttab[minor(dev)].nsect << SECTBITS;
}
