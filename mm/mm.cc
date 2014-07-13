#include <lib/root.h>
#include <lib/errno.h>
#include <lib/limits.h>
#include <lib/string.h>
#include <lib/gcc.h>
#include <kern/sched.h>
#include <mm/allocpage.h>
#include <asm/pgtbl.h>
#include <fs/inode.h>
#include <boot/elf.h>
#include "mm.h"

#undef debug
#define debug(...)

#define __try curr->mm->onfault = (vaddr_t) &&onfault

#define __final \
	curr->mm->onfault = 0; \
	return 0; \
onfault: /* here must be reachable */ \
	curr->mm->onfault = 0; \
	return EFAULT

/* verify read */ 
int verr(void * area, int size)
{
	if (!size)
		return EFAULT;
	__try;
	for (void * end = pageup(area + size); area < end; area += PAGESIZE) {
		if (!uspace(area))
			goto onfault; /* goto make onfault reachable */
		char tmp;
		asm volatile ("movb %1,%0":"=r"(tmp):"m"(*(char*)area));
	}
	__final;
}

/* verify write */
int verw(void * area, int size)
{
	if (!size)
		return EFAULT;
	__try;
	for (void * end = pageup(area + size); area < end; area += PAGESIZE) {
		if (!uspace(area))
			goto onfault; /* goto make onfault reachable */
		char tmp;
		asm volatile ("movb %1,%0":"=r"(tmp):"m"(*(char*)area));
		asm volatile ("movb %1,%0":"=m"(*(char*)area):"r"(tmp));
	}
	__final;
}

/* verify read string */
int verrstr(const char * str)
{
	__try;
	for (; *str; str++) {
		if (!uspace(str))
			goto onfault;
		char tmp;
		asm volatile ("movb %1,%0":"=r"(tmp):"m"(*str));
	}
	__final;
}

/* verify read path */
int verrpath(const char * path)
{
	if (verrstr(path))
		return EFAULT;
	return (strlen(path) <= PATHMAX) ? 0 : ENAMETOOLONG;
}

/* verify read null terminated vector */
static int verrvec(char ** vec)
{
	__try;
	for (; *vec; vec++) {
		if (!uspace(vec))
			goto onfault;
		void * tmp;
		asm volatile ("movl %1,%0":"=r"(tmp):"m"(*vec));
	}
	__final;
}

/*	 curr->startstack = sp & 0xfffff000;
 * sp:	 argc
 *	 argv : IBCS need not this field
 *	 envp : IBCS need not this field
 * argv: argv[0..argc-1]
 *       0
 * envp: envp[0..envc-1]
 *       0
 * sp:   argv[0][0] argv[0][1] ... 0; argv[1][0] argv[1][1] ... 0
 *       envp[0][0] envp[0][1] ... 0; envp[0][0] envp[0][1] ... 0
 *       ATPHDR
 *       ...
 *       ATEGID 
 *       ATNULL
 */
static vaddr_t createargv(ustack_t * ustack)
{
	char * str = (char*) (ustack->vbase() + pagemod(ustack->sp));
	ulong * sp = (ulong*) rounddown2((ulong)str, sizeof(ulong));
	int argc = ustack->argc;
	int envc = ustack->envc;

	sp -= envc + 1;
	char ** envp = (char**) sp;
	sp -= argc + 1;
	char ** argv = (char**) sp;
#ifdef AOUT
	*--sp = (ulong) envp;
	*--sp = (ulong) argv;
#endif
	*--sp = (ulong) argc;

	while (argc-- > 0) {
		*argv++ = str;
		str = str + strlen(str) + 1;
	}
	*argv = NULL;
	while (envc-- > 0) {
		*envp++ = str;
		str = str + strlen(str) + 1;
	}
	*envp = NULL;
	allege((vaddr_t)str == USEREND - (ATEGID+2)*8);

	ulong * entry = (ulong*)str;  
#define auxvec(id, value) *entry++ = id, *entry++ = value
	auxvec(ATPHDR, 0);
	auxvec(ATPHENT, sizeof(elfphdr_t));
	auxvec(ATPHNUM, 0);
	auxvec(ATPAGESZ, PAGESIZE);
	auxvec(ATBASE, 0);
	auxvec(ATFLAGS, 0);
	auxvec(ATEGID, 0);
	auxvec(ATUID, curr->uid);
	auxvec(ATEUID, curr->euid);
	auxvec(ATGID, curr->gid);
	auxvec(ATEGID, curr->egid);
	auxvec(ATNULL, 0);
#undef auxvec

	return (vaddr_t) sp;
}

