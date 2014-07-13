#ifndef _INITCTOR_H
#define _INITCTOR_H

#define MAXCTOR 256 /* enough for now */

/* primary order */
#define PRIQ            200
#define PRIMM		300
#define PRIDEV		400
#define PRIFS		500
#define PRIKERN		600
#define PRINET          700
#define PRIEND		800
#define PRIANY		900

/* subordinate order */
#define SUB1ST		1
#define SUB2ND		2
#define SUB3RD		3
#define SUBEND		4
#define SUBANY		5

#define __ctor2(pri,sub,desc) void __ctor1979_##pri##sub##_##desc ()
#define __ctor(pri,sub,desc) __ctor2(pri,sub,desc)

#endif
