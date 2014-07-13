#ifndef _LIBSTAT_H
#define _LIBSTAT_H

/* liunx 2.2 /usr/include/asm-i386/stat.h */
struct stat_t { /* kernel stat */
	unsigned short dev;
	unsigned short __pad1;
	unsigned long ino;
	unsigned short mode;
	unsigned short nlink;
	unsigned short uid;
	unsigned short gid;
	unsigned short rdev;
	unsigned short __pad2;
	unsigned long  size;
	unsigned long  blksize;
	unsigned long  blocks;
	unsigned long  atime;
	unsigned long  __unused1;
	unsigned long  mtime;
	unsigned long  __unused2;
	unsigned long  ctime;
	unsigned long  __unused3;
	unsigned long  __unused4;
	unsigned long  __unused5;
};

/* This matches struct stat64 in glibc2.1, hence the absolutely
 * insane amounts of padding around dev_t's.
 */
struct stat64_t {
        unsigned short  dev;
        unsigned char   __pad0[10];

#define STAT64_HAS_BROKEN_ST_INO        1
        unsigned long   __ino;

        unsigned int    mode;
        unsigned int    nlink;

        unsigned long   uid;
        unsigned long   gid;

        unsigned short  rdev;
        unsigned char   __pad3[10];

        long long       size;
        unsigned long   blksize;

        unsigned long   blocks;      /* Number 512-byte blocks allocated. */
        unsigned long   __pad4;      /* future possible blocks high bits */

        unsigned long   atime;
        unsigned long   __pad5;

        unsigned long   mtime;
        unsigned long   __pad6;

        unsigned long   ctime;
        unsigned long   __pad7;      /* will be high 32 bits of ctime someday */

        unsigned long long      ino;
};

#if 0
struct stat_t { /* uclibc stat */
	u16_t dev;
	u8_t padw[10];
	u32_t ino;
	u32_t mode;
	u32_t nlink;
	u32_t uid;
	u32_t gid;
	u16_t rdev;
	u8_t padl[10];
	u32_t size;
	u32_t blksize;
	u32_t blocks;
	u32_t atime;
	u32_t : 32;
	u32_t mtime;
	u32_t : 32;
	u32_t ctime;
	u32_t : 32;
	u32_t : 32;
	u32_t : 32;
};
#endif

#define SIFMT  00170000
#define SIFREG  0100000
#define SIFBLK  0060000
#define SIFDIR  0040000
#define SIFCHR  0020000
#define SIFIFO  0010000

#define SISUID  0004000
#define SISGID  0002000
#define SISVTX  0001000

#define SISREG(m)	(((m) & SIFMT) == SIFREG)
#define SISDIR(m)	(((m) & SIFMT) == SIFDIR)
#define SISCHR(m)	(((m) & SIFMT) == SIFCHR)
#define SISBLK(m)	(((m) & SIFMT) == SIFBLK)
#define SISFIFO(m)	(((m) & SIFMT) == SIFIFO)

#define SIRWXU 00700
#define SIRUSR 00400
#define SIWUSR 00200
#define SIXUSR 00100

#define SIRWXG 00070
#define SIRGRP 00040
#define SIWGRP 00020
#define SIXGRP 00010

#define SIRWXO 00007
#define SIROTH 00004
#define SIWOTH 00002
#define SIXOTH 00001

#endif
