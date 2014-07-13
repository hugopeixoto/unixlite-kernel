#ifndef _FSMINIX_H
#define _FSMINIX_H

#define MINIXBSIZE 1024
#define MINIXROOTINO 1
#define MINIXFREEINO 0
#define MINIXFREEBNO 0

#define MINIX2MAGIC2 0x2478 /* V2 && namelen == 30 */

#define MINIXNDADDR 10
#define MINIXEND0 7
#define MINIXEND1 (MINIXEND0+256)
#define MINIXEND2 (MINIXEND1+256*256)
#define MINIXEND3 (MINIXEND2+256*256*256)

typedef u32_t minixzone_t;
#define MINIXPTRPERBLOCK (MINIXBSIZE/sizeof(minixzone_t))

/* disk inode */
struct minixdi_t {
	u16_t mode;
	u16_t nlink;
	u16_t uid;
	u16_t gid;
	u32_t size;
	u32_t atime;
	u32_t mtime;
	u32_t ctime;
	u32_t daddr[MINIXNDADDR];
};

#include <lib/string.h>
#define MINIXNAMELEN 29
/* dir entry */
struct minixde_t {
	u16_t ino;
	str_tl <MINIXNAMELEN+1> name;

	int nameequ(const char * target) { return !strcmp(name.get(), target); }
};

/* file system on disk */
struct minixdfs_t {
	u16_t ninode;
	u16_t nzone16;	
	u16_t nimapblock;	/* block nr of inode map */
	u16_t nzmapblock;	/* block nr of zone map */
	u16_t datazone;		/* first data zone */
	u16_t logzonesize;	/* should be zero */
	u32_t maxfilesize;
	u16_t magic;
	u16_t state;
	u32_t nzone;		/* V2 introduce this new field */ 
};

#endif
