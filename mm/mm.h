#ifndef _MM_H
#define _MM_H
#include <asm/pgtbl.h>
#include <asm/frame.h>
#include <lib/queue.h>
#include <mm/allocpage.h>

/*
 * MAX_ARG_PAGES defines the number of pages allocated for arguments
 * and envelope for the new program. 32 should suffice, this gives
 * a maximum env+arg of 128kB !
 * MAX_ARGC define the maximum number of arguments and evnvelope.
 */
#define MAXARGPAGE 32
struct ustack_t {  
	char * page[MAXARGPAGE];
	char * sp; /* stack pointer */
	int base;
	int argc;
	int envc;

	vaddr_t vbase() { return USEREND - pagemul(MAXARGPAGE) + pagemul(base); }
	int pushustr(char * str);
	int pushkstr(char * str);
	int pushuvec(char ** vec);
	int push(int chars);
	void init()
	{
		sp = NULL;
		base = MAXARGPAGE;
	}
	void destroy()
	{
		for (; base < MAXARGPAGE; base++)
			freepage(page[base]);
	}
};

struct mm_t;
struct inode_t;

struct seg_t {
	CHAIN(,seg_t);
	mm_t * mm;
	vaddr_t start, end;
	int prot;
	int flags;
	inode_t * inode;
	ulong offset;

	~seg_t();
	static int lt(seg_t * x, seg_t * y) { return x->start < y->start; }
	int contain(vaddr_t vaddr) { return vaddr >= start && vaddr < end; }
	int clone(seg_t ** result);
	void nopage(vaddr_t vaddr, int u);
	void wppage(vaddr_t vaddr, int u);
	void unmap(vaddr_t from, vaddr_t to);
};
QUEUE(,seg_t);
typedef Q(,seg_t) segq_t;

#define PROTR        	0x1       /* page can be read */
#define PROTW       	0x2       /* page can be written */
#define PROTX		0x4       /* page can be executed */
#define PROTNONE        0x0       /* page can not be accessed */
#define PROTRW		(PROTR|PROTW)
#define PROTRX		(PROTR|PROTX)
#define PROTRWX		(PROTR|PROTW|PROTX)

#define MAPSHARED       1         /* Share changes */
#define MAPPRIVATE      2         /* Changes are private */
#define MAPTYPE         0xf       /* Mask for type of mapping */
#define MAPFIXED        0x10      /* Interpret addr exactly */
#define MAPANON    	0x20      /* don't use a file */

#define MAPUSTACK	0x40	  /* user stack */

#define KSTACKMAGIC 0x1234567

struct mm_t {
	vaddr_t onfault; /* page fault handler */
	segq_t segq;
	pgtbl_t * pgtbl;
	char * kstack;

	int initustack(ustack_t * usatck, vaddr_t * initsp);
	void dump();
	seg_t * findustack();
	seg_t * findbss();
	seg_t * findseg(vaddr_t vaddr);
	seg_t * contain(vaddr_t vaddr);
	int insertseg(seg_t * seg);
	int mapto(vaddr_t vaddr, paddr_t paddr, int prot);
	void pagefault(regs_t * regs, vaddr_t vaddr, int u, int w, int p);
	void clearfrag(vaddr_t from, vaddr_t to);
	int mmap(vaddr_t from, vaddr_t to, int prot, int flags,
	    inode_t * inode, ulong offset);
	int anonmap(vaddr_t from, vaddr_t to, int prot, int flags)
	{
		return mmap(from, to, prot, flags|MAPANON, NULL, 0);
	}
	vaddr_t brk(vaddr_t ebss);
	void unmapuspace();
	int clone(mm_t ** result);
	void init();
	mm_t();
	mm_t(vaddr_t * esp, vaddr_t * eip);
	~mm_t();
};

/* ver stands for verify */
extern int verr(void * area, int size);
extern int verw(void * area, int size);
extern int verrstr(const char * str);
extern int verrpath(const char * path);
#endif
