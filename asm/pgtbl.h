#ifndef	_ASM_PGTBL_H
#define _ASM_PGTBL_H

#include <asm/system.h>
#include "pte.h"
struct pgtbl_t {
	pde_t pgd[1024];

	pde_t * getpde(ulong vaddr) { return pgd + pdeidx(vaddr); }
	int present(ulong vaddr);
	pte_t * getpte(ulong vaddr);
	pte_t * probe(ulong vaddr);
	ulong explore(ulong from, ulong to, pde_t ** pdep, pte_t ** ptep);
	int none(ulong from, ulong to);
	int alloctree(ulong from, ulong to);
	void freetree(ulong from, ulong to);
	void freetree();
	void initkpgtbl();
	void sharekpgtbl();

	void * operator new(size_t size);
	void operator delete(void * p);
};
extern pgtbl_t kpgtbl;

extern inline pte_t * pgtbl_t::getpte(ulong vaddr)
{
	pde_t * pde;

	pde = getpde(vaddr);
	if (!pde->tstp() && !pde->alloc())
		return NULL;
	return pde->getpte(vaddr);
}

extern inline pte_t * pgtbl_t::probe(ulong vaddr)
{
	pde_t * pde;

	pde = getpde(vaddr);
	if (!pde->tstp())
		return NULL;
	return pde->getpte(vaddr);
}

extern inline int pgtbl_t::present(ulong vaddr)
{
	pte_t * pte = probe(vaddr);
	return pte ? pte->tstp() : 0;
}

/* scan the page table "tree" */
class scanpgtbl_t {
	pgtbl_t * pgtbl;
	pde_t * pde;	/* current pde */
	pte_t * pte;	/* current pte */
	ulong curpos, end;
public:
	ulong cursor() { return curpos; }
	pte_t * curpte() { return pte; }
	scanpgtbl_t(pgtbl_t * pgtbl_, ulong from, ulong to);
	void next();
	int more() { return curpos < end; }
};

#define foreachpte(pte, pgtbl, from, to) \
for (scanpgtbl_t scan(pgtbl, from, to); \
     pte = scan.curpte(), scan.more(); scan.next())

#define foreachpte2(cursor, pte, pgtbl, from, to) \
for (scanpgtbl_t scan(pgtbl, from, to); \
     cursor = scan.cursor(), pte = scan.curpte(), scan.more(); scan.next())


/* copy the page table "tree" */
class copypgtbl_t {
	pgtbl_t * src, * dst;
	pde_t * srcpde, * dstpde;
	pte_t * srcpte, * dstpte;
	ulong curpos, end;
public:
	ulong cursor() { return curpos; };
	pte_t * cursrcpte() { return srcpte; };
	pte_t *	curdstpte() { return dstpte; };
	int fromto(pgtbl_t * src_, pgtbl_t * dst_, ulong from, ulong to);
	int next();
	int more() { return curpos < end; };
};

#endif
