#include <lib/root.h>
#include <lib/string.h>
#include <mm/allocpage.h>
#include <asm/system.h>
#include "fifoq.h"

void fifoq_t::init(char * room_, int size_)
{
	nbyte = 0;
	r = w = room = room_; 
	eroom = room_ + size_;
	size = size_;
	mask = size_ - 1;
	assert(ispowerof2(size_));
}

/* multiple room is not supported yet */
fifoq_t::fifoq_t(char * room_, int size_)
{
	nbyte = 0;
	r = w = room = room_; 
	eroom = room_ + size_;
	size = size_;
	mask = size_ - 1;
	assert(ispowerof2(size_));
}

int fifoq_t::peekc(char * c)
{
	if (empty())
		return 0;
	*c = *r;
	return 1;
}

int fifoq_t::getc(char * c)
{
	if (empty())
		return 0;
	*c =  *r;
	adjustrptr(1);
	return 1;
}

int fifoq_t::ungetc(char c)
{
	if (full())
		return 0;
	adjustrptr(-1);
	*r = c;
	return 1;
}

int fifoq_t::putc(char c)
{
	if (full())
		return 0;
	*w = c;
	adjustwptr(1);
	return 1;
}

int fifoq_t::unputc(char * c)
{
	if (empty())
		return 0;
	adjustwptr(-1);
	*c = *w;
	return 1;
}

int fifoq_t::peekc(char * buf, int len)
{
	int result = getc(buf, len);
	adjustrptr(-result);
	return result;
}

int fifoq_t::getc(char * buf, int len)
{
	int chunk, readed = 0;

	while (len && nchar()) {
		chunk = min(len, maxrchunk());
		memcpy(buf, r, chunk);
		adjustrptr(chunk);
		buf += chunk;
		len -= chunk;
		readed += chunk;
	}
	return readed;
}

int fifoq_t::putc(char * buf, int len)
{
	int chunk, written = 0;

	while (len && space()) {
		chunk = min(len, maxwchunk());
		memcpy(w, buf, chunk);
		adjustwptr(chunk);
		buf += chunk;
		len -= chunk;
		written += chunk;
	}
	return written;
}
