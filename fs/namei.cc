#include <lib/root.h>
#include <lib/gcc.h>
#include <lib/limits.h>
#include <lib/string.h>
#include <kern/sched.h>
#include "inode.h"

static int advancei(inode_t ** cwd, char * name)
{
	inode_t * sub;
	int e;

	if (e = (*cwd)->lookup(name, &sub))
		return e;
	(*cwd)->lose();
	(*cwd) = sub;
	return 0;
}

int namei(int flags, const char * pathname, inode_t ** inode, char * basename)
{
	int e;
	scanstr_t scan(pathname, "/");
	inode_t * cwd;

	if (flags & I2SPLIT) {
		assert(basename);
		basename[0] = 0;
	}
	*inode = NULL;
	if ((scan.nlefttok() == 0) && (flags & I2SPLIT))
		return ENOENT;
	cwd = (*pathname == '/') ? curr->fs->root : curr->fs->cwd;
	allege(cwd);
	cwd->hold();

	for (; scan.more(); scan.next()) {
		if (!cwd->isdir()) {
			cwd->lose();
			return ENOTDIR;
		}
		if ((scan.nlefttok() == 1) && (flags & I2SPLIT)) {
			*inode = cwd;
			strncpy(basename, scan.curtok(), NAMEMAX+1);
			return 0;
		}
		if (e = advancei(&cwd, scan.curtok())) {
			cwd->lose();
			return e;
		}
	}
	*inode = cwd;
	return 0;
}

asmlinkage int syslink(const char * oldname, const char * newname)
{
	int e;
	inode_t * oldi, * newdir;
	char newbasename[NAMEMAX+1];

	if (lowfreepage())
		return ENOMEM;
	if ((e = verrpath(oldname)) || (e = verrpath(newname)))
		return e;
	if (e = namei(ISTAT, oldname, &oldi))
		return e;
	if (e = namei(ILINK, newname, &newdir, newbasename)) {
		oldi->lose();
		return e;
	}
	e = newdir->hardlink(newbasename, oldi);
	oldi->lose();
	newdir->lose();
	return e;
}

asmlinkage int sysunlink(const char * pathname)
{
	int e;
	inode_t * dir;
	char basename[NAMEMAX+1];

	if (lowfreepage())
		return ENOMEM;
	if (e = verrpath(pathname))
		return e;
	if (e = namei(IUNLINK, pathname, &dir, basename))
		return e;
	e = dir->unlink(basename);
	dir->lose();
	return e;
}

int domkfifo(const char * pathname, mode_t mode)
{
	int e;
	inode_t * dir;
	char basename[NAMEMAX+1];

	if (e = namei(IMKNOD, pathname, &dir, basename))
		return e;
	e = dir->mkfifo(basename, mode);
	dir->lose();
	return e;
}

asmlinkage int sysmknod(const char * pathname, int mode, int dev)
{
	int e;
	inode_t * dir;
	char basename[NAMEMAX+1];

	if (lowfreepage())
		return ENOMEM;
	if (e = verrpath(pathname))
		return e;
	if (e = namei(IMKNOD, pathname, &dir, basename))
		return e;
	e = dir->mknod(basename, mode, dev);
	dir->lose();
	return e;
}

asmlinkage int sysmkdir(const char * pathname, int mode)
{
	int e;
	inode_t * dir;
	char basename[NAMEMAX+1];

	if (e = verrpath(pathname))
		return e;
	if (e = namei(IMKDIR, pathname, &dir, basename))
		return e;
	e = dir->mkdir(basename, mode);
	dir->lose();
	return e;
}

asmlinkage int sysrmdir(const char * pathname)
{
	int e;
	inode_t * dir;
	char basename[NAMEMAX+1];

	if (lowfreepage())
		return ENOMEM;
	if (e = verrpath(pathname))
		return e;
	if (e = namei(IRMDIR, pathname, &dir, basename))
		return e;
	e = dir->rmdir(basename);
	dir->lose();
	return e;
}

