#include <lib/root.h>
#include <lib/string.h>
#include <lib/ostream.h>
#include <asm/system.h>
#include <kern/sched.h>
#include "allocpage.h"

#undef DEBUG
#define DEBUG 1

#define MAGIC 0x19981029
#define NLONG ((PAGESIZE - sizeof(frame_t *))/sizeof(long))
struct frame_t {
	frame_t * next;
	long longs[NLONG];

	void checkmagic()
	{
		return; /* too slow for bochs */
	#if DEBUG
		for (int i = 0; i < (int)NLONG; i++)
			assert(longs[i] == MAGIC);
	#endif
	}
	void markmagic()
	{
		return; /* too slow for bochs */
	#if DEBUG
		for (int i = 0; i < (int)NLONG; i++)
			longs[i] = MAGIC; 
	#endif
	}
	vaddr_t vaddr() { return (vaddr_t)this; }
	paddr_t paddr() { return vtop(vaddr()); }
	void * data() { return (void*)vaddr(); }
};

const paddr_t __640K = 640*1024;
const paddr_t __1M = 1024*1024;

ulong nphyspage; /* number of the total physical page */
ulong nphysbyte;
ulong nphysmeg;
page_t * pagemap;
int nfreepage;
static frame_t * freepageq; /* may changed by interrput code */

static int allocbmok;
static vaddr_t kernend, allocbmend;

void dumpmem(ostream_t * os)
{
	os->write("total memory:%d meg, %d page\n", nphysmeg, nphyspage);
	os->write("free memory:%d page\n", nfreepage);
	os->write("kernel used memory:%d page\n", pagediv(kernend - ptov(__1M)));
}

static void markfree(paddr_t from, paddr_t to)
{
	for (; from < to; from += PAGESIZE) {
		patopage(from)->refcnt = 1;
		freephys(from);
	}
}

static void marksys(paddr_t from, paddr_t to)
{
	for (; from < to; from += PAGESIZE)
		patopage(from)->refcnt = SYSPAGE;
}

void bssinit()
{
	memset(&__bss_start, 0, &_end - &__bss_start); /* clear the bss */
}

void allocbminit()
{
	allocbmok = 1;
	kernend = (vaddr_t) &_end;
	allocbmend = pageup(kernend + nphysbyte/2);
}

#define TEST 0
#define TESTN 127
static void test()
{
#if TEST
	static void * v[TESTN];

	// *(long*)ptov(nphysbyte - 4000) = 12345678;
	printf("nphyspage:%d, nfreepage:%d\n", nphyspage, nfreepage);
	for (int i = 0; i < TESTN; i++)
		v[i] = allocpage(AKERN);
	printf("nphyspage:%d, nfreepage:%d\n", nphyspage, nfreepage);
	for (int i = 0; i < TESTN; i++)
		freepage(v[i]);
	printf("nphyspage:%d, nfreepage:%d\n", nphyspage, nfreepage);
#endif
}

/* 4K 636K 384K text data bss bootmem freemem */ 
void pagemapinit()
{
	pagemap = (page_t*) allocbm(nphyspage * sizeof(page_t*), sizeof(page_t));
	marksys(0, PAGESIZE);
	markfree(PAGESIZE, __640K); 
	marksys(__640K, __1M);
	marksys(vtop(KCODESTART), vtop(allocbmend));
	markfree(vtop(allocbmend), nphysbyte);
	test();
	test();
}

void freebm()
{
	allocbmok = 0;
	kernend = pageup(kernend);
	markfree(vtop(kernend), vtop(allocbmend));
}

/* allocate memory during boot time */
void * allocbm(int size, int balign)
{
	vaddr_t result;

	if (!allocbmok)
		panic("allocbm has been disabled\n");
	result = kernend = roundup2(kernend, balign);
	kernend += size;
	if (kernend > allocbmend) 
		panic("out of memory\n");
	memset((void*)result, 0, size);
	return (void*) result;
}

int lowfreepage(int request)
{
	return nfreepage < request + 8 + DRIVERQUOTA + KERNQUOTA;
}

void outofmem(int user)
{
	warn("%s:out of mem, nfreepage=%d\n", user ? "user" : "kern", nfreepage);
	if (user) {
		curr->force(SIGSEGV);
		return;
	} 
	doexit(SIGSEGV);
}

static inline void check()
{
#if DEBUG
	if (nfreepage <= DRIVERQUOTA + KERNQUOTA) {
		int sum = 0;
		frame_t * frame = freepageq;
		for (; frame; frame = frame->next, sum++)
			assert(!vatopage(frame->vaddr())->refcnt);
		assert(sum == nfreepage);
	}
#endif
}

static inline int allocable(frame_t * frame)
{
	paddr_t paddr = frame->paddr();
	return between(PAGESIZE, paddr, __640K) ||
	       between(vtop(kernend), paddr, nphysbyte);
}

void * allocpage(int flags)
{
	frame_t * frame;
	page_t * page;
	psw_t psw;

	check();
	if (flags & AUSER) {
		if (nfreepage <= DRIVERQUOTA + KERNQUOTA)
			return NULL;
	} else if (flags & ADRIVER) {
		if (nfreepage <= KERNQUOTA) {
			printf("allcopage:driver quota used out\n");
			return NULL;
		}
	} else if (flags & AKERN) {
		if (!nfreepage) { /* flags & AKERN */
			printf("allocpage:kern quota used out\n");
			outofmem(0);
			return NULL;
		}
	} else {
		panic("invalid flags\n");
	}
	psw.save(), cli();
	frame = freepageq;
	assert(allocable(frame));
	freepageq = freepageq->next;
	nfreepage--;
	page = vatopage(frame->vaddr());
	assert(page->refcnt == 0);
	page->refcnt = 1;
	psw.restore();
	frame->checkmagic();
	if (flags & ACLEAR)
		memset(frame->data(), 0, PAGESIZE);
	return frame->data();
}

void freepage(void * ptr)
{
	page_t * page;
	frame_t * frame;
	psw_t psw;

	assert(!pagemod(ptr));
	assert(vtop((ulong) ptr) < nphysbyte);
	assert(allocable((frame_t*) ptr));

	page = vatopage((vaddr_t) ptr);
	if (page->refcnt == SYSPAGE)
		panic("try to free system page");
	else if (!page->refcnt)
		panic("try to free freed page");
	else if (--page->refcnt)
		return;

	psw.save(), cli();
	frame = (frame_t*) ptr;
	frame->next = freepageq;
	freepageq = frame;
	nfreepage++;
	psw.restore();
	frame->markmagic();
}
