#include "bootparam.h"
#include "bios.h"
#include "../asm/system.h"

	.code16
	.text
	cli
	jmp start16

loadkernstr:	.asciz "load kernel, please wait:"
dotstr:		.asciz "."
ioerrstr:	.asciz "io error."
enterpmodestr:	.asciz "enter protected mode and verify check sum."
cksumfail:	.asciz "verify cksum fail"
cksumsucc:	.asciz "verify cksum succ"
wordbuf:	.asciz "0123"
longbuf:	.asciz "01234567"
line:		.long 0
	.balign 8
gdtdes: .word	0x800		# gdt limit=2048,256 GDT entries
	.long	SETUPPA+gdt
	.balign	8			
gdt:	.word	0,0,0,0		# dummy
	.word	0xFFFF		# limit=4GB
	.word	0x0000		# base address=0
	.word	0x9A00		# code read/exec
	.word	0x00CF		# granularity=4096,386
	.word	0xFFFF		# limit=4GB
	.word	0x0000		# base address=0
	.word	0x9200		# data read/write
	.word	0x00CF		# granularity=4096,386

start16:movw $SETUPSEG,%ax
	movw %ax,%ds
	movw %ax,%es
	movw %ax,%ss
	movw $XINITIALSP,%sp

	pushw $loadkernstr
	call outstr
	call loadkern
	call gethddata
	call getextmemk
	call enablea20
	call reprogram8259

	pushw $enterpmodestr
	call outstr
	lgdt gdtdes
	movl %cr0,%eax
	orl $1,%eax /* enable protect */
	movl %eax,%cr0
	ljmpl $8,$SETUPPA+start32

#define ENTER pushw %bp; movw %sp,%bp; pushaw; pushw %es
#define LEAVE(n) popw %es; popaw; popw %bp; ret $n
outstr:
	ENTER
	movw 4(%bp),%si
	movw $VIDEOSEG,%ax
	movw %ax,%es
	imulw $BYTEPERLINE,line,%di
	movb $ERRORCHAR,%ah
	cld
1:	lodsb
	stosw
	testb %al,%al
	jnz 1b
	incw line
	LEAVE(2)

outword:
	ENTER
	movw $SETUPSEG,%ax
	movw %ax,%es
	movw 4(%bp),%bx
	movw $wordbuf+3,%di
	movw $4,%cx
	std
1:	movw %bx,%ax
	andw $0xf,%ax
	movb $ERRORCHAR,%ah
	cmpb $9,%al
	jbe 2f
	addb $'A'-'9'-1,%al
2:	addb $'0',%al
	stosb
	shrw $4,%bx
	loop 1b
	pushw $wordbuf
	call outstr 
	LEAVE(2)

outlong:
	ENTER
	movw $SETUPSEG,%ax
	movw %ax,%es
	movl 4(%bp),%ebx
	std
	movw $longbuf+7,%di
	movw $8,%cx
1:	movw %bx,%ax
	andw $0xf,%ax
	movb $ERRORCHAR,%ah
	cmpb $9,%al
	jbe 2f
	addb $'A'-'9'-1,%al
2:	addb $'0',%al
	stosb
	shrl $4,%ebx
	loop 1b
	pushw $longbuf
	call outstr 
	LEAVE(4)

#define NSECT 18
#define ENDSECT (NSECT+1)
#define NHEAD 2
#define tmp si
#define nsectw ax /* maximum sectors transfered during one IO */
#define leftsect di /* left sectors needed to transfer */
#define min(a,b) cmpw a,b; ja 8f; movw b,a; 8:	/* assign the min value to a */

loadkern:
	pushw %es
	movl XKERNSIZE,%eax
	shrl $9,%eax
	movw %ax,%leftsect
	movw $KERNPA0/65536,%ax
	movw %ax,%es
	movw $KERNPA0%65536,%bx
	movb $DISKA,%drive
	movb $0,%cyl
	movb $0,%head
	movb $2+SETUPSIZE/SECTSIZE,%sect /* sect count from 1 */
	
gogogo: /* nsect = MIN(ENDSECT - sect, leftsect, (64K - %bx)/SECTSIZE */
	movw $ENDSECT,%nsectw
	subb %sect,%nsect
	min(%nsectw,%leftsect)
	test %bx,%bx
	jz 1f

	xorw %tmp,%tmp
	subw %bx,%tmp
	shrw $SECTBITS,%tmp
	min(%nsectw,%tmp)

1:	movb $READ,%ah
	int $0x13
	jnc 1f 
	pushw $ioerrstr
	call outstr
	hlt
1:	movb $0,%ah /* clear the exitstat */

	/* leftsect -= nsect */
	subw %nsectw,%leftsect
	jz exit

	/* bx += nsect * 512 */
	movw %nsectw,%tmp
	shlw $SECTBITS,%tmp
	addw %tmp,%bx
	jnc 1f 
	movw %es,%tmp
	addw $0x1000,%tmp
	movw %tmp,%es

	/* sect += nsect, update the sect/head/cyl */
