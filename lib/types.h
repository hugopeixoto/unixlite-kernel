#ifndef _LIBTYPES_H
#define _LIBTYPES_H

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned long ulong;
typedef unsigned int uint;

typedef	uchar u8_t;
typedef ushort u16_t;
typedef ulong u32_t;
typedef char s8_t;
typedef short s16_t;
typedef long s32_t;

typedef int pid_t;
typedef ushort uid_t;
typedef ushort gid_t;

typedef ushort dev_t;
typedef ulong ino_t;
typedef ushort mode_t;
typedef ushort nlink_t;

typedef ulong off_t;
typedef uint size_t;

typedef long time_t;
typedef long tick_t;

typedef ulong vaddr_t;
typedef ulong paddr_t;

typedef long bno_t; /* logical or phyical block number */
typedef void (*vfp_t)(void * object);

#endif
