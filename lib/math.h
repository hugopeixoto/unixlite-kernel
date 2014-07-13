#ifndef _LIBMATH_H
#define _LIBMATH_H

#ifndef _LIBROOT_H
#error "don't include root.h directly"
#endif

#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))
#define minmax(min_, mid, max_) max(min_, min(mid, max_))
#define between(a, b, c) ((a)<=(b) && (b)<(c))

#define roundup(size, balign) (((size)+(balign)-1)/(balign)*(balign))
#define rounddown(size, balign) ((size)/(balign)*(balign))
#define ispowerof2(balign) ((((balign)-1)&(balign))==0)

#define roundup2(size, balign) \
({	assert(ispowerof2(balign)); \
	((size)+(balign)-1)&(~((balign)-1)); \
})

#define rounddown2(size, balign) \
({	assert(ispowerof2(balign)); \
	(size)&(~((balign)-1)); \
})

/* index of the most significant bit */
extern inline int idxofmsb(int size)
{
	if (!size)
		return -1;
	int idx;
	asm ("bsrl %1,%0":"=r"(idx):"r"(size));
	return idx;
}

/* index of the least significant bit */
extern inline int idxoflsb(int size)
{
	if (!size)
		return -1;
	int idx;
	asm ("bsfl %1,%0":"=r"(idx):"r"(size));
	return idx;
}

template <class type>
extern inline type roundup2p2(type size)
{
	if (ispowerof2(size))
		return size;
	return 1 << (idxofmsb(size) + 1);
}

template <class type>
extern inline type rounddown2p2(type size)
{
	if (!size)
		return size;
	return 1 << idxofmsb(size);
}

#endif
