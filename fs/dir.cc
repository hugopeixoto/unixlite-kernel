#include <lib/root.h>
#include <lib/errno.h>
#include <lib/string.h>
#include <lib/limits.h>
#include <kern/sched.h>
#include "inode.h"
#include "dirent.h"
#include "fs.h"
#include "buf.h"

int inode_t::incnlink()
{
	if (nlink >= LINKMAX)
		return EMLINK;
	nlink++;
	setdirty();
	return 0;
}

void inode_t::decnlink()
{
	if (nlink <= 0) {
		warn("try to unlink inode with nlink=%d\n", nlink);
		return;
	}
	nlink--;
	setdirty();
}

int inode_t::findde(const char * name, buf_t ** b, minixde_t ** de)
{
	scandir_t scan(this);

	for (; scan.more(); scan.next()) {
		*de = scan.curde();
		if (((*de)->ino != MINIXFREEINO) && (*de)->nameequ(name)) {
			(*b = scan.curb())->hold();
			return 0;
		}
	}
	return ENOENT;
}

int inode_t::findde(ino_t ino, char *buf, char **end) 
{
	scandir_t scan(this);
	minixde_t * de;
	
	for (; scan.more(); scan.next()) {
		de = scan.curde();
		if (de->ino == ino) {
			int len = strlen(de->name.get());
			if (buf + len  + 1 >= *end)
				return ERANGE;
			*end -= len;
			memcpy(*end, de->name.get(), len);
			*end -= 1;
			**end = '/';
			return 0;
		}
	}
	return ENOENT;
}

int inode_t::findde(const char * name)
{
	scandir_t scan(this);

	for (; scan.more(); scan.next()) {
		minixde_t * de = scan.curde();
		if ((de->ino != MINIXFREEINO) && de->nameequ(name))
			return 0;
	}
	return ENOENT;
}

/* find and add must be atomic */
int inode_t::addde(const char * name, buf_t ** b, minixde_t ** pp)
{
	int grow = 0;
	minixde_t * de;

	assert(isdir());
	if (strlen(name) > MINIXNAMELEN)
		return ENAMETOOLONG;
	if (!*name)
		return EPERM;
	if (size % sizeof(minixde_t)) {
		warn("invalid dir(dev:%x,ino:%lx)size(%ld)\n", dev, ino, size);
		return EINVAL;
	}
	if (findde(name) == 0)
		return EEXIST;
	scandir_t scan(this, 1, 0);
	for (; scan.more(); scan.next()) {
		de = scan.curde();
		if (grow) de->ino = MINIXFREEINO;
		if (de->ino == MINIXFREEINO) {
			mtime = now();
			scan.curb()->state = BDIRTY; /* just in case */ 
			(*b = scan.curb())->hold();
			*pp = de;
			return 0;
		}
		if (scan.curoff() + sizeof(minixde_t) >= size) {
			grow = 1;
			size += sizeof(minixde_t);
			setdirty();
		}
	}
	panic("should not reach here\n");
}

int inode_t::addde(const char * name, ino_t newino)
{
	int e;
	buf_t * b;
	minixde_t * de;

	if (e = addde(name, &b, &de))
		return e;
	de->name.set(name);
	de->ino = newino;
	b->lose2();
	return 0;
}

int inode_t::lookup2(const char * name, inode_t ** inode)
{
	int e;
	buf_t * b;
	minixde_t * de;

	if (e = findde(name, &b, &de))
		return e;
	e = readi(fs, de->ino, inode);
	b->lose();
	return e;
}

int inode_t::lookup(const char * name, inode_t ** result)
{
	int e;

	if (!isdir())
		return ENOTDIR;
	if (deny(IX))
		return EACCES;
	allege(!ismount()); /* HAHA */
	if (!strcmp(name, "..")) {
		if (this == curr->fs->root) {
			(*result = this)->hold();
			return 0;
		}
		/* cross from root dir to mount point */
		if (isroot()) {
			rwlock.rlock(); /* ? */
			peer->rwlock.rlock();
			e = peer->lookup2(name, result);
			peer->rwlock.unlock();
			rwlock.unlock();
			return e;
		}
	}
	rwlock.rlock();
	e = lookup2(name, result);
	rwlock.unlock();
	/* cross from mount point to root dir */
	if (!e && (*result)->ismount()) {
		(*result)->lose();
		(*result = (*result)->peer)->hold();
	}
	return e;
}

int inode_t::hardlink2(const char * basename, inode_t * oldi)
{
	int e;

	if (!isdir())
		return ENOTDIR;
	if (dev != oldi->dev)
		return EXDEV;
	if (oldi->isdir())
		return EPERM; /* even the super user */
	if (oldi->nlink >= LINKMAX)
		return EMLINK;
	if (e = addde(basename, oldi->ino))
		return e;
	oldi->incnlink();
	return 0;
}

int inode_t::hardlink(const char * basename, inode_t * oldi)
{
	rwlock.wlock();
	int e = hardlink2(basename, oldi);
	rwlock.unlock();
	return e;
}

