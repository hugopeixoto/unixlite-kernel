#include <lib/config.h>
#include <lib/root.h>
#include <lib/string.h>
#include <lib/stack.h>
#include <kern/sched.h>
#include "buf.h"
#include "inode.h"
#include "fs.h"

void inode_t::reada2(bno_t logic, int nr)
{
#if RWAHEAD
	buf_t * b[16];
	bno_t elogic = logic + nr, phys;
	int e; 

	allege(nr < 16);
	for (nr = 0; logic < elogic; logic++) {
		if ((e = readmap(logic, &phys)) || (phys == BNOTALLOC))
			continue;
		b[nr++] = getb(dev, phys);
	}
	llreadab(b, nr);
	while (nr-- > 0)
		b[nr]->lose();
#endif
}

void inode_t::reada(bno_t logic, int nr)
{
	rwlock.rlock();
	reada2(logic, nr);
	rwlock.unlock();
}

#define READASIZE 4
#define READAMASK 3

int inode_t::read(off_t pos, void * buf, int len)
{
	int e;
	bno_t phys;
	buf_t * b;

	if (!isdir() && !isreg() && !isblk())
		return EINVAL;
	if (pos >= size)
		return 0;
	rwlock.rlock();
	void * obuf = buf, * ebuf = buf + min(size-pos, len);
	while (buf < ebuf) {
		bno_t logic = bnodiv(pos);
		off_t off = bnomod(pos);
		if ((logic & READAMASK) == 0)
			reada2(logic, READASIZE);
		int chars = min(bsize - off, (char*)ebuf - (char*)buf);
		if (e = readmap(logic, &phys)) {
			rwlock.unlock();
			return e;
		}
		if (phys == BNOTALLOC)
			memset(buf, 0, chars);
		else {
			if (e = readb(dev, phys, &b)) {
				rwlock.unlock();
				return e;
			}
			memcpy(buf, b->data + off, chars);
			b->lose();
		}
		pos += chars, buf += chars;
	}
	atime = now();
	rwlock.unlock();
	return (char*)buf - (char*)obuf;
}

int inode_t::write(off_t pos, void * buf, int len)
{
	buf_t * b;
	bno_t phys; 
	int e;

	if (!isdir() && !isreg() && !isblk())
		return EINVAL;
	rwlock.rlock();
	void * obuf = buf, * ebuf = buf + len;
	while (buf < ebuf) {
		bno_t logic = bnodiv(pos); 
		off_t off = bnomod(pos); 
		int chars = min(bsize - off, (char*)ebuf - (char*)buf);
		if (e = writemap(logic, &phys)) {
			rwlock.unlock();
			return e;
		}
		if ((chars == bsize) || (!off && (pos+chars)>=size))
			b = getb(dev, phys);
		else { 
			if (e = readb(dev, phys, &b)) {
				rwlock.unlock();
				return e;
			}
		}
		memcpy(b->data + off, buf, chars);
		b->lose2();
		pos += chars, buf += chars;
		if (pos > size) {
			size = pos;
			setdirty();
		}
	}
	atime = mtime = now();
	rwlock.unlock();
	return (char*)buf - (char*)obuf;
}

/* we don't clear the newly-allocated data block */
int inode_t::allocdatab(minixzone_t * ptr)
{
	int e;
	bno_t bno;

	if (*ptr != MINIXFREEBNO)
		return 0;
	if (e = fs->allocb(&bno)) /* may sleep */
		return e;
	if (*ptr == MINIXFREEBNO) {
		*ptr = bno;
		return 0;
	}
	fs->freeb(bno);
	return 0;
}

int inode_t::allocindexb(minixzone_t * ptr)
{
	int e;
	bno_t bno;
	buf_t * b;

	if (*ptr != MINIXFREEBNO)
		return 0;
	if (e = fs->allocb(&bno)) /* may sleep */
		return e;
	b = getb(dev, bno); /* may sleep */
	memset(b->data, 0, bsize);
	b->lose2();
	if (*ptr == MINIXFREEBNO) {
		*ptr = bno;
		return 0;
	}
	fs->freeb(bno);
	return 0;
}

#define PTRSIZE (sizeof(minixzone_t))
#define PTRPERBLOCK (MINIXBSIZE/PTRSIZE)

#define NBIT 8
#define MASK 255

static inline minixzone_t * zone(buf_t * b)
{
	return (minixzone_t*) (b->data);
}

