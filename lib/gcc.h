#ifndef _LIBGCC_H
#define _LIBGCC_H

#define asmlinkage extern "C" __attribute__((regparm(0)))
#ifndef __ASSEMBLY__
extern void checkoffsetof();
#endif

#ifdef __STDC__
#define STRCAT(a,b) a##b
#else
#define STRCAT(a,b) a/**/b
#endif

#ifdef __ASSEMBLY__
#define ALIGN .balign 16,0x90
#define ENTRY(name) \
	.globl name; \
	ALIGN; \
	STRCAT(name,:)
#endif

/*
class base_class
{
public:
	long base_magic;
	virtual void interface() = 0;
};

class derive_class
{
public:
	long derive_magic;
	void interface(); 
};

  There is no standard about where c++ compiler insert the the virtual table
poninter inside the object, but many macros defined in include/queue.h expect 
the virtual table ponter appears after the last member of the base class.
  The following function doesn't work if compiled by microsoft c++, because 
microsoft c++ insert the virtual table pointer before the first member of the 
base class.

void check_vtbl_ptr()
{
	derive_class * derive = new base_class();
	derive->base_magic = BASE_MAGIC;
	derive->derive_magic = DERIVE_MAGIC;

	long * l = (long *) derive;
	if ((sizeof(base_class) == 2 * sizeof(long)) && 
	    (sizeof(derive_class) == sizeof(long) &&
	    (l[0] == BASE_MAGIC) &&  
            l[1] is the virtual table pointer
            (l[2] == DERIVE_MAGIC))
		printk("check virtual table pointer passed \n");
	else
		panic("BAD: gcc dones't insert the virtual table pointer at desired address\n");
};
*/

#endif
