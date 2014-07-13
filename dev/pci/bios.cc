/* - adapted from linux/bios32.c
 * - memory mode is "flat mode", virtual address is *not* equal 
 *   physical address */
#include <lib/root.h>
#include <init/ctor.h>
#include <mm/layout.h>
#include <asm/seg.h>
#include "bios.h"

struct faraddr_t {
	u32_t vaddr;
	u16_t sel;
};

static faraddr_t biosaddr;
static faraddr_t pcibiosaddr;
bool biospresent = false;
bool pcibiospresent = false;

/* BIOS32 signature: "_32_" */
#define BIOSSIGN (('_' << 0) + ('3' << 8) + ('2' << 16) + ('_' << 24))
/* PCI signature: "PCI " */
#define PCISIGN (('P' << 0) + ('C' << 8) + ('I' << 16) + (' ' << 24))
/* PCI service signature: "$PCI" */
#define PCISERVSIGN (('$' << 0) + ('P' << 8) + ('C' << 16) + ('I' << 24))

struct biosserv_t {
        uchar base[0];
	ulong signature;        /* _32_ */
	paddr_t paddr;	        /* 32 bit physical address */
	uchar revision;	        /* Revision level, 0 */
	uchar length;	        /* Length in paragraphs should be 01 */
	uchar chksum;	        /* All bytes must add up to zero */
	uchar reserved[5];      /* Must be zero */
};

#define START ((biosserv_t*)ptov(0x0e0000))
#define END ((biosserv_t*)ptov(0x100000))
static void checkbiosaddr()
{
	biosaddr.sel = KCODESEL;
        for (biosserv_t * b = START; b < END; b++) {
                if (b->signature != BIOSSIGN)
                        continue;
                uchar sum = 0;
                for (uchar *c = b->base; c < b->base + b->length *16; c++) 
                        sum += *c;
                if (sum != 0)
			panic("bios32: check sum error %x\n", sum);
		printf("bios32 service directory structure at 0lx%x\n", vtop(b));
		printf("bios32 service directory at 0x%lx\n", b->paddr);
		biosaddr.vaddr = ptov(b->paddr);
                biospresent = true;
                return;
        } 
};

static int callbiosserv(ulong serv, paddr_t * paddr)
{
	uchar ret;		/* %al */
	ulong base;		/* %ebx */
	ulong len;		/* %ecx */
	ulong offset;		/* %edx */

	asm volatile("lcall *(%%edi)"
		: "=a" (ret),
		  "=b" (base),
		  "=c" (len),
		  "=d" (offset)
		: "0" (serv),
		  "1" (0),
		  "D" (&biosaddr));
        if (ret == 0)
                *paddr = base + offset;
        else if (ret == 0x80)
                printf("bios32 service(%ld) is not present\n", serv);
        else
                panic("call bios32 failed for unknown reasion\n");
        return ret;
}

static void checkpcibiosaddr() 
{	
        if (!biospresent)
                return;
	pcibiosaddr.sel = KCODESEL;
        paddr_t paddr;
        if (callbiosserv(PCISERVSIGN, &paddr) != 0) 
                return;
        pcibiosaddr.vaddr = ptov(paddr);

	ulong present;
	ulong revision;	/* bh = major; bl = minor; */
	ulong sign;
	asm volatile("lcall *(%%edi)\n\t"
		"jc 1f\n\t"	/* cf clear mean bios present */
		"xor %%ah, %%ah\n\t"
		"1:\n\t"
		:"=a"(present),
		 "=b"(revision),
		 "=d"(sign) 
		:"0"(BIOS_PCI_BIOS_PRESENT),
		 "D"(&pcibiosaddr)
		:"memory");
	if (!present || (sign != PCISIGN)) { 
		printf("pci bios is not present");
	        return;
	}
	printf("PCI BIOS revision %x.%02x entry at 0x%lx\n",
	(revision >> 8) & 0xff, revision & 0xff, pcibiosaddr.vaddr);
        pcibiospresent = true;
}

__ctor(PRIDEV, SUB1ST, initpci)
{
        checkbiosaddr();
        checkpcibiosaddr();
}

/* A driver can query multiple devices with the same class code by starting with 
   index = 0,1,2,... and calling this function until PCI_DEVICE_NOT_FOUND is returned. */