int inode_t::rtranslate
(int nindirect, minixzone_t index, bno_t offset, bno_t * result)
{
	int e;
	buf_t * b;

	assert(nindirect>0);
	nindirect *= NBIT;
	while ((index != MINIXFREEBNO) && ((nindirect -= NBIT)>=0)) {
		if (e = readb(dev, index, &b))
			return e;
		index = *(zone(b) + ((offset>>nindirect)&MASK));
		b->lose();
	}
	if (index == MINIXFREEBNO) {
		*result = BNOTALLOC;
		return 0;
	}
	*result = index;
	return 0;
}

int inode_t::wtranslate
(int nindirect, minixzone_t * indexp, bno_t offset, bno_t * result)
{
	int e = 0;
	buf_t * b;
	stack_tl<buf_t*, 8> stack;

	if (*indexp == MINIXFREEBNO) {
		ctime = now();
		setdirty();
	}
	nindirect *= NBIT;
	while ((nindirect -= NBIT) >= 0) {
		if (e = allocindexb(indexp))
			goto error;
		if (e = readb(dev, *indexp, &b))
			goto error;
		indexp = zone(b) + ((offset>>nindirect)&MASK);
		if (*indexp == MINIXFREEBNO)
			b->state = BDIRTY;
		/* because indexp reference someplace inside b, so the buffer
		   can't be released now */
		stack.push(b);
	}
	e = allocdatab(indexp);
	*result = *indexp; /* result is undefined if error occurs */
error:	while (stack.pop(&b))
		b->lose();
	return e;
}

#if 0
int inode_t::wtranslate
(int nindirect, minixzone_t * indexp, bno_t offset, bno_t * result)
{
	int e;
	buf_t * b;
	minixzone_t index;

	if (*indexp == MINIXFREEBNO) {
		ctime = now();
		setdirty();
	}
	e = nindirect ? allocindexb(indexp) : allocdatab(indexp);
	if (e)
		return e;
 	index = *indexp;
	nindirect *= NBIT;
	while ((nindirect -= NBIT) >= 0) {
		assert(index != MINIXFREEBNO);
		if (e = readb(dev, index, &b))
			return e;
		indexp = zone(b) + ((offset>>nindirect)&MASK);
		int empty = (*indexp == MINIXFREEBNO);
		e = nindirect ? allocindexb(indexp) : allocdatab(indexp);
		if (e) {
			b->lose();
			return e;
		}
		index = *indexp;
		empty ? b->lose2(): b->lose();
	}
	*result = index;
	return 0;
}
#endif

/* assume the caller hold the rwlock */
int inode_t::readmap(bno_t logic, bno_t * phys)
{
	if (!isdir() && !isreg() && !isblk())
		return EINVAL;
	if (isblk()) {
		*phys = logic;
		return 0;
	}
	/* readmap on regular file */
	if (logic < MINIXEND0) {
		*phys = (daddr[logic]) ? daddr[logic] : BNOTALLOC;
		return 0;
	}
	if (logic < MINIXEND1) {
		logic -= MINIXEND0;
		return rtranslate(1, daddr[7], logic, phys);
	}
	if (logic < MINIXEND2) {
		logic -= MINIXEND1;
		return rtranslate(2, daddr[8], logic, phys);
	}
	if (logic < MINIXEND3) {
		logic -= MINIXEND2;
		return rtranslate(3, daddr[9], logic, phys);
	}
	warn("logic block number(%ld) is too big", logic);
	return EINVAL;
}

/* assume the caller hold the rwlock */
int inode_t::writemap(bno_t logic, bno_t * phys)
{
	if (!isdir() && !isreg() && !isblk())
		return EINVAL;
	if (isblk()) {
		*phys = logic;
		return 0;
	}
	if (logic < MINIXEND0) {
		return wtranslate(0, daddr+logic, logic, phys);
	}
	if (logic < MINIXEND1) {
		logic -= MINIXEND0;
		return wtranslate(1, daddr+7, logic, phys);
	}
	if (logic < MINIXEND2) {
		logic -= MINIXEND1;
		return wtranslate(2, daddr+8, logic, phys);
	}
	if (logic < MINIXEND3) {
		logic -= MINIXEND2;
		return wtranslate(3, daddr+9, logic, phys);
	}
	warn("logic block number(%ld) is too big", logic);
	return EINVAL;
}
