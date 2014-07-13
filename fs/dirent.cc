#include <lib/root.h>
#include "inode.h"
#include "dirent.h"

scandir_t::scandir_t(inode_t * dir_, int create_, off_t off_)
{
	assert(dir_->isdir());
	dir = dir_;
	b = NULL;
	off = off_;
	create = create_;
	advance();
}

scandir_t::~scandir_t()
{
	if (b) b->lose();
}

void scandir_t::advance()
{
	int e;
	bno_t logic, phys;

	allege(!b);
	for (; more(); off = roundup2(off + MINIXBSIZE - 1, MINIXBSIZE)) {
		logic = dir->bnodiv(off);
		if (logic % READA == 0)
			dir->reada2(logic, READA);
		if (create)
			e = dir->writemap(logic, &phys);
		else
			e = dir->readmap(logic, &phys);
		if (e) {
			warn("IO error while lookup dir\n");
			continue;
		}
		if (!create && (phys == BNOTALLOC))
			continue;
		if (e = readb(dir->dev, phys, &b)) {
			warn("IO error while lookup dir\n");
			continue;
		}
		return;
	}
}

void scandir_t::next()
{
	off += sizeof(minixde_t);
	if (dir->bnomod(off))
		return;
	b->lose();
	b = NULL;
	advance();
}

int destream_t::write(ino_t ino_, off_t off_, char * name)
{
	int reclen = offsetof(dirent_t,name) + strlen(name) + 1;
	if ((char*)cursor + reclen > eroom)
		return 0;
	cursor->ino = ino_;
	cursor->off = off_;
	cursor->reclen = reclen;
	strcpy(cursor->name, name);
	cursor = cursor->next();
	return reclen;
}

int inode_t::getdents(off_t * pos, dirent_t * dirent, int nbyte)
{
	if (!isdir())
		return ENOTDIR;
	destream_t stream(dirent, nbyte);

	rwlock.rlock();
	for (scandir_t scan(this, 0, *pos); scan.more(); scan.next()) {
		minixde_t * de = scan.curde();
		if (de->ino == MINIXFREEINO)
			continue;
		if (!stream.write(de->ino, *pos, de->name.get()))
			break;
		*pos = scan.curoff() + sizeof(minixde_t);
	}
	rwlock.unlock();
	return stream.written();
}
