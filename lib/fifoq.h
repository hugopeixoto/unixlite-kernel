#ifndef _LIBFIFOQ_H
#define _LIBFIFOQ_H

#include <asm/page.h>
class fifoq_t {
	char * room, * eroom;
	char * r, * w; /* read/write pointer */
	int nbyte;
	int mask;
	int size;

	void check()
	{	
	#if DEBUG
		assert(r >= room && r < eroom);
		assert(w >= room && w < eroom);
		if (r < w)
			assert(nbyte == w - r);
		if (w < r)
			assert(nbyte == size + w - r);
		if (r == w)
			assert(nbyte == 0 || nbyte == size);
	#endif
	}
	int maxrchunk() 
	{ 
		return min(nbyte, eroom - r);
	}
	int maxwchunk() 
	{ 
		return min(size - nbyte, eroom - w);
	}
	int offset(int n) { return n & mask; }
	/* adjust read pointer after read nr bytes */
	void adjustrptr(int nr) 
	{
		check();
		assert(nr <= nchar());
		r = room + offset(r - room + nr);
		nbyte -= nr;
	}
	/* adjust write pointer after write nr bytes */
	void adjustwptr(int nr) 
	{
		check();
		assert(nr <= space());
		w = room + offset(w - room + nr);
		nbyte += nr;
	}

public: void init(char * room_, int size_);
	fifoq_t() {};
	fifoq_t(char * room_, int size_);
	~fifoq_t() {};
	int nchar() { return nbyte; };
	int space() { return size - nbyte; };
	int empty() { return !nchar(); };
	int full() { return !space(); };
	int peekc(char * c);
	int getc(char * c);
	int ungetc(char c);
	int putc(char c);
	int unputc(char * c);
	int peekc(char * buf, int len);
	int getc(char * buf, int len);
	int putc(char * buf, int len);
	void clear() { init(room, size); };
};

template <int SIZE> class fifoq_tl : public fifoq_t {
	char house[SIZE];
public:	fifoq_tl() : fifoq_t(house, SIZE) { ; }
};

/*
class fifopageq_t : public fifoq_t {
	char * house; 
public: fifopageq_t() : fifoq_t();
	~fifopageq_t();
};
*/

#endif