int pcifindclass(ulong classcode, ushort index, uchar* bus, uchar* devfn)
{
	ulong ebx;
	ulong ret;

	asm volatile("lcall *(%%edi)\n\t"
		"jc 1f\n\t"
		"xor %%ah, %%ah\n"
		"1:"
		: "=b" (ebx),
		  "=a" (ret)
		: "1" (BIOS_FIND_PCI_CLASS_CODE),
		  "c" (classcode),
		  "S" ((int) index),
		  "D" (&pcibiosaddr));
	*bus = (ebx >> 8) & 0xff;
	*devfn = ebx & 0xff;
	return (int) (ret & 0xff00) >> 8;
};

int pcifinddev(ushort vendor, ushort devid, ushort index, uchar* bus, uchar* devfn)
{
	ushort ebx;
	ushort ret;

	asm volatile("lcall *(%%edi)\n\t"
		"jc 1f\n\t"
		"xor %%ah, %%ah\n"
		"1:"
		: "=b" (ebx),
		  "=a" (ret)
		: "1" (BIOS_FIND_PCI_DEV),
		  "c" (devid),
		  "d" (vendor),
		  "S" ((int) index),
		  "D" (&pcibiosaddr));
	*bus = (ebx >> 8) & 0xff;
	*devfn = ebx & 0xff;
	return (int) (ret & 0xff00) >> 8;
}

int pcirdcfgb(uchar bus, uchar devfn, uchar where, uchar* value)
{	
	ulong ret;
	ulong ebx = (bus << 8) | devfn;

	asm volatile("lcall *(%%esi)\n\t"
		"jc 1f\n\t"
		"xor %%ah, %%ah\n"
		"1:"
		: "=c" (*value),
		  "=a" (ret)
		: "1" (BIOS_RDCFGB),
		  "b" (ebx),
		  "D" ((long) where),
		  "S" (&pcibiosaddr));
	return (int) (ret & 0xff00) >> 8;
}

int pcirdcfgw(uchar bus, uchar devfn, uchar where, ushort* value)
{
	ulong ret;
	ulong bx = (bus << 8) | devfn;

	asm volatile("lcall *(%%esi)\n\t"
		"jc 1f\n\t"
		"xor %%ah, %%ah\n"
		"1:"
		: "=c" (*value),
		  "=a" (ret)
		: "1" (BIOS_RDCFGW),
		  "b" (bx),
		  "D" ((long) where),
		  "S" (&pcibiosaddr));
	return (int) (ret & 0xff00) >> 8;
}

int pcirdcfgd(uchar bus, uchar devfn, uchar where, ulong* value)
{
	ulong ret;
	ulong bx = (bus << 8) | devfn;

	asm volatile("lcall *(%%esi)\n\t"
		"jc 1f\n\t"
		"xor %%ah, %%ah\n"
		"1:"
		: "=c" (*value),
		  "=a" (ret)
		: "1" (BIOS_RDCFGD),
		  "b" (bx),
		  "D" ((long) where),
		  "S" (&pcibiosaddr));
	return (int) (ret & 0xff00) >> 8;
}

int pciwrcfgb(uchar bus, uchar devfn, uchar where, uchar value)
{
	ulong ret;
	ulong bx = (bus << 8) | devfn;

	asm volatile("lcall *(%%esi)\n\t"
		"jc 1f\n\t"
		"xor %%ah, %%ah\n"
		"1:"
		: "=a" (ret)
		: "0" (BIOS_WRCFGB),
		  "c" (value),
		  "b" (bx),
		  "D" ((long) where),
		  "S" (&pcibiosaddr));
	return (int) (ret & 0xff00) >> 8;
}

int pciwrcfgw(uchar bus, uchar devfn, uchar where, ushort value)
{
	ulong ret;
	ulong bx = (bus << 8) | devfn;

	asm volatile("lcall *(%%esi)\n\t"
		"jc 1f\n\t"
		"xor %%ah, %%ah\n"
		"1:"
		: "=a" (ret)
		: "0" (BIOS_WRCFGW),
		  "c" (value),
		  "b" (bx),
		  "D" ((long) where),
		  "S" (&pcibiosaddr));
	return (int) (ret & 0xff00) >> 8;
}

int pciwrcfgd(uchar bus, uchar devfn, uchar where, ulong value)
{
	ulong ret;
	ulong bx = (bus << 8) | devfn;

	asm volatile("lcall *(%%esi)\n\t"
		"jc 1f\n\t"
		"xor %%ah, %%ah\n"
		"1:"
		: "=a" (ret)
		: "0" (BIOS_WRCFGD),
		  "c" (value),
		  "b" (bx),
		  "D" ((long) where),
		  "S" (&pcibiosaddr));
	return (int) (ret & 0xff00) >> 8;
}
