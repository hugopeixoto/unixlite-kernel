#include <lib/root.h>
#include <lib/errno.h>
#include "inode.h"
#include "fs.h"
#include "buf.h"

/* need not mark the indirect block buffer dirty */
int inode_t::truncate(int nindirect, minixzone_t * index)
{
	int e;
	buf_t * b;
	assert(nindirect > 0);
	if (*index == MINIXFREEBNO)
		return 0;
	if (e = readb(dev, *index, &b))
		return EIO;
	minixzone_t * z = (minixzone_t*) b->data, * end = z + MINIXPTRPERBLOCK;
	for (; z < end; z++) {
		if (nindirect == 1) {
			if (*z == MINIXFREEBNO)
				continue;
			fs->freeb(*z);
		} else {
			if (e = truncate(nindirect-1, z)) {
				b->lose();
				return e;
			}
		}
		*z = MINIXFREEBNO;
	}
	b->lose();
	fs->freeb(*index);
	*index = MINIXFREEBNO;
	return 0;
}

int inode_t::truncate()
{
	int e = 0;
	/* bno_t end = bnodiv(size + MINIXBSIZE - 1); */

	if (!isreg() && !isdir())
		return EINVAL;
	rwlock.wlock();
	for (int i = 0; i < MINIXEND0; i++) {
		if (daddr[i] != MINIXFREEBNO) {
			fs->freeb(daddr[i]);
			daddr[i] = MINIXFREEBNO;
		}
	}
	if (e = truncate(1, daddr + 7))
		goto exit;
	if (e = truncate(2, daddr + 8))
		goto exit;
	if (e = truncate(3, daddr + 9))
		goto exit;
	size = 0;
	setdirty();
exit:	rwlock.unlock();
	return e;
}
