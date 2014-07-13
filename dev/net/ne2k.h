#ifndef _NE2000_H
#define _NE2000_H

/* The 8390 specific per-packet-header format. */
struct dp8390pkthdr_t {
        u8_t status;
        u8_t nextpage;
        u16_t totlen;
};

#include <net/lib/pkt.h>
#include "dev.h"

struct ne2kdev_t : public netdev_t {
        enum {  NEPG = 256 };

               	/* READ */                      /* WRITE */
        enum {  CR = 0x00,                      /* CR */
                PG0CLDA0 = 0x01,		PG0PSTART = 0x01,
		PG0CLDA1 = 0x02,		PG0PSTOP = 0x02,
                PG0BNRY = 0x03,                 /* PG0BNRY */
                PG0TSR = 0x04,			PG0TPSR = 0x04,   
		PG0NCR = 0x05,		        PG0TBCR0 = 0x05,
		PG0FIFO = 0x06,		        PG0TBCR1 = 0x06,
		PG0ISR = 0x07,		        /* PG0ISR */
                PG0CRDA0 = 0x08,                PG0RSAR0 = 0x08,
		PG0CRDA1 = 0x09,		PG0RSAR1 = 0x09,		
		/* reserve */                   PG0RBCR0 = 0x0a,
		/* reserve */                   PG0RBCR1 = 0x0b, 
		PG0RSR = 0x0c,		        PG0RCR = 0x0c,
		PG0CNTR0 = 0x0d,		PG0TCR = 0x0d,
		PG0CNTR1 = 0x0e,		PG0DCR = 0x0e,
		PG0CNT2 = 0x0f,		        PG0IMR = 0x0f,
		PG0DATA = 0x10,                 /* PG0DATA */

		PG1PAR0 = 0x01,                 /* PG1PAR0 */
		PG1CURR = 0x07,                 /* PG1CURR */
		PG1MAR0 = 0x08,                 /* PG1MAR0 */

		PG2PSTART = 0x01, 		PG2CLDA0 = 0x01,
		PG2PSTOP = 0x02,                PG2CLDA1 = 0x02,
		PG2TPSR = 0x04,                 /* reserve */
		PG2RCR = 0x0c,                  /* reserve */
		PG2TCR = 0x0d,                  /* reserve */
		PG2DCR = 0x0e,                  /* reserve */
		PG2IMR = 0x0f,                  /* reserve */
	};

	#define BIT8(b7, b6, b5, b4, b3, b2, b1, b0) enum { b0 = 0x01, b1 = 0x02, \
        b2 = 0x04, b3 = 0x08, b4 = 0x10, b5 = 0x20, b6 = 0x40, b7 = 0x80 }
	/* command register */
	BIT8(CRPS1, CRPS0, CRNODMA, CRWDMA, CRRDMA, CRTXP, CRSTA, CRSTP); 
	enum { CRPG0 = 0, CRPG1 = CRPS0, CRPG2 = CRPS1 | CRPS0, CRSNDPKT = CRWDMA|CRRDMA};
	/* interrupt status register */
	BIT8(ISRRST, ISRRDC, ISRCNT, ISROVW, ISRTXE, ISRRXE, ISRPTX, ISRPRX);
	/* interrupt mask register */
	BIT8(IMR___, IMRRDCE, IMRCNTE, IMROVWE, IMRTXEE, IMRRXEE, IMRPTXE, IMRPRXE);
	/* data config register */
	BIT8(DCR___, DCRFT1, DCRFT0, DCRARM, DCRLS, DCRLAS, DCRBOS, DCRWTS);
	/* transmit config register */
	BIT8(TCR__7, TCR__6, TCR__5, TCROFST, TCRATD, TCRLB1, TCRLB0, TCRCRC);
	/* receive config register */
	BIT8(RCR__7, RCR__6, RCRMON, RCRPRO, RCRAM, RCRAB, RCRAR, RCRSEP);
	/* transmit status register */
	BIT8(TSROWC, TSRCDH, TSRFU, TSRCRS, TSRABT, TSRCOL, TSR___, TSRPTX);
	/* receive status register */
	BIT8(RSRDFR, RSRDIS, RSRPHY, RSRMPA, RSRFO, RSRFAE, RSRCRC, RSRPRX);
	#undef BIT8

	pktq_t backlog;
        ulong iobase;
	uchar irqno;
        int txstartpg, txendpg; /* in unit of page */
        int rxstartpg, rxendpg; /* in unit of page */
	int rxpgnum;
        bool wordmode;
	int txing;

	ne2kdev_t(u8_t bus, u8_t dev);
        void command(u16_t value) { outbp(value, iobase + CR); }
        void select(u8_t page) { command(page | CRNODMA | CRSTA); }; 
	u8_t nein8(int reg) { return inbp(iobase + reg); }
	void neout8(u8_t value, int reg) { outbp(value, iobase + reg); }
	u8_t nein8(u8_t page, int reg) 
	{ 
		select(page);
		u8_t u8 = inbp(iobase + reg); 
		select(CRPG0);
		return u8;
	}
	void neout8(u8_t page, u8_t value, int reg) 
	{
		select(page);
		outbp(value, iobase + reg); 
		select(CRPG0);
	}
        u16_t nein16(int r0, int r1)
        {
                return inbp(iobase + r0) + (inbp(iobase + r1) << 8);
        }
        void neout16(u16_t value, int r0, int r1)
        {
                outbp(value & 0xff, iobase + r0);
                outbp(value >> 8, iobase + r1);
        }
	int waitrdc();
	void isr(int irqno);
	void isrptx();
	void isrprx();
        int remoteread(u16_t ringoffset, void *buf, int len);
        int remotereadrxring(u16_t ringoffset, void *buf, int len);
        int remotewrite(u16_t ringoffset, void *buf, int len);
	int hardwareoutput(pkt_t* pkt);
};


#endif /* _8390_h */