1:	addb %nsect,%sect
	cmpb $NSECT+1,%sect
	jnz gogogo 
	movb $1,%sect
	inc %head
	cmp $NHEAD,%head
	jnz gogogo 
	xorb %head,%head
	incb %cyl
	jmp gogogo 

	/* stop motor */
	xorw	%ax, %ax		# reset FDC
	xorb	%dl, %dl
	int	$0x13

exit:	popw %es
	ret

gethddata:
	pushw %ds
	xorw %ax,%ax
	movw %ax,%ds
	lds 4*0x41,%si
	movw $XHDDATA,%di
	movw $16,%cx
	cld; rep; movsb	

	xorw %ax,%ax
	movw %ax,%ds
	lds 4*0x46,%si
	movw $XHDDATA+16,%di
	movw $16,%cx
	cld; rep; movsb
	popw %ds
	ret
        
/* Phoenix BIOS v4.0 - GET MEMORY SIZE FOR >64M CONFIGURATIONS
   AX = E801h
   Return:

   CF clear if successful
   AX = extended memory between 1M and 16M, in K (max 3C00h = 15MB)
   BX = extended memory above 16M, in 64K blocks
   CX = configured memory 1M to 16M, in K
   DX = configured memory above 16M, in 64K blocks
   CF set on error 

   reference at http://www.ctyme.com/intr/rb-1739.htm  */
   
getextmemk:
        movw $0xe801,%ax
        int $0x15
        jc int1588
        andl $0xffff,%ebx
        shll $6,%ebx
        andl $0xffff,%eax
        addl %eax,%ebx
        movl %ebx,XEXTMEMK 
        ret

int1588:movb $0x88,%ah
	int $0x15
	movw %ax,XEXTMEMK
	ret

/* 
 * This routine checks that the keyboard command queue is empty
 * No timeout is used - if this hangs there is something wrong with
 * the machine,and we probably couldn't proceed anyway.
 */
empty8042:
	.word 0x00eb,0x00eb
	inb $0x64,%al		/* 8042 status port */
	testb $2,%al		/* is input buffer full? */
	jnz empty8042		/* yes - loop */
	ret

enablea20:
	call empty8042
	movb $0xD1,%al		/* command write */
	outb %al,$0x64
	call empty8042
	movb $0xDF,%al		/* A20 on */
	out %al,$0x60
	call empty8042
	ret

#define outbp(val, port) \
	movb val,%al; \
	outb %al,port; \
	.word 0x00eb,0x00eb	/* jmp $+2,jmp $+2 */

reprogram8259:
	outbp($0x11,$0x20)	/* initialization sequence */
	outbp($0x11,$0xA0)
	outbp($0x20,$0x21)	/* start of hardware int's (0x20) */
	outbp($0x28,$0xA1)  	/* start of hardware int's (0x28) */
	outbp($0x04,$0x21)	/* 8259-1 is master */
	outbp($0x02,$0xA1)	/* 8259-2 is slave */ 
	outbp($0x01,$0x21)	/* 8086 mode for both */
	outbp($0x01,$0xA1)
	outbp($0xff,$0x21)	/* mask off all the interrupt */
	outbp($0xff,$0xA1)
	ret
	
/*****************************************************************************/
start32:
	.code32
	movw $0x10,%ax 
	movw %ax,%ds
	movw %ax,%es
	movw %ax,%ss
	movl $SETUPPA+XINITIALSP,%esp
	call movekern
	call verifykerncksum
	call startpaging

movekern:
	movl $KERNPA0,%esi
	movl $KERNPA1,%edi
	movl SETUPPA+XKERNSIZE,%ecx
	shrl $2,%ecx
	cld; rep; movsl
	ret

verifykerncksum:
	xorl %ebx,%ebx
	movl $KERNPA1,%esi
	movl SETUPPA+XKERNSIZE,%ecx
	shrl $2,%ecx
	cld
0:	lodsl
	addl %eax,%ebx
	loop 0b
	cmpl SETUPPA+XKERNCKSUM,%ebx
	jz 1f

	movb $'x',%al
	movb $ERRORCHAR,%ah
	movl $VIDEOPA,%edi
	movl $1*80,%ecx
	cld; rep; stosw
	hlt
1:	ret

#define PGDIR (SETUPPA+XPGDIR)
#define PG0 (SETUPPA+XPG0)
startpaging:
	xorl %eax,%eax
	movl $PGDIR,%edi
	movl $1024,%ecx
	cld; rep; stosl
	movl $3+PG0,PGDIR
	movl $3+PG0,PGDIR+(KERNSTART>>20)

	movl $3,%eax		/* page present,writable, system page */
	movl $PG0,%edi
	movl $1024,%ecx
	cld
9:	stosl
	addl $4096,%eax
	loop 9b

	movl $PGDIR,%eax
	movl %eax,%cr3
	movl %cr0,%eax
	andl $~(CR0EM|CR0TS),%eax /* setup i387 */
	orl $(CR0PG|CR0WP|CR0MP),%eax  /* enable paging and write-protect */
	movl %eax,%cr0
	jmp 1f 
1:	movw $0x4779,%ax
	movw %ax,0xb8002
	movl SETUPPA+XKERNENTRY,%eax
	jmp *%eax
	hlt