int ustack_t::push(int chars)
{
	while (chars) {
		int once;
		if (!pagemod(sp)) {
			if (--base < 0)
				return E2BIG;
			page[base] = (char*) allocpage(AUSER);
			if (!page[base])
				return ENOMEM;
			sp = page[base] + PAGESIZE;
			once = min(PAGESIZE, chars); 
		} else
			once = min(pagemod((ulong)sp), chars);
		chars -= once;
		sp -= once; 
	}	
}

int ustack_t::pushkstr(char * str)
{
	char * c;

	/* the trailing zero is also pushed onto stack */
	for (c = str + strlen(str); c >= str; c--) {
		if (!pagemod(sp)) {
			if (--base < 0)
				return E2BIG;
			page[base] = (char*) allocpage(AUSER);
			if (!page[base])
				return ENOMEM;
			sp = page[base] + PAGESIZE;
		}
		*--sp = *c;
	}
	return 0;
}

int ustack_t::pushustr(char * str)
{
	int e;

	if (e = verrstr(str))
		return e;
	return pushkstr(str);
}

int ustack_t::pushuvec(char ** vec)
{
	int e;
	char ** str;

	if (e = verrvec(vec))
		return e;
	for (str = vec; *str; str++)
		/* nothing */;
	for (str--; str >= vec; str--) {
		if (e = pushustr(*str))
			return e;
	}
	return 0;
}

int mm_t::initustack(ustack_t * ustack, vaddr_t * initsp)
{
	int i = ustack->base, e;
	vaddr_t vaddr = ustack->vbase(); 

	for (; vaddr < USEREND; vaddr += PAGESIZE, i++) {
		if (e = mapto(vaddr, vtop(ustack->page[i]), PROTRW))
			return e;
	}
	*initsp = createargv(ustack);
	return anonmap(*initsp, USEREND, PROTRW, MAPPRIVATE|MAPFIXED);
}

void mm_t::dump()
{
	seg_t * s;

	printf("start-end offset-inode prot\n");
	foreach (s, segq) {
		const char * prot = (s->prot & PROTW) ? "write" : "read";
		printf("%08lx-%08lx %08lx-%08lx %s\n", 
		s->start, s->end, s->offset, s->inode, prot);
	}
}

seg_t * mm_t::findustack()
{
	if (segq.count() < 4) { 
		warn("bad mm layout\n");
		dump();
		return NULL;
	}
	seg_t * seg = segq.tail();
	if (seg && (seg->prot & PROTW))
		return seg;
	warn("user stack segment not found\n");
	return NULL;
}

/* code data bss stack */
seg_t * mm_t::findbss()
{
	if (segq.count() < 4) { 
		warn("bad mm layout\n");
		dump();
		return NULL;
	}
	seg_t * seg = segq.at(2); 
	if (seg && seg->prot & PROTW)
		return seg;
	warn("user bss segment not found\n");
	return NULL;
}

/* look up the first segment which satifies vaddr < seg->end, NULL if none */
seg_t * mm_t::findseg(vaddr_t vaddr)
{
	seg_t * seg;

	foreach (seg, segq)
		if (vaddr < seg->end)
			return seg;
	return NULL;
}

seg_t * mm_t::contain(vaddr_t vaddr)
{
	seg_t * seg;
	return ((seg = findseg(vaddr)) && (seg->start <= vaddr)) ? seg : NULL;
}

int mm_t::mapto(ulong vaddr, paddr_t paddr, int prot)
{
	pte_t * pte = pgtbl->getpte(vaddr);
	if (!pte)
		return ENOMEM;
	assert(pte->none());
	prot = (prot & PROTW) ? (PTEU | PTEW | PTEP) : (PTEU | PTEP);
	pte->mapto(paddr, prot); 
	return 0;
}

