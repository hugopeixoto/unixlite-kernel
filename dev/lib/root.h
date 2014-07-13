#ifndef _DEVLIBROOT_H
#define _DEVLIBROOT_H

/*
 * This file has definitions for major device numbers
 */

/* limits */

#define MAXCHRDEV 8 
#define MAXBLKDEV 8

/*
 * assignments
 *
 * devices are as follows (same as minix, so we can use the minix fs):
 *
 *      character              block                  comments
 *      --------------------   --------------------   --------------------
 *  0 - unnamed                unnamed                minor 0 = true nodev
 *  1 - /dev/mem               ramdisk
 *  2 -                        floppy
 *  3 -                        hd
 *  4 - /dev/tty*
 *  5 - /dev/tty; /dev/cua*
 *  6 - lp
 *  7 -                                               UNUSED
 *  8 -                        scsi disk
 *  9 - scsi tape
 * 10 - mice
 * 11 -                        scsi cdrom
 * 12 - qic02 tape
 * 13 -                        xt disk
 * 14 - sound card
 * 15 -                        cdu31a cdrom
 * 16 - sockets
 * 17 - af_unix
 * 18 - af_inet
 * 19 -                                               UNUSED
 * 20 -                                               UNUSED
 * 21 - scsi generic
 * 22 -                        (at2disk)
 * 23 -                        mitsumi cdrom
 * 24 -	                       sony535 cdrom
 * 25 -                        matsushita cdrom       minors 0..3
 * 26 -
 * 27 - qic117 tape
 */

#define NULLMAJOR	0
#define MEMMAJOR	1
#define FLOPPYMAJOR	2
#define HDMAJOR		3
#define TTYMAJOR	4
#define CONSOLEMAJOR	5
#define LPMAJOR		6

extern inline dev_t mkdev(int major, int minor)
{
	assert(major < MAXCHRDEV);
	return	(major << 8) + minor;
};

extern inline int major(dev_t dev)
{
	return  dev >> 8;
};
	
extern inline int minor(dev_t dev)
{
	return  dev & 0xff;
};

extern inline int validdev(dev_t dev)
{
	return major(dev) < MAXCHRDEV;
};

#endif
