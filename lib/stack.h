#ifndef _LIBSTACK_H
#define _LIBSTACK_H

template <class elem_t, int NR>
struct stack_tl {
	elem_t elem[NR];
	int sp;

public: stack_tl()
	{
		sp = -1;
	}
	int empty() { return sp == -1; }
	int full() { return sp == NR-1; } 
	void push(elem_t e)
	{
		assert(!full());
		elem[++sp] = e;
	}
	int pop(elem_t * e)
	{
		if (empty())
			return 0;
		*e = elem[sp];
		--sp;
		return 1;
	}
};
#endif
