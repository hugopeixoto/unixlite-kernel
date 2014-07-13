#include <lib/root.h>
#include <lib/errno.h>
#include <lib/string.h>
#include <mm/allocpage.h>
#include "pgtbl.h" 

pgtbl_t kpgtbl __attribute__((aligned(PAGESIZE)));

static pte_t * getkpte(ulong vaddr)
{
	pde_t * pde = kpgtbl.pgd + pdeidx(vaddr);
	if (!pde->tstp())
		pde->bits = vtop((ulong)allocbm(PAGESIZE,PAGESIZE)) + PTEU + PTEW + PTEP;
	return pde->getpte(vaddr);
}

extern char _end;
void kpgtblinit()
{
	memset(&kpgtbl, PAGESIZE, 0);
	for (ulong vaddr = KERNSTART; vaddr < KERNEND; vaddr += PAGESIZE)
		getkpte(vaddr)->bits = vtop(vaddr) + PTEW + PTEP;
	asm volatile ("movl %0,%%cr3; jmp 1f; 1:"::"r"(vtop((ulong)&kpgtbl)));
}

void pgtbl_t::sharekpgtbl()
{
	int i;

	for (i = pdeidx(KERNSTART); i < pdeidx(KERNEND); i++)
		pgd[i] = kpgtbl.pgd[i];
	for (i = pdeidx(ALLOCVMSTART); i < pdeidx(ALLOCVMEND); i++)
		pgd[i] = kpgtbl.pgd[i];
}

void * pgtbl_t::operator new(size_t size)
{
	return allocpage(AKERN | ACLEAR); /* never fail */
}

void pgtbl_t::operator delete(void * p)
{
	pgtbl_t * pgtbl = (pgtbl_t*) p;
	assert(pgtbl->none(0,USEREND));
	pgtbl->freetree();
	freepage(pgtbl);
}

/* this function only allocate page-table-tree, all pte are set 
   to non-present */
int pgtbl_t::alloctree(ulong from, ulong to)
{
	pde_t * pde;
	ulong oldfrom = from;

	from = rounddown2(from, PDECOVERED);
	to = roundup2(to, PDECOVERED);
	for (; from < to; from += PDECOVERED) {
		pde = getpde(from);
		if (!pde->tstp() && !pde->alloc()) {
			freetree(oldfrom, from);
			return 0;
		}
	}
	return 1;
}

int pgtbl_t::none(ulong from, ulong to)
{
	pte_t * pte;
	foreachpte(pte, this, from, to)
		if (!pte->none())
			return 0;
	return 1;
}

/* NOTE: TLB managemnet is up to the caller */
#warning "hasbug"
void pgtbl_t::freetree(ulong from, ulong to)
{
	assert(from <= to && to <= USEREND);
	assert(none(from, to));
#if 0
	pde_t * start = getpde(rounddown2(from, PDECOVERED));
	pde_t * end = getpde(roundup2(to, PDECOVERED));
	pte_t * pte;
	pte_t * startpte;
	pte_t * endpte;

	for (pde_t * pde = start + 1; pde < end - 1; pde++) {
		if (pde->tstp()) {
			freephys(pde->paddr());
			pde->clear();
		}
	}
	startpte = start->startpte();
	endpte = startpte + pteidx(from); 
	for (pte = startpte; pte < endpte; pte++)
		if (pte->tstp())
			goto  checklast;
	if (start != end) {
		freephys(start->paddr());
		start->clear();
	}
checklast:
	end--;
	startpte = end->startpte() + pteidx(to);
	endpte = end->endpte(); 
	for (pte = startpte; pte < endpte; pte++)
		if (pte->tstp())
			return;
	freephys(end->paddr());
	end->clear();
#endif
}

/* NOTE: TLB managemnet is up to the caller */
void pgtbl_t::freetree()
{
	pde_t * start = getpde(USERSTART);
	pde_t * end = getpde(USEREND);

	assert(none(USERSTART, USEREND));
	for (pde_t * pde = start; pde < end; pde++) {
		if (pde->tstp()) {
			freephys(pde->paddr());
			pde->clear();
		}
	}
}

ulong pgtbl_t::explore(ulong from, ulong to, pde_t ** pdep, pte_t ** ptep)
{
	assert(from <= to);
	ulong off = from & (PDECOVERED - 1); /* offset within one page table */
	from = rounddown2(from, PDECOVERED);
	to = roundup2(to, PDECOVERED);
	assert(from <= to); /* "to" may overflow */

	for (; from < to; from += PDECOVERED) {
		pde_t * pde = getpde(from);
		if (pde->tstp()) {
			*pdep = pde;
			*ptep = pde->getpte(from + off);
			return from + off;
		}
		off = 0;
	}
	*pdep = NULL;
	*ptep = NULL;
	return to;
}

scanpgtbl_t::scanpgtbl_t(pgtbl_t * pgtbl_, ulong from, ulong to)
{
	assert(from <= to);
	assert(!pagemod(from) && !pagemod(to));
	pgtbl = pgtbl_;
	end = to;
	curpos = pgtbl->explore(from, to, &pde, &pte);
}

void scanpgtbl_t::next()
{
	++pte;
	curpos += PAGESIZE;
	if (curpos & (PDECOVERED-1))
		return;
	++pde;
	/* skip non present secondary level pgtbl */
	while (more()) { 
		if (pde->tstp()) {
			pte = pde->startpte();
			return;
		}
		++pde;
		curpos += PDECOVERED;
	}
	pde = NULL;
	pte = NULL;
}

int copypgtbl_t::fromto(pgtbl_t * src_, pgtbl_t * dst_, ulong from, ulong to)
{
	assert(!pagemod(from) && !pagemod(to));
	src = src_;
	dst = dst_;
	end = to;
	curpos = src->explore(from, to, &srcpde, &srcpte);
	if (curpos >= to) {
		dstpde = NULL;
		dstpte = NULL;
		return 0;
	}
	dstpte = dst->getpte(curpos);
	if (!dstpte)
		return ENOMEM;
	dstpde = dst->getpde(curpos);
	return 0;
}

int copypgtbl_t::next()
{
	++srcpte, ++dstpte;
	curpos += PAGESIZE;
	if (curpos & (PDECOVERED-1))
		return 0;
	++srcpde, ++dstpde;
	/* skip non present secondary level pgtbl */
	while (more()) {
		if (srcpde->tstp()) {
			dstpte = dst->getpte(curpos);
			if (!dstpte)
				return ENOMEM;
			dstpde = dst->getpde(curpos);
			return 0;
		}
		srcpde++;
		curpos += PDECOVERED;
	}
	srcpde = dstpde = NULL;
	srcpte = dstpte = NULL;
	return 0;
}
