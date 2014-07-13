#ifndef	_ASMPAGE_H
#define _ASMPAGE_H

#define PAGESIZE 4096
#define PAGEBITS 12
#define PAGE0011 (PAGESIZE - 1)
#define PAGE1100 (~PAGE0011)

/* round page up/down */
#define cast(type, value) ((type)(value))

#define pageup(addr) \
cast(typeof(addr), (cast(ulong, addr) + PAGESIZE - 1) & PAGE1100)
#define pagedown(addr) cast(typeof(addr), cast(ulong, addr) & PAGE1100)
#define pagemul(addr) cast(typeof(addr), cast(ulong, addr) << PAGEBITS)
#define pagediv(addr) cast(typeof(addr), cast(ulong, addr) >> PAGEBITS)
#define pagemod(addr) cast(typeof(addr), cast(ulong, addr) & PAGE0011) 

#if 0
#define pageup(addr) (((ulong)(addr) + PAGESIZE - 1) & PAGE1100)
#define pagedown(addr) ((ulong)(addr) & PAGE1100)
#define pagemul(addr) ((ulong)(addr) << PAGEBITS)
#define pagediv(addr) ((ulong)(addr) >> PAGEBITS)
#define pagemod(addr) ((ulong)(addr) & PAGE0011)
#endif

#define clearpage(addr) memset((void*)addr,0,PAGESIZE)

/* PDECOVERED determines what a third-level page table entry can map */
#define PDECOVERED (1UL<<22)

extern inline int pdeidx(ulong vaddr)
{
	return (vaddr >> 22);
}

extern inline int pteidx(ulong vaddr)
{
	return (vaddr >> 12) & 1023; 
}

#endif
