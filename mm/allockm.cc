#include <lib/root.h>
#include <lib/string.h>
#include <lib/ostream.h>
#include <asm/system.h>
#include "allockm.h" 

static Q(,pool_t) poolq;

#define FREEOBJMAGIC 0xdeaddead
inline void kmobj_t::markmagic(int objsize)
{
#if DEBUG
	long * p = longs;
	long * end = longs + (objsize - sizeof(kmobj_t*))/sizeof(long);
	while (p < end)
		*p++ = FREEOBJMAGIC;
#endif
}

inline void kmobj_t::checkmagic(int objsize)
{
#if DEBUG
	long * p = longs;
	long * end = longs + (objsize - sizeof(kmobj_t*))/sizeof(long);
	while (p < end)
		assert(*p++ == FREEOBJMAGIC);
#endif
}

inline void * bucket_t::alloc()
{
	if (!ncurfree)
		return NULL;
	assert(freeobj);
	ncurfree--;
	kmobj_t * ret = freeobj;
	freeobj = freeobj->next;
	ret->checkmagic(objsize);
	return ret;
}

inline void bucket_t::check(kmobj_t * o)
{
#if DEBUG
	if (((ulong)o - (ulong)objs) % objsize)
		panic("freed object has wrong alignment\n");
	for (kmobj_t * t = freeobj; t; t = t->next)
		if (t == o)
			panic("try to free freed object\n");
#endif
}

inline void bucket_t::free(void * p)
{
	kmobj_t * o = (kmobj_t *)p;
	check(o);
	ncurfree++;
	o->next = freeobj;
	freeobj = o;
	o->markmagic(objsize);
}

inline void bucket_t::init(pool_t * pool_)
{
	ulong o, end;

	construct(this);
	assert((ulong)objs % sizeof(long) == 0);
	next = prev = NULL;
	pool = pool_;
	objsize = pool->objsize;
	freeobj = NULL;
	ncurfree = 0;
	nmaxfree = (PAGESIZE - sizeof(bucket_t)) / objsize;
	for (o = (ulong)objs, end = o + objsize*nmaxfree; o < end; o += objsize) 
		free((kmobj_t*)o);
}

int pool_t::alloced()
{
	int sum = poorq.count() * PAGESIZE;
	bucket_t * b;
	foreach (b, richq)
		sum += (b->nmaxfree - b->ncurfree) * objsize;
	return sum;
}

void * pool_t::alloc(int flags)
{
	void * res;
	bucket_t * b;
	psw_t psw;

	psw.save(), cli();	
	if (richq.empty()) {
		b = (bucket_t*) allocpage(flags);
		if (!b)	{
			psw.restore();
			return NULL;
		}
		b->init(this);
		richq.enqtail(b);
	}
	b = richq.head();
	assert(b->ncurfree);
	res = b->alloc();
	if (!b->ncurfree) {
		richq.unlink(b);
		poorq.enqtail(b);
	}
#if DEBUG
	if (flags & ACLEAR)
		memset(res, 0, objsize);
#endif
	psw.restore();
	return res;
}

void pool_t::free(void * p)
{
	bucket_t * b;
	psw_t psw;

	psw.save(), cli();
	b = (bucket_t *) pagedown(p);
	if (b->ncurfree == 0) {
		poorq.unlink(b);
		richq.enqtail(b);
	}
	b->free(p);
	if (b->ncurfree == b->nmaxfree) {
		richq.unlink(b);
		freepage(b);
	}
	psw.restore();
}

pool_t::pool_t(char * name_, int objsize_)
{
	next = prev = NULL;
	name = name_;
	objsize = roundup2(objsize_, sizeof(ulong));
	poolq.enqtail(this);
}

struct kmpool_t {
	char name[32];
	int size;
	pool_t * pool;
} kmpools[32] = {
	{"size-16", 16, NULL},
	{"size-32", 32, NULL},
	{"size-64", 64, NULL},
	{"size-96", 96, NULL},
	{"size-128", 128, NULL},
	{"size-256", 256, NULL},
	{"size-512", 512, NULL},
	{"size-1024", 1024, NULL},
	{"size-2048", 2048, NULL},
	{"size-4000", 4000, NULL},
	{NULL, 0, NULL},
};

void dumpkm(ostream_t * os)
{
	pool_t * p;
	os->write("name kernel-object-size alloced-memory\n");
	foreach (p, poolq)
		os->write("%s  %d  %d\n", p->name, p->objsize, p->alloced());
}

#define TEST 0
#define TESTN 123 

extern int nfreepage;
static void test()
{
#if TEST
	static void * v[TESTN];

	printf("nphyspage:%d, nfreepage:%d\n", nphyspage, nfreepage);
	for (int i = 0; i < TESTN; i++)
		v[i] = allockm(AKERN, 33);
	printf("nphyspage:%d, nfreepage:%d\n", nphyspage, nfreepage);
	for (int i = 0; i < TESTN; i++)
		freekm(v[i]);
	printf("nphyspage:%d, nfreepage:%d\n", nphyspage, nfreepage);
	dump();
#endif
/*
	char * c0 = (char *) allockm(AKERN, 8);
	c0[-4] = 123;
	while (1)
		allockm(AKERN, 8);
*/
}

void allockminit()
{
	construct(&poolq);
	for (kmpool_t * kp = kmpools; kp->size; kp++)
		kp->pool = new pool_t(kp->name, kp->size);
	test();
	test();
}

void * allockm(int flags, int size)
{
	for (kmpool_t * k = kmpools; k->name; k++)
		if (size <= k->size)
			return k->pool->alloc(flags);
	warn("requested size(%d) too big\n", size);
	return NULL; 
}

void freekm(void * ptr)
{
	if (!ptr)
		panic("try to free null pointer\n");
	pool_t * pool = ((bucket_t*)pagedown(ptr))->pool;
	pool->free(ptr);
}