asmlinkage int sysrename(const char * oldname, const char * newname)
{
	int e;
	inode_t * olddir, * newdir;
	char oldbasename[NAMEMAX+1], newbasename[NAMEMAX+1];

	if ((e = verrpath(oldname)) || (e = verrpath(newname)))
		return e;
	if (e = namei(IUNLINK, oldname, &olddir, oldbasename))
		return e;
	if (e = namei(IMKNOD, newname, &newdir, newbasename)) {
		olddir->lose();
		return e;
	}
	/* olddir may equal newdir */
	e = olddir->rename(oldbasename, newdir, newbasename);
	olddir->lose();
	newdir->lose();
	return e;
}

#define getstat(i, s) \ 
	s->dev = i->dev; \
	s->ino = i->ino; \
	s->mode = i->mode; \
	s->nlink = i->nlink; \
	s->uid = i->uid; \
	s->gid = i->gid; \
	s->rdev = i->getrdev(); \
	s->size = i->size; \
	s->blksize = MINIXBSIZE; \
	s->blocks = i->bnodiv(i->size); \
	s->atime = i->atime; \
	s->mtime = i->mtime; \
	s->ctime = i->ctime; \

asmlinkage int sysstat(const char * pathname, stat_t * stat)
{
	int e;
	inode_t * leaf;

	if (e = verrpath(pathname))
		return e;
	if (e = verw(stat, sizeof(stat_t)))
		return e;
	if (e = namei(ISTAT, pathname, &leaf))
		return e;
	getstat(leaf, stat);
	leaf->lose();
	return 0;
}

asmlinkage int sysstat64(const char * pathname, stat64_t * stat)
{
	int e;
	inode_t * leaf;

	if (e = verrpath(pathname))
		return e;
	if (e = verw(stat, sizeof(stat_t)))
		return e;
	if (e = namei(ISTAT, pathname, &leaf))
		return e;
	getstat(leaf, stat);
	leaf->lose();
	return 0;
}

asmlinkage int syslstat(const char * pathname, stat_t * stat)
{
	return sysstat(pathname, stat);
}

#include "regfd.h"
asmlinkage int sysfstat(int fd, stat_t * stat)
{
	int e;
	fdes_t * fdes;

	if (e = verw(stat, sizeof(stat_t)))
		return e;
	if (e = fdtofdes(fd, &fdes))
		return e;
#warning "to be continue..."
	/* some hack for gawk */
	if (fdes->type != REGFD) {
		stat->blksize = 1024;
		return 0;
	}
	getstat(((regfd_t*)fdes)->inode, stat);
	return 0;
}

asmlinkage int syschdir(const char * pathname)
{
	int e;
	inode_t * inode;

	if (e = verrpath(pathname))
		return e;
	if (e = namei(ISTAT, pathname, &inode))
		return e;
	if (!inode->isdir()) {
		inode->lose();
		return ENOTDIR;
	}
	if (inode->deny(IX)) {
		inode->lose();
		return EACCES;
	}
	curr->fs->cwd->lose();
	curr->fs->cwd = inode;
	return 0;
}

asmlinkage int sysfchdir(int fd)
{
	fdes_t *fdes;
	int e = fdtofdes(fd, &fdes);
	if (e < 0)
		return e;
	if (fdes->type != REGFD) 
		return ENOTDIR;
	regfd_t *regfd = (regfd_t*)fdes;
	if (!regfd->inode->isdir())
		return ENOTDIR;

	inode_t * inode = regfd->inode;
	if (inode->deny(IX))
		return EACCES;
	inode->hold();
	curr->fs->cwd->lose();
	curr->fs->cwd = inode;
	return 0;
}

asmlinkage int syschmod(const char * pathname, mode_t mode)
{
	int e;
	inode_t * inode;

	if (e = verrpath(pathname))
		return e;
	if (e = namei(ISTAT, pathname, &inode))
		return e;
	if ((curr->euid != inode->uid) && !suser()) {
		inode->lose();
		return EPERM;
	}
	inode->mode = (mode & 07777) | (inode->mode & ~07777);
	inode->lose2();
	return 0;
}

