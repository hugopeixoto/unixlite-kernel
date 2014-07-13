#include <lib/root.h>
#include <lib/string.h>
#include <lib/gcc.h>
#include "irq.h"
#include "seg.h"

gdt_t gdt;
static gdtdes_t gdtdes = {sizeof(gdt_t)-1, (u32_t)&gdt};
static idt_t idt;
static idtdes_t idtdes = {sizeof(idt_t)-1, (u32_t)&idt};
static ldt_t ldt; /* all the process share the same ldt */

void gdt_t::init()
{
	memset(&null, 0, 8);
	kcode.init(X86KCODE, 0, 0xfffff);
	kdata.init(X86KDATA, 0, 0xfffff);
	ldtdes.init(X86LDT, (u32_t)&ldt, sizeof(ldt_t)-1);
	tss[0].init(X86AVL386TSS, 0, sizeof(tss_t)-1);
	tss[1].init(X86AVL386TSS, 0, sizeof(tss_t)-1);
}

void gdt_t::swtch(tss_t * from, tss_t * to)
{
	/* also clear "busy tss" flag */
	tss[0].init(X86AVL386TSS, (u32_t)from, sizeof(tss_t)-1);
	tss[1].init(X86AVL386TSS, (u32_t)to, sizeof(tss_t)-1);
	asm volatile(
		"ltr %w0\n\t"
		"ljmp %1,$0\n\t"
		::"r"(TSS0SEL),"i"(TSS1SEL));
}

void ldt_t::init()
{
	memset(&null, 0, 8);
	ucode.init(X86UCODE, 0, 0xfffff);
	udata.init(X86UDATA, 0, 0xfffff);
}

asmlinkage void trap0x00();
asmlinkage void trap0x01();
asmlinkage void trap0x02();
asmlinkage void trap0x03();
asmlinkage void trap0x04();
asmlinkage void trap0x05();
asmlinkage void trap0x06();
asmlinkage void trap0x07();
asmlinkage void trap0x08();
asmlinkage void trap0x09();
asmlinkage void trap0x0a();
asmlinkage void trap0x0b();
asmlinkage void trap0x0c();
asmlinkage void trap0x0d();
asmlinkage void trap0x0e();
asmlinkage void trap0x0f();
asmlinkage void trap0x10();
asmlinkage void trap0x11();
asmlinkage void trap0x12();

asmlinkage void irq0x00();
asmlinkage void irq0x01();
asmlinkage void irq0x02();
asmlinkage void irq0x03();
asmlinkage void irq0x04();
asmlinkage void irq0x05();
asmlinkage void irq0x06();
asmlinkage void irq0x07();
asmlinkage void irq0x08();
asmlinkage void irq0x09();
asmlinkage void irq0x0a();
asmlinkage void irq0x0b();
asmlinkage void irq0x0c();
asmlinkage void irq0x0d();
asmlinkage void irq0x0e();
asmlinkage void irq0x0f();

asmlinkage void syscallentry();

#define __trap(trapno) \
sysgate[trapno].init(X86386TRAP, KCODESEL, (u32_t) trap##trapno)

#define __irq(irqno) \
sysgate[0x20+irqno].init(X86386INTR, KCODESEL, (u32_t) irq##irqno)

void idt_t::init()
{
	__trap(0x00);
	__trap(0x01);
	__trap(0x02);
	__trap(0x03);
	__trap(0x04);
	__trap(0x05);
	__trap(0x06);
	__trap(0x07);
	__trap(0x08);
	__trap(0x09);
	__trap(0x0a);
	__trap(0x0b);
	__trap(0x0c);
	__trap(0x0d);
	__trap(0x0e);
	__trap(0x0f);
	__trap(0x10);
	__trap(0x11);
	__trap(0x12);

	__irq(0x00);
	__irq(0x01);
	__irq(0x02);
	__irq(0x03);
	__irq(0x04);
	__irq(0x05);
	__irq(0x06);
	__irq(0x07);
	__irq(0x08);
	__irq(0x09);
	__irq(0x0a);
	__irq(0x0b);
	__irq(0x0c);
	__irq(0x0d);
	__irq(0x0e);
	__irq(0x0f);

	sysgate[0x80].init(X86SYSCALL, KCODESEL, (u32_t)syscallentry);
}

void i386init()
{
	idt.init();
	asm volatile ("lidt %0"::"m"(idtdes));
	gdt.init();
	asm volatile(
		"lgdt %0\n\t"
		"ljmp %1,$0f\n\t"
		"0: mov %2,%%ss\n\t"
		"mov %2,%%ds\n\t"
		"mov %2,%%es\n\t"
		::"m"(gdtdes),"i"(KCODESEL),"r"(KDATASEL));
	ldt.init();
	asm volatile("lldt %w0"::"r"(LDTSEL));
}
