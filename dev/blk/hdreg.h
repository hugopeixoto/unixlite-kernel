/*
 * This file contains some defines for the AT-hd-controller.
 * Various sources. Check out some definitions (see comments with
 * a ques).
 */
#ifndef _HDREG_H
#define _HDREG_H

/* Hd controller regs. Ref: IBM AT Bios-listing */
#define HDDATA		0x1f0	/* _CTL when writing */
#define HDERR		0x1f1	/* see err-bits */
#define HDNSECT		0x1f2	/* nr of sectors to read/write */

#define HDSECT		0x1f3	/* starting sector */
#define HDLCYL		0x1f4	/* starting cylinder */
#define HDHCYL		0x1f5	/* high byte of starting cyl */
#define HDDRVHEAD	0x1f6	/* 101dhhhh , d=drive, hhhh=head */

#define	HD0700	0x1f3
#define	HD1508	0x1f4
#define	HD2316	0x1f5
#define	HDDRV2724 0x1f6 

#define HDSTAT		0x1f7	/* see status-bits */
#define HDPRECOMP 	HDERR	/* same io address, read=error, write=precomp */
#define HDCMD     	HDSTAT	/* same io address, read=status, write=cmd */

/* Bits of HDSTATUS */
#define ERRSTAT		0x01
#define IDXSTAT		0x02
#define ECCSTAT		0x04	/* Corrected error */
#define DRQSTAT		0x08
#define SEEKSTAT 	0x10	/* seek complete, obsolete */
#define WRERRSTAT	0x20
#define RDYSTAT		0x40
#define BSYSTAT		0x80

/* Values for HDCOMMAND */
#define WINRESTORE	0x10
#define WINREAD		0x20
#define WINWRITE	0x30
#define WINVERIFY	0x40
#define WINFORMAT	0x50
#define WININIT		0x60
#define WINSEEK 	0x70
#define WINDIAGNOSE	0x90
#define WINSPECIFY	0x91

#define WINREADMULT	0xc4
#define WINWRITEMULT	0xc5
#define WINSETMULT	0xc6

/* Bits for HDERR */
#define MARKERR		0x01	/* Bad address mark ? */
#define TRK0ERR		0x02	/* couldn't find track 0 */
#define ABRTERR		0x04	/* ? */
#define IDERR		0x10	/* ? */
#define ECCERR		0x40	/* ? */
#define	BBDERR		0x80	/* ? */

#endif
