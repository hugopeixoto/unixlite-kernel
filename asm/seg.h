#ifndef _ASM_SEGMENT_H
#define _ASM_SEGMENT_H

#define NULLIDX		0
#define KCODEIDX	1
#define KDATAIDX	2
#define LDTIDX		3
#define	TSS0IDX		4 
#define TSS1IDX  	5	
#define	NULLSEL		(NULLIDX <<3)
#define KCODESEL	(KCODEIDX << 3)
#define KDATASEL	(KDATAIDX << 3)
#define	LDTSEL		(LDTIDX << 3)
#define TSS0SEL		(TSS0IDX << 3)
#define TSS1SEL		(TSS1IDX << 3)

#define UCODEIDX	1
#define UDATAIDX	2
#define	UCODESEL	((UCODEIDX << 3) + 7)
#define	UDATASEL	((UDATAIDX << 3) + 7)

#ifndef __ASSEMBLY__
struct i387_t {
	u32_t cw; /* control word (16 bits) */
	u32_t sw; /* status word (16 bits) */
	u32_t tw; /* tag word (16 bits) */
	u32_t ip; /* instruction pointer */
	u16_t cs; /* code segment seleector */
	u16_t op; /* opcode last executed (11 bits) */
	u32_t oo; /* floating operand offset */
	u32_t os; /* floating operand segment selector */
	u8_t regs[80];
};

struct tss_t {
	u32_t	backlink;	/* 16 high bits zero */
	u32_t	esp0;
	u32_t	ss0;		/* 16 high bits zero */
	u32_t	esp1;
	u32_t	ss1;		/* 16 high bits zero */
	u32_t	esp2;
	u32_t	ss2;		/* 16 high bits zero */
	u32_t	cr3;
	u32_t	eip;
	u32_t	eflags;
	u32_t	eax;
	u32_t	ecx;
	u32_t	edx;
	u32_t	ebx;
	u32_t	esp;
	u32_t	ebp;
	u32_t	esi;
	u32_t	edi;
	u32_t	es;		/* 16 high bits zero */
	u32_t	cs;		/* 16 high bits zero */
	u32_t	ss;		/* 16 high bits zero */
	u32_t	ds;		/* 16 high bits zero */
	u32_t	fs;		/* 16 high bits zero */
	u32_t	gs;		/* 16 high bits zero */
	u32_t	ldt;		/* 16 high bits zero */
	u32_t	tracebitmap;	/* bits: trace 0, bitmap 16-31 */
};

struct segsel_t {
	unsigned rpl:2,		/* request privilege level */
		 ti:1,		/* table index : 0 select gdt,1 select ldt */
		 idx:13;	/* index */
} __attribute__((packed));

struct gdtdes_t {
	u16_t limit;
	u32_t base;
} __attribute__((packed));
typedef gdtdes_t idtdes_t;

/* type: for appseg -- 0x2 = read/write, 0xa = read/exec,
         for sysseg -- 0x2 = ldt, 0x9 = avl386tss, 0xe = 386intrgate, 
		       0xf = 386trapgate
   appseg: 1 = application segment, 0 = ldt/tss/gate 
   dpl: desriptor privilege level
   p: present */
struct segattr_t {
	unsigned type:4, appseg:1, dpl:2, p:1; 
};

/* application segment attribute byte */
#define X86UCODE 	0xfa
#define X86UDATA	0xf2
#define X86KCODE	0x9a
#define X86KDATA	0x92

/* system segment attribute byte */
#define X86LDT		0x82
#define X86AVL386TSS	0x89
#define X86386INTR	0x8e
#define X86386TRAP	0x8f
#define X86SYSCALL	0xef /* dpl=3, system call gate */

/* limlo: limit low
   limhi: limit high
   baselo: base low
   basemi: base middle
   basehi: base high
   avl: avalialbe bits
   d: default size, 0 = 16 bit, 1 = 32 bit
   g: granularity, 0 = byte, 1 = page */

struct appseg_t {
	u16_t limlo;
	u16_t baselo;
	u8_t basemi;
	u8_t attr; 
	u8_t limhi; /* unsigned limhi:4, avl:1, pad:1, d:1, g:1; */
	u8_t basehi;

	/* set application segment : code and data */
	void init(u8_t attr_, u32_t base, u32_t rawlim)
	{
		attr = attr_;
		baselo = base & 0xffff; 
		basemi = (base >> 16) & 0xff;
		basehi = base >> 24;
		limlo = rawlim & 0xffff;
		limhi = 0xc0 | (rawlim >> 16); /* g = 1, d = 1 */
	}
} __attribute__((packed));

struct sysseg_t {
	u16_t limlo;
	u16_t baselo;
	u8_t basemi;
	u8_t attr; 
	u8_t limhi; /* unsigned limhi:4, pad:3, g:1; */
	u8_t basehi;

	/* set system segment: ldt and tss */
	void init(u8_t attr_, u32_t base, u32_t rawlim)
	{
		attr = attr_;
		baselo = base & 0xffff; 
		basemi = (base >> 16) & 0xff;
		basehi = base >> 24;
		limlo = rawlim & 0xffff;
		limhi = 0 | (rawlim >> 16); /* g = 0, d = 0 */
	}
} __attribute__((packed));

struct sysgate_t {
	u16_t offlo;
	u16_t sel;
	u8_t dwordcount; /* unsigned dwordcount:5, pad:3; */
	u8_t attr;
	u16_t offhi;

	/* set system gate : interrupt gate, exception gate */ 
	void init(u8_t attr_, u16_t sel_, u32_t offset)
	{
		attr = attr_; 
		sel = sel_;
		offlo = offset & 0xffff;
		offhi = offset >> 16;
		dwordcount = 0;
	}
} __attribute__((packed));

struct gdt_t {
	appseg_t null;
	appseg_t kcode;
	appseg_t kdata;
	sysseg_t ldtdes;
	sysseg_t tss[2];
	void init();
	void swtch(tss_t * from, tss_t * to);
}__attribute__((aligned(8)));
extern gdt_t gdt;
extern inline void longjump(tss_t* from, tss_t* to)
{
	gdt.swtch(from, to);
}

struct ldt_t {
	appseg_t null;
	appseg_t ucode;
	appseg_t udata;
	void init();
}__attribute__((aligned(8)));

struct idt_t {
	sysgate_t sysgate[256];
	void init();
}__attribute__((aligned(8)));

#endif /* __ASSEMBLY__ */
#endif