asmlinkage int syschown(const char * pathname, uid_t uid, gid_t gid)
{
	int e;
	inode_t * inode;

	if (e = verrpath(pathname))
		return e;
	if (e = namei(ISTAT, pathname, &inode))
		return e;
	if (!suser()) {
		inode->lose();
		return EPERM;
	}
	inode->uid = uid;
	inode->gid = gid;
	inode->lose2();
	return 0;
}

asmlinkage int syschroot(const char * pathname)
{
	int e;
	inode_t * inode;

	if (e = verrpath(pathname))
		return e;
	if (e = namei(ISTAT, pathname, &inode))
		return e;
	if (!inode->isdir()) {
		inode->lose();
		return ENOTDIR;
	}
	if (!suser()) {
		inode->lose();
		return EPERM;
	}
	curr->fs->root->lose();
	curr->fs->root = inode;
	return (0);
}

struct utimbuf_t {
	time_t atime;
	time_t mtime;
};
/* If times==NULL, set access and modification to current time,
 * must be owner or have write permission.
 * Else, update from *times, must be owner or super user.
 */
asmlinkage int sysutime(char * pathname, utimbuf_t * t)
{
	int e;
	inode_t * inode;

	if (e = verrpath(pathname))
		return e;
	if (e = namei(ISTAT, pathname, &inode))
		return e;
	if (e = inode->deny(IW)) {
		inode->lose();
		return EPERM;
	}
	if (t) {
		if (e = verr(t, sizeof(*t))) {
			inode->lose();
			return e;
		}
		inode->atime = t->atime;
		inode->mtime = t->mtime;
	} else {
		inode->atime = inode->mtime = now();
	}
	inode->lose2();
	return 0;
}

/*
 * XXX we should use the real ids for checking _all_ components of the
 * path.  Now we only use them for the final component of the path.
 */
asmlinkage int sysaccess(const char * pathname,int mode)
{
	int e;
	inode_t * inode;

	if (e = verrpath(pathname))
		return e;
	if (mode != (mode & 7))
		return EINVAL;
	if (e = namei(ISTAT, pathname, &inode))
		return e;
	e = inode->deny(mode);
	inode->lose();
	return e;
}

asmlinkage int sysumask(int mask)
{
	int old = curr->fs->umask;

	curr->fs->umask = mask & 0777; 
	return old;
}

/*
* NOTE! The user-level library version returns a
* character pointer. The kernel system call just
* returns the length of the buffer filled (which
* includes the ending '\0' character), or a negative
* error value. So libc would do something like
*
*      char *getcwd(char * buf, size_t size)
*      {
*              int retval;
*
*              retval = sys_getcwd(buf, size);
*              if (retval >= 0)
*                      return buf;
*              errno = -retval;
*              return NULL;
*      }
*/
static int backward(inode_t ** cwd, char * buf, char **end)
{
	inode_t * child = *cwd;
	if (child->isroot() && child->peer) { /* cross from root dir to mountpoint */ 
		child->lose();
		(child = child->peer)->hold();
	}
	inode_t * parent;
	int e;

	if (e = child->lookup2("..", &parent))
		return e;
	if (e = parent->findde(child->ino, buf, end))
		return e;
	child->lose();
	*cwd = parent;
	return 0;
}

asmlinkage int sysgetcwd(char * buf, size_t size)
{
	int e;

	if (e = verw(buf, size))
		return e;
	buf[--size] = 0;
	char * end = buf + size;
	inode_t * cwd = curr->fs->cwd;
	cwd->hold();
	while (cwd != curr->fs->root) {
		if (e = backward(&cwd, buf, &end)) {
			cwd->lose();
			return e;
		}
	}
	cwd->lose();
	strcpy(buf, end);
	return end - buf;
}
