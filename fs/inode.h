#ifndef _FSINODE_H
#define _FSINODE_H

#include "minix.h"
#include "stat.h"
#include <lib/queue.h>
#include <kern/sched.h>
#include <lib/limits.h>
/* inode state */
enum {
	IINVAL,
	ICLEAN,
	IDIRTY,
};

/* inode flags */
enum {
	IMOUNT = 1, /* inode is mountpoint */
};

/* read/write/exec */
enum { IR = 4,  IW = 2,  IX = 1 };

#define BNOTALLOC ((bno_t)-1)

struct dirent_t;
struct buf_t;
struct dinode_t;
struct fdes_t;
struct fs_t;
struct unstsock_t;

struct inode_t {
	CHAIN(all,inode_t);
	CHAIN(hash,inode_t);
	CHAIN(free,inode_t);
	int state;
	int flags;
	int refcnt;
	fs_t * fs;
	dev_t dev;
	ino_t ino;
	int bsize;
	int bsizebits;
	inode_t * peer;
	unstsock_t * unstsock;
	IOLOCK;
	/* readdir VS writedir, readwriteregular VS truncate */
	rwlock_t rwlock;

	/* on disk part */
	mode_t mode;
	nlink_t	nlink;
	uid_t uid;
	gid_t gid;
	off_t size;
	time_t atime, mtime, ctime;
	u32_t daddr[MINIXNDADDR];

	dev_t getrdev() { return daddr[0]; }
	void setrdev(dev_t rdev) { daddr[0] = rdev; }
	bno_t bnodiv(off_t off) { return off >> bsizebits; }
	int bnomod(off_t off) { return off & (bsize-1); }
	off_t bnodown(off_t off) { return off & ~(bsize-1); }
	off_t bnoup(off_t off) { return bnodown(off+bsize-1); }
	int denyunlink(inode_t * inode);
	int deny(mode_t request);
	int denyuid(mode_t request);
	void hold()
	{
		assert(refcnt >= 0);
		refcnt++;
	}
	void lose();
	void setdirty() { state = IDIRTY; ctime = now(); }
	void lose2() { setdirty(); lose(); }
	int read();
	int write();
	void setsize(off_t newsize) { size = max(newsize, size); };
	int isdir() { return SISDIR(mode); };
	int isreg() { return SISREG(mode); };
	int ischr() { return SISCHR(mode); };
	int isblk() { return SISBLK(mode); };
	int isdev() { return ischr() || isblk(); };
	int isfifo() { return SISFIFO(mode); };
	int isroot() { return ino == MINIXROOTINO; } 
	int ismount() { return flags & IMOUNT; };
	fdes_t * open(int flags);

	/* operation on regular and block file */
	int allocindexb(minixzone_t * ptr);
	int allocdatab(minixzone_t * ptr);
	int rtranslate(int nindirect, minixzone_t index, bno_t offset, 
	    bno_t * result);
	int wtranslate(int nindirect, minixzone_t * index, bno_t offset, 
	    bno_t * result);
	int readmap(bno_t logic, bno_t * phys);
	int writemap(bno_t logic, bno_t * phys);
	void reada2(bno_t logic, int nr);
	void reada(bno_t logic, int nr);
	int read(off_t pos, void * buf, int len);
	int write(off_t pos, void * buf, int len);
	int truncate(int nindirect, minixzone_t * index);
	int truncate();

	/* operation on dir */
	int incnlink();
	void decnlink();
	int findde(const char * name, buf_t ** b, minixde_t ** de);
	int findde(ino_t ino, char *buf, char **end);
	int findde(const char * name);
	int addde(const char * name, buf_t ** b, minixde_t ** de);
	int addde(const char * name, ino_t newino); 

	int lookup2(const char * name, inode_t ** result);
	int lookup(const char * name, inode_t ** result);
	int hardlink2(const char * basename, inode_t * oldi);
	int hardlink(const char * basename, inode_t * oldi);
	int unlink2(const char * name);
	int unlink(const char * name);

	int mkcommon(const char * name, mode_t mode_, inode_t ** inode = NULL);
	int mkreg(const char * name, mode_t mode_, inode_t ** inode = NULL);
	int mkfifo(const char * name, mode_t mode_);
	int mknod(const char * name, mode_t mode_, dev_t dev);
	int mkdir(const char * name, mode_t mode_);

	int emptydir();
	int rmdir2(const char * name);
	int rmdir(const char * name);
	int rename2(char * oldname, inode_t * newdir, char * newname);
	int rename(char * oldname, inode_t * newdir, char * newname);
	int getdents(off_t * pos, dirent_t * de, int nbyte);
};
QUEUE(all,inode_t);
QUEUE(hash,inode_t);
QUEUE(free,inode_t);

/* flags for namei */
#define I2SPLIT 0x1
	
#define ICREAT  I2SPLIT
#define ILINK 	I2SPLIT
#define IUNLINK I2SPLIT
#define IRMDIR	I2SPLIT
#define IMKDIR	I2SPLIT
#define IMKNOD	I2SPLIT
#define ISTAT	0

extern int 
namei(int flags, const char * name, inode_t ** inode, char * basename=NULL);
extern inode_t * findi(fs_t * fs, ino_t ino);
extern int readi(fs_t * fs, ino_t ino, inode_t ** result);
extern void synci();
extern void synci(fs_t * fs);
extern int tryumount(fs_t * fs);
extern void invali(fs_t *fs);
extern int domkfifo(const char * pathname, mode_t mode); 
#endif
