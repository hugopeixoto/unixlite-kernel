#include <lib/config.h>
#include <lib/root.h>
#include <lib/errno.h>
#include <lib/gcc.h>
#include <lib/ostream.h>
#include <init/ctor.h>
#include <dev/blk/dev.h>
#include "buf.h"
#include "inode.h" /* for BNOTALLOC */

static ulong nfind;
static ulong nfindsucc;
static int nbuf;
static Q(all,buf_t) allq;
static Q(free,buf_t) freeq;

static int hsize;
static int hmask;
typedef Q(hash,buf_t) hashq_t;
static hashq_t * hashtab;

static bno_t nullbno;

static inline hashq_t * hashfunc(dev_t dev, bno_t bno)
{
	return hashtab + ((dev + bno) & hmask);
}

void dumpb()
{
//	printf("total buffer number:%d\n", nbuf);
	printf("free buffer number:%d\n", freeq.count());
//	printf("bcache hit rate:lookup=%ld, succeed=%ld\n", nfind, nfindsucc);
}

void dumpb(ostream_t * os)
{
	os->write("total buffer number:%d\n", nbuf);
	os->write("free buffer number:%d\n", freeq.count());
	os->write("bcache hit rate:lookup=%ld, succeed=%ld\n", nfind, nfindsucc);
}

__ctor(PRIFS, SUBANY, buffercache)
{
	construct(&allq);
	construct(&freeq);
	hsize = nbuf = roundup2p2(128 * nphysmeg);
	hmask = hsize - 1;
	hashtab = (hashq_t*) allocbm(hsize * sizeof(hashq_t));
	construct(hashtab, hsize);

	buf_t * b = (buf_t*) allocbm(nbuf * sizeof(buf_t));
	char * data = (char *) allocbm(nbuf * MINIXBSIZE, MINIXBSIZE);

	for (int i = 0; i < nbuf; i++, b++, data += MINIXBSIZE) {
		b->nextall = b->prevall = NULL;
		b->nexthash = b->prevhash = NULL;
		b->nextfree = b->prevfree = NULL;
		b->nextreq = b->prevreq = NULL;
		b->state = BINVAL;
		b->refcnt = 0;
		b->dev = mkdev(NULLMAJOR,0);
		b->bno = nullbno++;
		b->bsize = MINIXBSIZE;
		b->data = data;
		construct(&b->iolock);
		hashfunc(b->dev, b->bno)->enqtail(b);
		freeq.enqtail(b);
		allq.enqtail(b);
	}
}

static buf_t * findb(dev_t dev, bno_t bno, int bsize = MINIXBSIZE)
{
	buf_t * b;
	hashq_t * hashq = hashfunc(dev, bno); 

	nfind++;
	foreach (b, *hashq) {
		if ((b->dev == dev) && (b->bno == bno)) {
			assert(b->bsize == bsize);
			nfindsucc++;
			return b;
		}
	}
	return NULL;
}

static inline int ideal(buf_t * b)
{
	return !(b->locked()) && (b->state == BDIRTY);
}

/* write async buf cluster */
static void writeb(buf_t * core)
{
#if !RWAHEAD
	llwriteab(&core, 1);
#else
	if (major(core->dev) == NULLMAJOR)
		return;
	buf_t * cluster[MAXBUFPERIO];
	dev_t dev = core->dev;
	bno_t bno = core->bno;
	buf_t ** p = cluster + MAXBUFPERIO/2;
	buf_t ** end = cluster + MAXBUFPERIO;
	int total = 0;
	buf_t * b = core;
	
	/* find neighbour on right */
	do {
		(*p = b)->hold();
		total++, p++, bno++;
	} while ((p < end) && (b = findb(dev, bno)) && ideal(b));

	/* find neighbour on left */
	p = cluster + MAXBUFPERIO/2 - 1;
	end = cluster - 1;
	bno = core->bno - 1;
	while ((p > end) && (b = findb(dev, bno)) && ideal(b)) {
		(*p = b)->hold();
		total++, p--, bno--;
	}
	p++;

	/* write them all */
	llwriteb(p, total);
	while (total--)
		(*p++)->lose();
#endif
}

