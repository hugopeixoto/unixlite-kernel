#include "bootparam.h"
#include "bios.h"

/* bootsect is loaded into 0000:7c00 (not 07c0:0000), its will 
   load setup into SETUPSEG, then setup contiue to load kernel */
	.code16
	.text

	cli
	ljmp $0x7c0,$next
next:	pushw %cs
	popw %ss
	xorw %sp,%sp
	movw $SETUPSEG,%ax
	movw %ax,%es
	xorw %bx,%bx
	movb $READ,%service
	movb $DISKA,%drive
	movb $0,%cyl
	movb $0,%head
	movb $2,%sect		/* sector count from 1 */
	movb $SETUPSIZE/SECTSIZE,%nsect
	int $0x13
ioerr:	jc ioerr
	ljmp $SETUPSEG,$0

	.org 510
magic: .word 0xaa55
