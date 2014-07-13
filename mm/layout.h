#ifndef	_MMLAYOUT_H
#define	_MMLAYOUT_H

/*
 * Assume physical memory is 256M, the memory lay out is following :
 * user area 	: from 0G + 000M to 3G + 000M
 * kern area 	: from 3G + 000M to 3G + 256M
 * hole      	: from 3G + 256M to 3G + 260M
 * kstk area 	: from 3G + 260M to 3G + 264M
 * hole      	: from 3G + 264M to 3G + 268M
 * vmalloc area : from 36 + 268M to 3G + 272M
 *
 * A KERNSTART of 0xC0000000 means that the kernel has
 * a virtual address spaddrce of one gigabyte, which limits the
 * amount of physical memory you can use to about 950MB. If
 * you want to use more physical memory, change this define.
 *
 * For example, if you have 2GB worth of physical memory, you
 * could change this define to 0x80000000, which gives the
 * kernel 2GB of virtual memory (enough to most of your physical memory
 * as the kernel needs a bit extra for vaddrrious io-memory mappings)
 *
 * IF YOU CHANGE THIS, PLEASE ALSO CHANGE
 *
 * Makefile
 *
 * which has the same constant encoded..
 */
#include <asm/page.h>
#include <boot/bootparam.h> 

#define MAXPHYSBYTE	(0x40000000UL - 0x04000000UL)
#define	USERSTART	0x0UL
#define USEREND		KERNSTART
#define KERNEND		(KERNSTART + nphysbyte)

#define	KSTACKSTART	(KERNSTART + MAXPHYSBYTE)
#define KSTACKEND	(KSTACKSTART + PDECOVERED)

#define	MAXKSTACKPAGE 2
#if (KSTACKSTART & (PDECOVERED-1) != 0)
#error "KSTACKSTART has wrong alignment"
#endif

#define	ALLOCVMSTART	(KSTACKEND + PDECOVERED)
#define ALLOCVMEND	(ALLOCVMSTART + PDECOVERED)
#if (ALLOCVMSTART & (PDECOVERED-1)!= 0)
#error "ALLOCVMSTART has wrong alignment"
#endif

#ifndef __ASSEMBLY__

extern char _start, __data_start, _edata, __bss_start, _end;
extern ulong nphyspage; /* number of total physical pages */
extern ulong nphysbyte; /* number of total physical bytes */
extern ulong nphysmeg;

/* convert paddr(physical address) to vaddr (virtual address) */
#define ptov(paddr) \
({ \
	ulong __paddr = (ulong)(paddr); \
	assert(__paddr <= nphysbyte); \
	__paddr + KERNSTART; \
})

/* convert vaddr (virtual address) to paddr(physical address) */
#define vtop(vaddr) \
({ \
	ulong __vaddr = (ulong)(vaddr); \
	assert((__vaddr >= KERNSTART) && (__vaddr < KERNEND)); \
	__vaddr - KERNSTART; \
})

/* fall in kern space */
#define kspace(vaddr) (((vaddr_t)(vaddr)) >= USEREND)

/* fall in user space */
#define uspace(vaddr) (((vaddr_t)(vaddr)) < USEREND)

#endif	/* __ASSEMBLY__ */

#endif
