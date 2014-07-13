#ifndef _LIBBITMAP_H
#define _LIBBITMAP_H

#include <lib/string.h>

/*
 * Some hacks to defeat gcc over-optimizations..
 */
struct hugestruct_t { unsigned long a[100]; };
#define BITS (*(hugestruct_t*)bits)

extern inline int tstbit(void * bits, int i)
{
	int oldbit;
	asm volatile ("btl %1,%2\n\tsbbl %0,%0"
		:"=r"(oldbit):"Ir"(i),"m"(BITS));
	return oldbit;
}

extern inline void setbit(void * bits, int i)
{
	asm volatile ("btsl %1,%0":"=m"(BITS):"Ir"(i));
}

extern inline void clrbit(void * bits, int i)
{
	asm volatile ("btrl %1,%0":"=m"(BITS):"Ir"(i));
}

extern inline void chgbit(void * bits, int i)
{
	asm volatile ("btcl %1,%0":"=m"(BITS):"Ir"(i));
}

#undef BITS

extern inline int ffbit0(void * bits, int nbit)
{
	assert(nbit % 32 == 0);
	long * l = (long *)bits, * end = ((long *)bits) + nbit/32;
	for(;l < end; l++) {
		if (*l == -1) 
			continue;
		int idx;
		asm ("notl %1\nbsfl %1,%0":"=r"(idx):"r"(*l));
		return (l - (long*)bits) * 32 + idx;
	}
	return -1;
}

extern inline int ffbit1(void * bits, int nbit)
{
	assert(nbit % 32 == 0);
	long * l = (long *)bits, * end = ((long *)bits) + nbit/32;
	for(;l < end; l++) {
		if (*l == 0) 
			continue;
		int idx;
		asm ("bsfl %1,%0":"=r"(idx):"r"(*l));
		return (l - (long*)bits) * 32 + idx;
	}
	return -1;
}

extern int bit0perchar[];

extern inline int countbit0(void * bits, int nbit)
{
	assert(nbit % 32 == 0);
	int sum = 0;
	char * c = (char*) bits, * end = ((char *)bits) + nbit/8;
	for (; c < end; c++)
		sum += bit0perchar[*c & 0xf] + bit0perchar[(*c >> 4) & 0xf];
	return sum;
}

extern inline int countbit1(void * bits, int nbit)
{
	assert(nbit % 32 == 0);
	return nbit - countbit0(bits, nbit);
}

template<int NR> class bitmap_tl {
	long bits[NR/32];
public:
	bitmap_tl()
	{
		assert(NR % 32 == 0);
		memset(bits, 0, NR/32);
	}
	int tst(int i)
	{
		assert(i < NR);
		return tstbit(bits, i);
	}
	void set(int i)
	{
		assert(i < NR);
		setbit(bits, i);
	}
	void clr(int i)
	{
		assert(i < NR);
		clrbit(bits, i);
	}
	void chg(int i)
	{
		assert(i < NR);
		chgbit(bits, i);
	}
	int count0()
	{
		return countbit0(bits, NR);
	}
	int count1()
	{
		return NR - count0();
		return 0;
	}
	int ff0()
	{
		return ffbit0(bits, NR);
	}
	int ff1()
	{
		return ffbit1(bits, NR);
	}
};

class bitmap_t {
	int nbit0;
	int nbit;
	void * bits;
public:
	bitmap_t(void * bits_, int nbit_)
	{
		assert(nbit_ % 32 == 0);
		nbit0 = countbit0(bits_, nbit_);
		nbit = nbit_;
		bits = bits_;
	}
	int tst(int i)
	{
		assert(i < nbit);
		return tstbit(bits, i);
	}
	void set(int i)
	{
		assert(i < nbit);
		if (!tst(i))
			nbit0--;
		setbit(bits, i);
	}
	void clr(int i)
	{
		assert(i < nbit);
		if (tst(i))
			nbit0++;
		clrbit(bits, i);
	}
	void chg(int i)
	{
		assert(i < nbit);
		if (tst(i))
			nbit0++;
		else
			nbit0--;
		chgbit(bits, i);
	}
	int count0()
	{
		return nbit0; 
	}
	int count1()
	{
		return nbit - nbit0;
	}
	int ff0()
	{
		if (!nbit0)
			return -1;
		return ffbit0(bits, nbit);
	}
	int ff1()
	{
		if (nbit0 == nbit)
			return -1;
		return ffbit1(bits, nbit);
	}
};

#endif