extern void dumpb();
int inode_t::unlink2(const char * name)
{
	int e;
	buf_t * b;
	minixde_t * de;
	inode_t * child;

	if (!isdir())
		return ENOTDIR;
	if (e = findde(name, &b, &de))
		return e;
	if (e = readi(fs, de->ino, &child)) {
		b->lose();
		return e;
	}
	if (denyunlink(child)) {
		b->lose();
		child->lose();
		return EPERM;
	}
	if (child->isdir()) {
		b->lose();
		child->lose();
		return EPERM;
	}
	child->decnlink();
	child->lose();
	de->ino = MINIXFREEINO;
	b->lose2();
	return 0;
}

int inode_t::unlink(const char * name)
{
	rwlock.wlock();
	int e = unlink2(name);
	rwlock.unlock();
	return e;
}

extern void dumpb();

int inode_t::mkcommon(const char * name, mode_t mode_, inode_t ** inode)
{
	int e;

	allege(!(flags & IMOUNT));
	if (!isdir())
		return ENOTDIR;
	if (deny(IW))
		return EACCES;

	ino_t newino;
	if (e = fs->alloci(&newino))
		return e;
	inode_t * incache = findi(fs, newino);
	if (incache && incache->state != IINVAL) {
		warn("new allocated inode found in cache\n");
		fs->freei(newino);
		return EINVAL;
	}
	buf_t * b;
	minixdi_t * i;
	if (e = fs->seeki(newino, &b, &i)) {
		fs->freei(newino);
		return e;
	}
	if (e = addde(name, newino)) { /* do this last */
		fs->freei(newino);
		return e;
	}
	i->mode = mode_ & ~curr->fs->umask;
	i->nlink = SISDIR(mode_) ? 2 : 1;
	i->uid = curr->uid;
	i->gid = curr->gid;
	i->size = 0;
	for (int j = 0; j < MINIXNDADDR; j++)
		i->daddr[j] = 0;
	i->atime = i->mtime = i->ctime = now();
	b->lose2();
	return (inode) ? readi(fs, newino, inode) : 0;
}

static inline mode_t typemode(int t, mode_t m)
{
	return t | (m & ~SIFMT);
}

int inode_t::mkreg(const char * name, mode_t mode_, inode_t ** inode)
{
	int e;

	rwlock.wlock();
	e = mkcommon(name, typemode(SIFREG, mode_), inode);
	rwlock.unlock();
	return e;
}

int inode_t::mkfifo(const char * name, mode_t mode_)
{
	rwlock.wlock();
	int e = mkcommon(name, typemode(SIFIFO, mode_));
	rwlock.unlock();
	return e;
}

int inode_t::mknod(const char * name, mode_t mode_, dev_t dev_)
{
	int e;
	inode_t * i;

	if (!suser())
		return EPERM;
	if (!SISCHR(mode_) && SISBLK(mode_))
		return EINVAL;
	rwlock.wlock();
	mode_ = SISCHR(mode_) ? typemode(mode_,SIFCHR) : typemode(mode_,SIFBLK);
	e = mkcommon(name, mode_, &i);
	if (!e) {
		i->setrdev(dev_);
		i->lose2();
	}
	rwlock.unlock();
	return e;
}

int inode_t::mkdir(const char * name, mode_t mode_)
{
	int e = 0;
	bno_t bno;
	buf_t * b;
	minixde_t * de;
	inode_t * i;

	if (!isdir())
		return ENOTDIR;
	if (nlink >= LINKMAX) /* also mark the inode dirty */
		return EMLINK;
	if (e = fs->allocb(&bno))
		return e;
	if (e = readb(dev, bno, &b)) {
		fs->freeb(bno);
		return e;
	}
	rwlock.wlock();
	/* do this last, there has no way to "undo" mkcommon */
	if (e = mkcommon(name, typemode(SIFDIR, mode_), &i)) {
		fs->freeb(bno);
		b->lose();
		rwlock.unlock();
		return e; 
	}
	rwlock.unlock();

	de = (minixde_t *) (b->data);
	de->name.set(".");
	de->ino = i->ino;
	de++;
	de->name.set("..");
	de->ino = ino;
	b->lose2();

	i->nlink = 2;
	i->size = sizeof(minixde_t) * 2;
	i->daddr[0] = bno; /* not free */
	i->lose2();

	nlink++;
	setdirty();
	return e;
}

int inode_t::emptydir()
{
	assert(isdir());
	scandir_t scan(this, 0, 2*sizeof(minixde_t));
	for (; scan.more(); scan.next())
		if (scan.curde()->ino != MINIXFREEINO)
			return 0;
	if (nlink != 2)
		warn("empty dir has nlink(%d) !=2 \n", nlink);
	return 1;
}

