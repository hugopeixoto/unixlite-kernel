#ifndef _BLKDEVPARTTAB_H
#define _BLKDEVPARTTAB_H

struct parttab_t {
	u8_t bootflag;		/* 0x80 - active */
	u8_t head;		/* starting head */
	u8_t sect;		/* starting sector */
	u8_t cyl;		/* starting cylinder */
	u8_t type;		/* What partition type */
	u8_t ehead;		/* end head */
	u8_t esect;		/* end sector */
	u8_t ecyl;		/* end cylinder */
	u32_t startsect;	/* starting sector counting from 0 */
	u32_t nsect;		/* nr of sectors in partition */

	void dump();
};
#define PARTPERHD 64
extern int hdnum;
extern parttab_t parttab[];
extern void readparttab();

#endif
