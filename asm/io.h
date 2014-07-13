#ifndef	_ASMIO_H
#define _ASMIO_H

extern inline void outb(u8_t val, u16_t port)
{
	asm volatile ("outb %%al,%%dx"::"a"(val),"d"(port));
}

extern inline void outbp(u8_t val, u16_t port)
{
	asm volatile ("outb %%al,%%dx\njmp 1f\n1:\njmp 1f\n1:"
		::"a"(val),"d"(port));
}

extern inline u8_t inb(u16_t port)
{
	u8_t val;
	asm volatile ("inb %%dx,%%al":"=a"(val):"d"(port));
	return val;
}

extern inline u8_t inbp(u16_t port)
{
	u8_t val;
	asm volatile ("inb %%dx,%%al\njmp 1f\n1:\njmp 1f\n1:"
		:"=a"(val):"d"(port));
	return val;
}

extern inline void outw(u16_t val, u16_t port)
{
	asm volatile ("outw %%ax,%%dx"::"a"(val), "d"(port));
}

extern inline u16_t inw(u16_t port)
{
	u16_t val;
	asm volatile ("inw %%dx,%%ax":"=a"(val):"d"(port));
	return val;
}

extern inline void outl(u32_t val, u16_t port)
{
	asm volatile ("outl %%eax,%%dx"::"a"(val), "d"(port));
}

extern inline u32_t inl(u16_t port)
{
	u32_t val;
	asm volatile ("inl %%dx,%%eax":"=a"(val):"d"(port));
	return val;
}

#define __INS(s) \
static inline void ins##s(unsigned short port, void * addr, unsigned long count) \
{ __asm__ __volatile__ ("rep ; ins" #s \
: "=D" (addr), "=c" (count) : "d" (port),"0" (addr),"1" (count)); }

#define __OUTS(s) \
static inline void outs##s(unsigned short port, const void * addr, unsigned long count) \
{ __asm__ __volatile__ ("rep ; outs" #s \
: "=S" (addr), "=c" (count) : "d" (port),"0" (addr),"1" (count)); }

__INS(b)
__INS(w)
__INS(l)

__OUTS(b)
__OUTS(w)
__OUTS(l)

#endif