void mm_t::pagefault(regs_t * regs, vaddr_t vaddr, int u, int w, int p)
{
	seg_t * seg;

	if (!(seg = findseg(vaddr)))
		goto bad;
	if (vaddr < seg->start) {
		if (findustack() != seg)
			goto bad;
		/* accessing the stack below %esp is always a bug.
		   The "+ 32" is there due to some instructions (like
		   pusha) doing pre-decrement on the stack and that
		   doesn't show up until later.. */
		if (vaddr + 32 < regs->esp)
			goto bad;
		/* grow the user stack */
		seg->start = pagedown(vaddr);
	}
	/* now the segment contain the vaddr */
	assert(!p || w);
	if (w && !(seg->prot & PROTW))
		goto bad;
	/* good area */
	p ? seg->wppage(vaddr, u) : seg->nopage(vaddr, u);
	return;

bad:	if (u) {
		printf("user fault:(eip=%08lx,cr2=%08lx,u=%d,w=%d,p=%d)\n",
			regs->eip, vaddr, u, w, p);
		curr->post(SIGSEGV);
		return;
	}
	if (uspace(vaddr) && onfault) {
		regs->eip = onfault; /* throw the exception */
		return;
	}
	panic("unhandled page fault(eip=%08lx,cr2=%08lx,u=%d,w=%d,p=%d)", 
	regs->eip, vaddr, u, w, p);
}

/* we don't consider overlap currently */
int mm_t::insertseg(seg_t * neu)
{
	if ((neu->start > neu->end) || (neu->end > USEREND))
		return EINVAL;
	segq.insertlt(neu);
	return 0;
}

static inline void clear(vaddr_t from, vaddr_t to)
{
	memset((void*)from, 0, to - from);
}

/* We need to explicitly zero any fractional pages
   after the data section (i.e. bss).  This would
   contain the junk from the file that should not
   be in memory */
void mm_t::clearfrag(vaddr_t from, vaddr_t to)
{
	assert(from < to);
	seg_t * s = findseg(from);
	assert(s);
	s->nopage(from, 0);
	pte_t * pte = pgtbl->probe(from);
	pte->setw();
	flushone(from);
	memset((void*)from, 0, to - from);
	if (!(s->prot & PROTW)) {
		pte->clrw();
		flushone(from);
	}
}

/* size == 0 is acceptable, an empty segment will be created in this case */
int mm_t::mmap(vaddr_t from, vaddr_t to, int prot, int flags, 
	inode_t * inode, ulong offset)
{
	int e;
	seg_t * x;

	assert(from <= to);
	assert(flags & MAPFIXED);
	if (inode && (pagemod(from) != pagemod(offset))) {
		warn("from:(%lx) offset(%x) is not suitable for demand"
		      "paging\n", from, offset);
		return EINVAL;
	}
	x = new seg_t();
	x->next = x->prev = NULL;
	x->mm = this;
	x->start = pagedown(from);
	x->end = pageup(to);
/* the segment must be writebale while clear the fractional page */
	x->prot = prot;
	x->flags = flags;
	x->inode = inode;
	x->offset = pagedown(offset);
	if (e = insertseg(x)) {
		delete x;
		return e;
	}
	if (inode)
		inode->hold();
	return 0;
}

static int copypte(pgtbl_t * x, pgtbl_t * y, vaddr_t from, vaddr_t to)
{
	int e;
	copypgtbl_t copy;

	if (e = copy.fromto(x, y, from, to))
		return e;
	while (copy.more()) {
		pte_t * src = copy.cursrcpte();
		pte_t * dst = copy.curdstpte();
		*dst = *src;
		if (src->none())
			goto next;
		if (src->tstw()) {
			src->clrw();
			dst->clrw();
			flushone(copy.cursor());
		}
		dupphys(src->paddr());
	next:	if (e = copy.next())
			return e;
	}
	return 0;
}

void mm_t::unmapuspace()
{
	seg_t * seg;

	foreachsafe (seg, segq)
		delete seg;
}

mm_t::~mm_t()
{
	unmapuspace();
	allege(*(ulong*)kstack == KSTACKMAGIC);
	freepage(kstack);
	delete pgtbl;
}

