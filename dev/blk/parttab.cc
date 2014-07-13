#include <lib/root.h>
#include <dev/lib/root.h>
#include <fs/buf.h>
#include <boot/bootparam.h>
#include "parttab.h"

#define LINUXPART	0x83
#define LINUXSWAPPART 	0x82
#define MINIXPART	0x81	/* Minix partition type */
#define EXTPART		0x05	/* extended partition */
#define WIN95EXTPART	0x0f	/* window's 96 extended partition */ 

#define isext(type) ((type) == EXTPART || (type) == WIN95EXTPART)
#undef trace
#define trace(...)

/* MINOR : highest two bits(only use one bit) is drive number,
 	   next six bits is partion number */
/* major , drive, (primary, ..., extend, logic, ...) */
int hdnum = 1;
parttab_t parttab[2*PARTPERHD];

void parttab_t::dump()
{
#if 0
	if (!type)
		return;
	char * s;
	if (type == LINUXPART)
		s = "linux";
	else if (type == LINUXSWAPPART)
		s = "linux swap";
	else if (type == EXTPART)
		s = "extend";
	else if (type == MINIXPART)
		s = "minix";
	else if (type == 0x0b || type == 0x0c || type == 0x0d || type == 0x0e ||
		 type == 0x86 || type == 0x87)
		s = "msdos";
	else    s = "???";
	printf("%s:%08d-%08d(%d)\n", s, startsect, startsect + nsect, nsect);
#endif
}

static void readp(dev_t dev, ulong sectno, buf_t ** b, parttab_t ** p)
{
	int e;

	if (e= readb(dev, sectno / 2, b))
		panic("unable to read partition table\n");
	uchar * mbr = (uchar*)((*b)->data + (sectno&1)*512);
	if (mbr[510] != 0x55 || mbr[511] != 0xAA)
		panic("bad partition signature\n");
	*p = (parttab_t*)(mbr + 0x1be);
}

/*
 * Create devices for each logical partition in an extended partition.
 * The logical partitions form a linked list, with each entry being
 * a partition table with two entries.  The first entry
 * is the real data partition (with a start relative to the partition
 * table start).  The second is a pointer to the next logical partition
 * (with a start relative to the entire extended partition).
 * We do not create a Linux partition for the partition tables, but
 * only for the actual data partitions.
 */
static void extparttab(parttab_t * p, dev_t dev, parttab_t * ext)
{
	ulong extstart = ext->startsect;
	parttab_t * log; /* logic partition */
	ulong logstart = extstart;
	int more, nlog = 0;

	trace("EXTEND PARTTAB\n");
	do {
		buf_t * b;
		readp(dev, logstart, &b, &log);
		log->dump();
		more = log[1].type; 
 		/* relative to logic parttion */
		*p = log[0];
		p->startsect += logstart;
 		/* relative to entire extend parttion table */
		logstart = extstart + log[1].startsect;
		b->lose();
		p++;
		if (++nlog >= (PARTPERHD - 5)) {
			warn("too many logic partition\n");
			return;
		}
	} while (more);
}

static void priparttab(parttab_t * p, dev_t dev, parttab_t * pri)
{
	trace("PRIMARY PARTTAB\n");
	for (parttab_t * end = p + 4, * ext = NULL; p < end; p++, pri++) {
		if (!pri->type)
			continue;
		pri->dump();
		*p = *pri;
		if (!isext(p->type))
			continue;
		if (ext) panic("find two extend parttion\n");
		extparttab(end, dev, ext = pri);
	}
}

void readparttab()
{
	hdgeo_t * g = bootparam.hdgeo;
	parttab_t * p = parttab;
	for (int i = 0; i < hdnum; i++, g++, p += PARTPERHD) {
		p->startsect = 0;
		p->nsect = g->cyl * g->head * g->sect;
		printf("hd%c cyl:%d, head:%d, sect:%d (logical parameter)\n",
			'a' + i, g->cyl, g->head, g->sect);
	}
	buf_t * b;
	parttab_t * pri; 
	dev_t dev = mkdev(HDMAJOR, 0);
	for (p = parttab; p < parttab + hdnum * PARTPERHD; p += PARTPERHD) {
		readp(dev, 0, &b, &pri);
		priparttab(p + 1, dev, pri); 
		b->lose();
		dev += PARTPERHD; 
	}
}
