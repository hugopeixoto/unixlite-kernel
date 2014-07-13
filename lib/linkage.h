#ifndef _LINUX_LINKAGE_H
#define _LINUX_LINKAGE_H

#define asmlinkage extern "C" __attribute__((regparm(0)))
#ifdef __ASSEMBLY__
#define ENTRY(name) globl name; .balign 16,0x90; name##: 
#endif

#endif
