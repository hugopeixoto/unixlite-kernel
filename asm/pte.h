#ifndef	_ASM_PTE_H
#define _ASM_PTE_H

#include "page.h"
#include <mm/allocpage.h>

/*
 * Page-directory and page-table entires follow this format, with a few
 * of the fields not present here and there, depending on a lot of things.
 */
#define	PTEP		0x001		/* P	present			*/
#define PTEW		0x002		/* W	writable		*/
#define PTEU		0x004		/* U   	user			*/
#define	PTEWT		0x008		/* WT	write through		*/
#define	PTECD		0x010		/* CD	cache disable		*/
#define PTEA		0x020		/* A	accessed		*/
#define	PTED		0x040		/* D	dirty			*/
#define	PTERESERVE0	0x080		/* PS	page size (0=4k,1=4M)	*/
#define	PTERESERVE1	0x100		/* G	global			*/
#define	PTEAVL0		0x200		/*  	available for system	*/
#define	PTEAVL1		0x400		/*   	programmers use		*/
#define	PTEAVL2		0x800		/*  				*/
#define PTEFRAME	PAGE1100	/* FRAME page frame address	*/

/* the following define only apply to usr page */
#define PROTCODE	(PTEU | PTEP)
#define PROTDATA	(PTEP)
#define PROTPGTBL	(PTEU | PTEW | PTEP)
typedef u32_t prot_t;

struct pte_t {
	u32_t bits;

	int none() { return !bits; }
	void clear() { bits = 0; }
	u32_t paddr() { return bits & PTEFRAME; }
	u32_t vaddr() { return ptov(paddr()); }
	void mapto(ulong pa, prot_t prot)
	{
		assert(!pagemod(pa));
		bits = pa + prot;
	}

	int  tstp() { return bits & PTEP; }
	void setp() { bits |= PTEP; }
	void clrp() { bits &= ~PTEP; }

	int  tstw() { return bits & PTEW; }
	void setw() { bits |= PTEW; }
	void clrw() { bits &= ~PTEW; }

	int  tstu() { return bits & PTEU; }
	void setu() { bits |= PTEU; }
	void clru() { bits &= ~PTEU; }

	int  tsta() { return bits & PTEA; }
	void seta() { bits |= PTEA; }
	void clra() { bits &= ~PTEA; }

	int  tstd() { return bits & PTED; }
	void setd() { bits |= PTED; }
	void clrd() { bits &= ~PTED; }
};

struct pde_t : public pte_t {
	pte_t * startpte()
	{
		assert(tstp());
		return (pte_t *) vaddr();
	}
	pte_t * endpte()
	{
		return startpte() + 1024;
	}
	pte_t * getpte(ulong va)
	{
		return startpte() + pteidx(va); 
	}
	int alloc()
	{
		ulong pa;

		if (!(pa = allocphys(AUSER | ACLEAR)))
			return 0;
		mapto(pa, PROTPGTBL);
		return 1;
	}
};

#endif