int inode_t::rmdir2(const char * name)
{
	allege(!(flags & IMOUNT));
	int e = 0;
	buf_t * b = NULL;
	minixde_t * de;
	inode_t * child = NULL;

	if (!isdir())
		return ENOTDIR;
	if (e = findde(name, &b, &de))
		return e;
	if (e = readi(fs, de->ino, &child))
		goto error;
	if (denyunlink(child)) {
		e = EPERM;
		goto error;
	}
	if (!child->isdir()) {
		e = ENOTDIR;
		goto error;
	}
	if (child->refcnt > 1) {
		e = EBUSY;
		goto error;
	}
	if (!child->emptydir()) {
		e = ENOTEMPTY; 
		goto error;
	}
	decnlink();
	child->nlink = 0;
	child->setdirty();
	de->ino = MINIXFREEINO;
	b->state = BDIRTY;

error:	if (b) b->lose();
	if (child) child->lose();
	return e;
}

int inode_t::rmdir(const char * name)
{
	rwlock.wlock();
	int e = rmdir2(name); 
	rwlock.unlock();
	return e;
}

static int advancei(inode_t ** cwd, char * name)
{
	inode_t * sub;
	int e;

	if (e = (*cwd)->lookup2(name, &sub))
		return e;
	(*cwd)->lose();
	(*cwd) = sub;
	return 0;
}

static int parentchild(inode_t * p, inode_t * c)
{
	int e;

	assert(p->dev == c->dev);
	c->hold();
	while (!c->isroot()) {
		if (c == p) { 
			c->lose();
			return 1;
		}
		if (e = advancei(&c, "..")) {
			c->lose();
			return 0;  /* return false on io error */
		}
	}
	c->lose();
	return 0;
}

/* 1) oldname and newname must either both be directories, or both
      not be directories.  if newname is a directory, it must be empty.
   2) increase the newdir's nlink only when oldname is directory 
      and newname donesn't exist.
   3) unlink(newname), link(oldname, newname), unlink(oldname) 
      must be ATOMIC. */

int inode_t::rename2(char * oldname, inode_t * newdir, char * newname)
{
	inode_t * olddir = this; /* rmf && wl */
	buf_t * oldb = NULL, * newb = NULL;
	minixde_t * oldde, * newde;
	inode_t * oldi = NULL, * newi = NULL;
	ino_t oldino;
	int e = 0;

	/* sanity check */
	allege(!olddir->ismount() && !newdir->ismount());
	if (!olddir->isdir() || !newdir->isdir())
		return ENOTDIR;
	if (olddir->deny(IR|IW|IX) || newdir->deny(IR|IW|IX))
		return EACCES;
	if (olddir->dev != newdir->dev)
		return EXDEV;
	if ((olddir == newdir) && !strcmp(oldname, newname))
		return EINVAL;
	if (!*oldname || !strcmp(oldname, ".") || !strcmp(oldname, ".."))
		return EPERM;
	if (!*newname || !strcmp(newname, ".") || !strcmp(newname, ".."))
		return EPERM;

	/* lookup oldname and newname */
	if (e = olddir->findde(oldname, &oldb, &oldde))
		goto error;
	if (e = readi(olddir->fs, oldde->ino, &oldi))
		goto error;
	if (oldi->isdir() && parentchild(oldi, newdir)) {
		e = EINVAL;
		goto error;
	}
	e = newdir->findde(newname, &newb, &newde);
	if (e == 0) {
		/* newname exist, we need unlink newname */
		if (e = readi(newdir->fs, newde->ino, &newi))
			goto error;
		if (oldi->isdir()) {
			if (!newi->isdir()) {
				e = ENOTDIR;
				goto error;
			}
		} else {
			if (newi->isdir()) {
				e = ENOTDIR;
				goto error;
			}
		}
		if (newi->isdir()) {
			if (!newi->emptydir()) {
				e = ENOTEMPTY;
				goto error;
			}
			if (newi->refcnt > 1) {
				e = EBUSY;
				goto error;
			}
		}
		newi->nlink = 0;
		newi->setdirty();
		newi->lose();
	} else if (e == ENOENT) {
		if (oldi->isdir() && (e = newdir->incnlink()))
			goto error;
		if (e = newdir->addde(newname, &newb, &newde)) {
			newdir->decnlink();
			goto error;
		}
	} else goto error;/* other error except ENOENT */
	/* now we can do rename atomically */
	if (oldi->isdir())
		olddir->decnlink();
	oldino = oldde->ino;
	oldde->name.set("");
	oldde->ino = MINIXFREEINO;
	oldb->state = BDIRTY;

	newde->name.set(newname);
	newde->ino = oldino;
	newb->state = BDIRTY;

error:	if (oldb) oldb->lose();
	if (newb) newb->lose();
	if (oldi) oldi->lose();
	return e;
}

int inode_t::rename(char * oldname, inode_t * newdir, char * newname)
{
	inode_t * olddir = this; /* rmf && wl */

	olddir->rwlock.wlock();
	if (olddir != newdir)
		newdir->rwlock.wlock();
	int e = olddir->rename2(oldname, newdir, newname);
	if (olddir != newdir)
		newdir->rwlock.unlock();
	olddir->rwlock.unlock();
	return e;
}
