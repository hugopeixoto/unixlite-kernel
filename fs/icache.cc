#include <lib/root.h>
#include <lib/ostream.h>
#include <init/ctor.h>
#include <lib/errno.h>
#include <mm/allockm.h>
#include <dev/blk/dev.h>
#include "buf.h"
#include "inode.h"
#include "fs.h"

static int hsize, hmask;
typedef Q(hash,inode_t) hashq_t;
static hashq_t * hashtab;
static Q(all,inode_t) allq;
static Q(free,inode_t) freeq;
static int ninode;
static inode_t * inodetab;
static ino_t virtualino;

static hashq_t * hashfunc(fs_t * fs, ino_t ino)
{
	int base = (int) fs;
	return hashtab + ((base+ino)&hmask);
}

void dumpi(ostream_t * os)
{
	os->write("total inode number:%d ", ninode);
	os->write("free inode number:%d ", freeq.count());
	inode_t * i;
	os->write("\nbusy inode(dev:ino:refcnt\n");
	foreach (i, allq) {
		if (i->refcnt != 0)
			os->write("(%x:%d:%d)  ", i->dev, i->ino, i->refcnt);
	}
	os->write("\n");
}

__ctor(PRIFS, SUBANY, inodecache)
{
	construct(&allq);
	construct(&freeq);

	hsize = min(256, roundup2p2(8 * nphysmeg));
	hmask = hsize - 1;
	hashtab = (hashq_t *) allocbm(hsize * sizeof(hashq_t));
	construct(hashtab, hsize);

	ninode = hsize;
	inodetab = (inode_t *) allocbm(ninode * sizeof(inode_t));
	construct(inodetab, ninode);
	for (inode_t * i = inodetab, * end = inodetab + ninode; i < end; i++) {
		i->nextall = i->prevall = NULL;
		i->nexthash = i->prevhash = NULL;
		i->nextfree = i->prevfree = NULL;
		i->state = IINVAL;
		i->flags = 0;
		i->refcnt = 0;
		i->fs = NULL;
		i->dev = mkdev(NULLMAJOR, 0);
		i->ino = virtualino++;
		i->bsize = MINIXBSIZE;
		i->bsizebits = 10;
		i->peer = NULL;
		i->unstsock = NULL;

		allq.enqtail(i);
		freeq.enqtail(i);
		hashfunc(i->fs, i->ino)->enqtail(i);
	}
}

inode_t * findi(fs_t * fs, ino_t ino)
{
	inode_t * inode;

	foreach (inode, *hashfunc(fs, ino)) {
		if (inode->fs == fs && inode->ino == ino)
			return inode;
	}
	return NULL;
}

static inode_t * geti(fs_t * fs, ino_t ino)
{
	inode_t * inode;
	int candidates = 8;

repeat: if (inode = findi(fs, ino)) {
		inode->hold();
		inode->wait();
		if (inode->nextfree)
			freeq.unlink(inode);
		return inode;
	}
	foreach (inode, freeq) {
		if (inode->refcnt)
			continue;
		if (candidates > 0)
			candidates--;
		if (inode->locked()) {
			if (candidates)
				continue;
			inode->wait();
			goto repeat;
		}
		if (inode->state == IDIRTY) {
			inode->write();
			goto repeat;
		}
		/* !refcnt && !locked && state != BDIRTY */
		inode->hold();
		freeq.unlink(inode);
		inode->state = IINVAL;
		assert(!inode->flags);
		inode->unlinkhash();
		inode->fs = fs;
		inode->dev = fs->dev;
		inode->ino = ino;
		inode->bsize = getbsize(fs->dev);
		hashfunc(fs, ino)->enqhead(inode);
		assert(!inode->peer);
		assert(!inode->rwlock.locked());
		return inode;
	}
	return NULL;
}

/* iget */
int readi(fs_t * fs, ino_t ino, inode_t ** result)
{
	int e;

	if (!(*result = geti(fs, ino)))
		return ENFILE;
	if ((*result)->state != IINVAL)
		return 0;
	if (e = (*result)->read()) {
		(*result)->lose();
		(*result) = NULL;
		return e;
	}
	return 0;
}

/* iput */
void inode_t::lose()
{
	assert(findi(fs, ino));
	assert(refcnt > 0);

	if (--refcnt)
		return;
	if (!nextfree)	
		freeq.enqtail(this); /* make this inode as MRU */
	if (state == IINVAL)
		return;
	if (nlink)
		return;
	truncate();
	if (refcnt || nlink) {
		warn("nonexistent inode is referenced\n");
		return;
	}
	fs->freei(ino);
	state = IINVAL;
}

int inode_t::read()
{
	int e;
	buf_t * b;
	minixdi_t * di;

	lock();
	if (state != IINVAL) {
		unlock();
		return 0;
	}
	if (e = fs->seeki(ino, &b, &di)) {
		unlock();
		return e;
	}
	mode = di->mode;
	nlink = di->nlink;
	uid = di->uid;
	gid = di->gid;
	size = di->size;
	atime = di->atime;
	mtime = di->mtime;
	ctime = di->ctime;
	for (int i = 0; i < MINIXNDADDR; i++)
		daddr[i] = di->daddr[i];
	b->lose();
	state = ICLEAN;
	unlock();
	return 0;
}

int inode_t::write()
{
	int e;
	buf_t * b;
	minixdi_t * di;

	lock();
	if (state != IDIRTY) {
		unlock();
		return 0;
	}
	if (e = fs->seeki(ino, &b, &di)) {
		unlock();
		return e;
	}
	di->mode = mode;
	di->nlink = nlink;
	di->uid = uid; 
	di->gid = gid;
	di->size = size;
	di->atime = atime; 
	di->mtime = mtime;
	di->ctime = ctime;
	for (int i = 0; i < MINIXNDADDR; i++)
		di->daddr[i] = daddr[i];
	b->lose2();
	state = ICLEAN;
	unlock();
	return 0;
}

void synci()
{
	inode_t * inode;

	foreach (inode, allq) {
		inode->hold();
		inode->write();
		inode->lose();
	}
}

void synci(fs_t * fs)
{
	inode_t * inode;

	foreach (inode, allq) {
		if (inode->fs != fs)
			continue;
		inode->hold();
		inode->write();
		inode->lose();
	}
}

int tryumount(fs_t * fs)
{
	inode_t * inode;

	foreach (inode, allq) {
		if (inode->fs != fs)
			continue;
		if (inode == fs->root) {
			if (inode->refcnt != 1)
				return EBUSY;
		} else {
			if (inode->refcnt != 0)
				return EBUSY;
		}
	}
	return 0;
}

void invali(fs_t *fs)
{
	inode_t *inode;
	foreach (inode, allq) {
		if (inode->state == IINVAL || inode->fs != fs)
			continue;
		inode->state = IINVAL;
	}
}
