#ifndef _ASMSYSTEM_H
#define _ASMSYSTEM_H

#define EFLAGCF		(1)
#define EFLAGPF		(1 << 3)
#define EFLAGIF		(1 << 9)
#define EFLAGIOPL

#define CR0PE		(1)
#define CR0MP		(1 << 1)
#define CR0EM		(1 << 2)
#define CR0TS		(1 << 3)
#define CR0WP		(1 << 16)
#define CR0PG		(1 << 31)

#ifndef __ASSEMBLY__

extern inline int interruptflag()
{
	int i;

	asm volatile ("pushfl; popl %0":"=r"(i));
	return i & EFLAGIF;
}

extern inline void cli()
{
	asm volatile ("cli":::"memory");
}

extern inline void sti()
{
	asm volatile ("sti");
}

struct psw_t { /* processor state word */
	ulong data;
	void save(); 
	void restore(); 
};

extern inline void psw_t::save()
{
	asm volatile ("pushfl; popl %0":"=r"(data)::"memory"); 
}

extern inline void psw_t::restore()
{	
	asm volatile ("pushl %0; popfl"::"r"(data):"memory"); 
}

extern inline int interruptflagon()
{
	u32_t eflags;
	__asm__ volatile ("pushfl\n"
			  "popl	%0\n"
			  :"=m"(eflags)); 
	return eflags & EFLAGIF; 
}

extern inline void flushall()
{
	ulong tmpreg;
	asm volatile ("movl %%cr3,%0\nmovl %0,%%cr3\n":"=r"(tmpreg)::"memory");
}

extern inline void flushone(ulong vaddr)
{
	asm volatile ("invlpg %0"::"m"(*(char*)vaddr)); 
}
#endif
#endif
