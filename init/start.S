	.text
	.globl _start
_start:
	cli
	movl $bootstack+4096,%esp
	call main
1:	jmp 1b

	/* the initital bootstack must _not_ be put in .bss */
	.data
	.globl bootstack
bootstack: .long 0x19790106 
	.fill 4092,1,0
