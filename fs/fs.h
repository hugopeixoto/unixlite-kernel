#ifndef _FS_H
#define _FS_H

#include "minix.h"
#include <kern/sched.h>
#include <lib/queue.h>
struct buf_t;
struct inode_t;
struct bitmap_t;

/* minix file system layout:
   boot block
   super block
   inode bitmap
   zone bitmap
   inode table 
   data zone */

struct fs_t {
	CHAIN(,fs_t);
	dev_t dev;
	int bsize;
	IOLOCK;
	inode_t * mount;
	inode_t * root;
	/* minix specific */
	u16_t ninode;
	u16_t nzone16;		/* this field is obsolete in V2 */
	u16_t nimapblock;	/* block nr of inode map */
	u16_t nzmapblock;	/* block nr of zone map */
	u16_t datazone;		/* first data zone */
	u16_t logzonesize;	/* should be zero */
	u32_t maxfilesize;
	u16_t magic;
	u16_t state;
	u32_t nzone;		/* V2 introduce this new field */ 
	buf_t ** imapbuf;
	buf_t ** zmapbuf;
	bitmap_t * imap;
	bitmap_t * zmap;

	fs_t(dev_t dev_);
	~fs_t();
	void freeimap();
	void freezmap();
	int read();
	int write();
	int allocb(bno_t * ino);
	void freeb(bno_t bno);
	int alloci(ino_t * ino);
	void freei(ino_t ino);
	int seeki(ino_t ino, buf_t ** b, minixdi_t ** di);
};
QUEUE(,fs_t);

#endif
