#ifndef _MMALLOCPAGE_H
#define _MMALLOCPAGE_H

#include "layout.h"

#define SYSPAGE -1979
struct page_t;
extern page_t * pagemap;
struct page_t {
	int refcnt;
	paddr_t paddr()
	{
		return pagemul(this - pagemap);
	}
	vaddr_t vaddr() { return ptov(paddr()); }
	void * data() { return (void *)vaddr(); }
};

extern inline page_t * patopage(paddr_t pa)
{
	assert(pa < nphysbyte);
	return pagemap + pagediv(pa);
}
extern inline page_t * vatopage(vaddr_t va)
{
	return patopage(vtop(va));
}

/* flags for allocpage */
#define AKERN	1	/* atomic and NEVER fail */
#define ADRIVER 2	/* atomic but may fail */
#define AUSER	4	/* non-atomic(currently is atomic) and may fail */
#define ACLEAR	8

#define KERNQUOTA 	4
#define DRIVERQUOTA 	12

extern void allocbminit();
extern void * allocbm(int size, int balign = sizeof(long));
extern void freebm();

extern void pagemapinit();
extern int lotsfreemem();
extern void * allocpage(int flags);
extern void freepage(void * page);

extern inline paddr_t allocphys(int flags)
{
	vaddr_t vaddr = (vaddr_t) allocpage(flags); 
	return vaddr ? vtop(vaddr) : 0;
}
extern inline void freephys(paddr_t paddr)
{
	freepage((void*)ptov(paddr));
}
extern inline void dupphys(paddr_t paddr)
{
#if DEBUG
	int ret = patopage(paddr)->refcnt;
	assert(ret != SYSPAGE);
#endif
	patopage(paddr)->refcnt++;
}
extern inline void duppage(void * page)
{
	dupphys(vtop((vaddr_t)page));
}

extern int lowfreepage(int request = 0);
extern void outofmem(int user);

#endif
