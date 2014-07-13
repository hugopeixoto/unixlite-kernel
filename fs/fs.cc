#include <lib/root.h>
#include <lib/errno.h>
#include <lib/string.h>
#include <mm/allockm.h>
#include "buf.h"
#include "fs.h"
#include "inode.h"

int fs_t::seeki(ino_t ino, buf_t ** b, minixdi_t ** di)
{
	if (ino >= ninode) {
		warn("bad ino:%lx\n", ino);
		return EINVAL;
	}
	ino--; /* brain damage */
	bno_t bno = 2 + nimapblock + nzmapblock + 
		    ino * sizeof(minixdi_t) / MINIXBSIZE;
	int off = ino * sizeof(minixdi_t) % MINIXBSIZE;
	int e = readb(dev, bno, b);
	if (e)
		return e;
	*di = (minixdi_t*)((*b)->data + off);
	return 0;
}

#define BITS (13) /* number of bits per block */
#define MASK (8191)

int fs_t::allocb(bno_t * bno)
{
	int i,j;

	for (i = 0; i < nzmapblock; i++) {
		if ((j = zmap[i].ff0()) < 0)
			continue;
		zmap[i].set(j);
		*bno = (i<<BITS) + j + datazone - 1;
		zmapbuf[i]->state = BDIRTY;
		return 0; 
	}
	return ENOSPC;
}

void fs_t::freeb(bno_t bno)
{
	if (bno < datazone || bno >= nzone) {
		warn("block number out of range:%ld on dev:%x\n", bno, dev);
		return;
	}
	int i = bno - datazone + 1;
	if (!zmap[i>>BITS].tst(i&MASK)) {
		warn("block is already freed:%ld on dev:%x\n", bno, dev);
		return;
	}
	zmap[i>>BITS].clr(i&MASK);
	zmapbuf[i>>BITS]->state = BDIRTY;
}

int fs_t::alloci(ino_t * ino)
{
	int i, j;
	for (i = 0; i < nimapblock; i++) {
		if ((j = imap[i].ff0()) < 0)
			continue;
		imap[i].set(j);
		*ino = (i<<BITS) + j;
		imapbuf[i]->state = IDIRTY;
		return 0;
	}
	return ENOSPC; 
}

void fs_t::freei(ino_t ino)
{
	if (ino == 0 || ino >= ninode) {
		warn("ino out of range:%ld on dev:%x\n", ino, dev);
		return;
	}
	if (!imap[ino>>BITS].tst(ino&MASK)) {
		warn("inode is already freed:%ld on dev:%x\n", ino, dev);
		return;
	}
	imap[ino>>BITS].clr(ino&MASK);
	imapbuf[ino>>BITS]->state = IDIRTY;
}

void fs_t::freeimap()
{	
	int i;
	for (i = 0; i < nimapblock; i++) {
		if (imapbuf && imapbuf[i]) {
			imapbuf[i]->lose();
			imapbuf[i] = NULL;
			imap[i].~bitmap_t();
		}
	}
	if (imapbuf)
		freekm(imapbuf);
	if (imap)
		freekm(imap);
}

void fs_t::freezmap()
{
	int i;
	for (i = 0; i < nzmapblock; i++) {
		if (zmapbuf && zmapbuf[i]) {
			zmapbuf[i]->lose();
			zmapbuf[i] = NULL;
			zmap[i].~bitmap_t();
		}
	}
	if (zmapbuf)
		freekm(zmapbuf);
	if (zmap)
		freekm(zmap);
}

int fs_t::read()
{
	int e, i;
	buf_t * b;

	lock();
	if (e = readb(dev, 1, &b)) {
		unlock();
		return e;
	}
	minixdfs_t * dfs = (minixdfs_t*)(b->data);
	ninode = dfs->ninode;
	nzone16 = dfs->nzone16;
	nimapblock = dfs->nimapblock;
	nzmapblock = dfs->nzmapblock;
	datazone = dfs->datazone;
	logzonesize = dfs->logzonesize;
	maxfilesize = dfs->maxfilesize;
	magic = dfs->magic;
	state = dfs->state;
	nzone = dfs->nzone;
	b->lose();

	if (magic != MINIX2MAGIC2) {
		warn("%x:bad magic, it should be %x\n", magic, MINIX2MAGIC2);
		unlock();
		return EINVAL; 
	}
	if (nimapblock > 64 || nzmapblock > 64) {
		warn("filesytem too large\n");
		unlock();
		return ENOMEM;
	}


	readab(dev, 1, 1 + nimapblock + nzmapblock);
	imapbuf = (buf_t**) allockm(AKERN|ACLEAR, sizeof(buf_t*) * nimapblock);
	imap = (bitmap_t*) allockm(AKERN, sizeof(bitmap_t) * nimapblock);
	zmapbuf = (buf_t**) allockm(AKERN|ACLEAR, sizeof(buf_t*) * nzmapblock);
	zmap = (bitmap_t*) allockm(AKERN, sizeof(bitmap_t) * nzmapblock);
	int block = 2;

	for (i = 0; i < nimapblock; i++) {
		if (e = readb(dev, block++, imapbuf + i)) {
			freeimap();
			unlock();
			return e;
		}
		new (imap + i) bitmap_t(imapbuf[i]->data, 1<<BITS);
	}

	for (i = 0; i < nzmapblock; i++) {
		if (e = readb(dev, block++, zmapbuf + i)) {	
			freeimap();
			freezmap();
			unlock();
			return e;
		}
		new (zmap + i) bitmap_t(zmapbuf[i]->data, 1<<BITS);
	}

	imap[0].set(0);
	zmap[0].set(0);
	unlock();

	if (e = readi(this, MINIXROOTINO, &root)) {
		warn("unable to read root inode on dev:%x\n", dev);
		return e;
	}
	return 0;
}

int fs_t::write()
{
	int e;
	buf_t * b;

	lock();
	readab(dev, 1, 1 + nimapblock + nzmapblock);
	if (e = readb(dev, 1, &b)) {
		unlock();
		return e;
	}
	minixdfs_t * dfs = (minixdfs_t*)(b->data);
	dfs->ninode = ninode;
	dfs->nzone16 = nzone16;
	dfs->nimapblock = nimapblock;
	dfs->nzmapblock = nzmapblock;
	dfs->datazone = datazone;
	dfs->logzonesize = logzonesize;
	dfs->maxfilesize = maxfilesize;
	dfs->magic = magic;
	dfs->state = state;
	dfs->nzone = nzone;
	b->lose2();

	llwriteb(imapbuf, nimapblock);
	llwriteb(zmapbuf, nzmapblock);
	unlock();
	return 0;
}

