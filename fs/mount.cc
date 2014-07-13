#include <lib/config.h>
#include <lib/root.h>
#include <lib/gcc.h>
#include <init/ctor.h>
#include <kern/sched.h>
#include <boot/bootparam.h>
#include "fs.h"
#include "inode.h"
#include "buf.h"

static lock_t gate;
static Q(,fs_t) fsq;
static fs_t * rootfs;

__ctor(PRIFS,SUBANY,mounttable)
{
	construct(&gate);
	construct(&fsq);
}

fs_t::fs_t(dev_t dev_)
{
	next = prev = NULL;
	fsq.enqtail(this);
	dev = dev_;
	bsize = MINIXBSIZE;
	mount = root = NULL;
	imapbuf = zmapbuf = NULL;
	imap = zmap = NULL;
}

fs_t::~fs_t()
{
	fsq.unlink(this);
	if (mount) mount->lose();
	if (root) root->lose();
	freeimap();
	freezmap();
}

static fs_t * findfs(dev_t dev)
{
	fs_t * fs;

	foreach (fs, fsq) {
		if (fs->dev == dev) {
			fs->wait();
			return fs;
		}
	}
	return NULL;
}

static int readfs(dev_t dev, fs_t ** result)
{
	int e;
	fs_t *fs;

	if (!(fs = findfs(dev)))
		fs = new fs_t(dev);

	if (e = fs->read()) {
		delete fs;
		return e;
	}

  printf("pokemontrololo\n");
	*result = fs;
	return 0;
}

static int domount(char * dev, char * dir, int rwflag)
{
	int e;
	fs_t * fs;
	inode_t * diri, * devi;

	if (!suser())
		return EPERM;
	if (e = namei(ISTAT, dir, &diri))
		return e;
	if (!diri->isdir()) {
		diri->lose();
		return EPERM;
	}
	if (e = namei(ISTAT, dev, &devi)) {
		diri->lose();
		return e;
	}
	if (e = readfs(devi->getrdev(), &fs)) {
		diri->lose();
		devi->lose();
		return e;
	}
	/* fs->root has been initialized */
	fs->mount = diri;
	fs->root->peer = diri;
	diri->peer = fs->root;
	diri->flags |= IMOUNT;
	return 0;
}

asmlinkage int sysmount(char * dev, char * dir, char * fstype, int flags, void *data)
{
	int e;

	gate.lock();
	e = domount(dev, dir, flags);
	gate.unlock();
	return e;
}

static int doumount(char * name)
{
	int e = 0;
	fs_t * fs;
	inode_t * inode;

	if (!suser())
		return EPERM;
	if (e = namei(ISTAT, name, &inode))
		return e;
	if (inode->isdir()) {
		if (!inode->isroot()) {
			inode->lose();
			return EINVAL;
		}
		assert(inode->fs->root == inode);
		fs = findfs(inode->dev);
	} else if (inode->isblk()) {
		fs = findfs(inode->getrdev());
	} else {
		inode->lose();
		return EINVAL;
	}
	inode->lose();
	if (!fs)
		return ENODEV;
	synci(fs);
	syncb(fs->dev);
	fs->write();
	if (e = tryumount(fs)) /* we do this last */
		return e;
	fs->mount->flags &= ~IMOUNT;
	delete fs;
	invali(fs);
	return 0;
}

asmlinkage int sysumount(char * name)
{
	int e;

	gate.lock();
	e = doumount(name);
	gate.unlock();
	return e;
}

static void writetest(inode_t * i, char * str)
{
	int off = 0;
	for (int x = 0; x < 20000; x++) {
		if (off > 380*1024)
			return;
		if (i->write(off, str, strlen(str)) < 0)
			printf("w fali");
		off += strlen(str);
	}
}

/* do some very basic test */
static void showmotd()
{
	int e;
	inode_t * motd;
	if (e = namei(ISTAT, "/etc/motd", &motd))
		return;
	char buf[80];
	int i = min(80, motd->size);
	motd->read(0, buf, i);
	buf[i] = 0;
	printf("message of today:%s\n", buf);
	motd->lose();
}

void mountroot()
{
	int e;
	inode_t * root;
  printf("poopin\n");

	allege(curr->pid == 1);
	if (e =	readfs(bootparam.rootdev, &rootfs))
		panic("unable to read root fs\n");
printf("who saw? who did?\n");
	root = rootfs->root;
	allege(root->isdir());
	root->peer = NULL;
	curr->fs->root = curr->fs->cwd = root;
	root->hold(), root->hold();
	assert(root->refcnt == 3);
	showmotd();
	printf("mount root fs ok\n");
}