/* getblk always succeed, it will return a non-null and unlocked b; */
buf_t * getb(dev_t dev, bno_t bno)
{
	buf_t * b;
	int candidates = 8;

	assert(bno != BNOTALLOC);
repeat: if (b = findb(dev, bno, MINIXBSIZE)) {
		b->hold();
		b->wait();
		if (b->nextfree) /* b may not in the freeq */
			freeq.unlink(b);
		return b;
	}
	foreach(b, freeq) { /* search from lru to mru */
		if (b->refcnt)
			continue;
		if (candidates > 0)
			candidates--;
 		if (b->locked()) {
			if (candidates)
				continue;
/* NOTE!! While we slept waiting for this block, somebody else might
   already have added "this" block to the cache. check it */
			b->wait();
			goto repeat;
		}
		if (b->state == BDIRTY) {
			b->hold();
			writeb(b);
			if (!candidates)
				b->wait();
			b->lose();
			goto repeat;
		}
		/* !b->refcnt && !b->locked() && b->state != BDIRTY */
		b->hold();
		freeq.unlink(b); /* b must in the freeq */
		b->unlinkhash(); /* remove from old hashq */
		hashfunc(dev, bno)->enqtail(b); /* join the new hashq */
		b->state = BINVAL;
		b->dev = dev;
		b->bno = bno;
		return b;
	}
	panic("no free block buffer in bcache\n");
	return NULL;
}

/* brelse */
void buf_t::lose()
{
	assert(refcnt > 0);
	assert(findb(dev, bno, bsize));
	if (--refcnt)
		return;
	if (!nextfree)
		freeq.enqtail(this); /* put at mru */
}

int readb(dev_t dev, bno_t bno, buf_t ** b)
{
	if (!validdev(dev)) {
		warn("read on invalid block device %x\n", dev);
		(*b) = NULL;
		return EINVAL;
	}
	*b = getb(dev, bno);
	if ((*b)->state != BINVAL)
		return 0;
	llreadb(b, 1);
	if ((*b)->state == BINVAL) { /* may be IO error */
		(*b)->lose();
		(*b) = NULL;
		return EIO;
	}
	return 0;
}

void readab(dev_t dev, bno_t start, int nr)
{
#if !RWAHEAD
	buf_t * b[nr];

	assert(nr < 64);
	for (int i = 0; i < nr; i++)
		b[i] = getb(dev, start + i);
	llreadab(b, nr);
	for (int i = 0; i < nr; i++)
		b[i]->lose();
#endif
}

extern void prempt();
void syncb(dev_t dev)
{
	buf_t * b;
	
	foreach (b, allq) {
		if ((dev != 0xffff) && (dev != b->dev))
			continue;
		b->hold();
		writeb(b);
		b->lose();
		prempt();
	}
}

void syncb()
{
	syncb(0xffff);
}

extern void synci();
asmlinkage int syssync()
{
	for (int i = 0; i < 2; i++) {
		synci();
		syncb();
	}
	return 0;
}

void llrwab(int rw, buf_t * b[], int nr)
{
	if (!nr)
		return;
	if (major(b[0]->dev) == NULLMAJOR)
		return;
	blkdev_t * dev = getblkdev(b[0]->dev);
	if (!dev) {
		warn("invlaid block device:%x\n", b[0]->dev);
		return;
	}
	int count = 0;
	for (int i = 0; i < nr; i++) {
		assert(b[i]->dev == b[0]->dev);
		assert(b[i]->bsize = b[0]->bsize);
		assert(b[i]->refcnt > 0);
		b[i]->lock();
		if (rw == BREAD) {
			if (b[i]->state != BINVAL) {
				b[i]->unlock();
				continue;
			}
		} else {
			if (b[i]->state != BDIRTY) {
				b[i]->unlock();
				continue;
			}
		}
		count++;
		dev->addreq(rw, b[i]);
	}
	if (count)
		dev->doreq();
}