void mm_t::init()
{
	onfault = 0;
	pgtbl = new pgtbl_t();
	pgtbl->sharekpgtbl();
	kstack = (char*)allocpage(AKERN);
	*(ulong*)kstack = KSTACKMAGIC;
}

mm_t::mm_t()
{
	init();
}

#include "idletaskcode.h"
/* init idle task's mm */
mm_t::mm_t(vaddr_t * esp, vaddr_t * eip)
{
	init();
	anonmap(IDLETASKSTART, IDLETASKEND, PROTRWX, MAPFIXED|MAPPRIVATE);
	char * page = (char *)allocpage(AKERN);
	memcpy(page, idletaskcode, sizeof(idletaskcode));
	mapto(IDLETASKSTART, vtop(page), PROTRWX);
	*esp = IDLETASKSTART + PAGESIZE;
	*eip = IDLETASKSTART;
}

int mm_t::clone(mm_t ** result)
{
	int e;
	mm_t * y = new mm_t();
	*result = NULL;
	seg_t * seg, * segy;
	pgtbl_t * pgtbly = y->pgtbl;

	foreach (seg, segq) {
		segy = new seg_t();
		segy->next = segy->prev = NULL;
		segy->mm = y;
		segy->start = seg->start;
		segy->end = seg->end;
		segy->prot = seg->prot;
		segy->flags = seg->flags;
		if (segy->inode = seg->inode)
			segy->inode->hold();
		segy->offset = seg->offset;  
		if (e = copypte(pgtbl, pgtbly, seg->start, seg->end))
			return e;
		y->segq.enqtail(segy);
	}
	*result = y;
	return 0;
}

int seg_t::clone(seg_t ** result)
{
	seg_t * x = new seg_t();
	*result = x;
	x->next = x->prev = NULL;
	return 0;
}

void seg_t::nopage(vaddr_t vaddr, int u)
{
	assert(between(start,vaddr,end));
	void * page = allocpage(AUSER);
	if (!page) {
		outofmem(u);
		return;
	}
	if (mm->mapto(vaddr, vtop(page), prot) == ENOMEM) {
		freepage(page);
		outofmem(u);
		return;
	}
	if (!inode) {
		memset(page, 0, PAGESIZE);
		return;
	}
	inode->read(offset + pagedown(vaddr) - start, page, PAGESIZE);
	debug("nopage vaddr:%x mapto paddr:%x--no\n", vaddr, vtop(page));
}

void seg_t::wppage(vaddr_t vaddr, int u)
{
	pte_t * pte = mm->pgtbl->probe(vaddr);
	assert(pte);
	page_t * our = patopage(pte->paddr());
	assert(our->refcnt>0);
	if (our->refcnt == 1) {
		pte->setw();
		flushone(vaddr);
		return;
	}
	void * my = allocpage(AUSER);
	if (!my) {
		outofmem(u);
		return;
	}
	pte->clear();
	flushone(vaddr);
	pte->mapto(vtop(my), PTEU | PTEW | PTEP);
	memcpy(my, our->data(), PAGESIZE);
	our->refcnt--;
}

seg_t::~seg_t()
{
	unlink();
	unmap(start, end);
	if (inode) inode->lose(), inode = NULL;
}

extern int sig;
void seg_t::unmap(vaddr_t from, vaddr_t to)
{
	vaddr_t cursor;
	pte_t * pte;

	foreachpte2 (cursor, pte, mm->pgtbl, from, to) {
		if (pte->tstp()) {
			freephys(pte->paddr());
			pte->clear();
			flushone(cursor);
		}
	}
//	mm->pgtbl->freetree(from, to);
}

vaddr_t mm_t::brk(vaddr_t ebss)
{
	ebss = pageup(ebss);
	seg_t * bss = findbss();
	if (!bss)
		return 0;
	if (ebss < bss->start)
		return bss->end;
	if (ebss < bss->end) {
		bss->unmap(pageup(ebss), pageup(bss->end));
		bss->end = ebss;
		return bss->end;
	}
	if (bss->next != segq.vnode())
		bss->end = min(ebss, pagedown(bss->next->start));
	else
		bss->end = ebss; 
	return bss->end;
}

asmlinkage vaddr_t sysbrk(vaddr_t ebss)
{
	return curr->mm->brk(ebss);
}
