#ifndef _BOOTPARAM_H
#define _BOOTPARAM_H

#define SECTSIZE 512
#define SECTBITS 9
/* PA stands for physical address */
#define SETUPSIZE 8192 /* not include the stack and initial page tables */
#define SETUPPA (640*1024 - SETUPSIZE - 4096*3)
#define SETUPSEG (SETUPPA/16)

/* the kernel is loaded into KERNPA0 in real mode, then
   move it to KERNPA1 in protected mode */
#ifdef __ASSEMBLY__
#define KERNSTART (0xc0000000)
#define KCODESTART (KERNSTART + 0x100000)
#else
#define KERNSTART (0xc0000000UL)
#define KCODESTART (KERNSTART + 0x100000UL)
#endif

#define KERNPA0 (1024*8)
#define KERNPA1 (1024*1024)
#define MAXKERNSIZE (640*1024 - KERNPA0 - SETUPSIZE - 4096*3)

#ifndef __ASSEMBLY__
struct hdgeo_t {
	unsigned short cyl;
	unsigned char head;
	unsigned short pad0;
	unsigned short wpcom;
	unsigned char pad1;
	unsigned char ctl;
	unsigned char pad2[3];
	unsigned short lzone;
	unsigned char sect;
	unsigned char pad3;
}__attribute__((packed));

struct bootparam_t {
	/* kernel size(has been rouned up to SECTSIZE), not include bss */
	unsigned long kernsize;
	unsigned long kerncksum;
	unsigned long kernentry;
	unsigned long rootdev;
	unsigned long extmemk;
	unsigned long ramdisksize;
	hdgeo_t hdgeo[2];
}__attribute__((packed));
extern bootparam_t bootparam;
extern int booting;
extern char bootstack[];

#endif
#define BOOTPARAMSIZE	(24+32)
#define BOOTPARAMPA (SETUPPA + SETUPSIZE - BOOTPARAMSIZE)
/* offset within the setup segment */
#define XBOOTPARAM	(BOOTPARAMPA - SETUPPA)
#define XKERNSIZE 	(XBOOTPARAM + 0)
#define XKERNCKSUM 	(XBOOTPARAM + 4)
#define XKERNENTRY	(XBOOTPARAM + 8)
#define XROOTDEV 	(XBOOTPARAM + 12)
#define XEXTMEMK 	(XBOOTPARAM + 16)
#define XRAMDISKSIZE 	(XBOOTPARAM + 20)
#define XHDDATA		(XBOOTPARAM + 24)
#define XPGDIR		(SETUPSIZE + 4096)
#define XPG0		(SETUPSIZE + 4096*2)
#define XINITIALSP	(SETUPSIZE + 4096*3) /* initial stack pointer */

#endif
