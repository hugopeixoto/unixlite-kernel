#include <lib/root.h>
#include <lib/string.h>
#include <lib/errno.h>
#include <lib/gcc.h>
#include "fdes.h"
#include "sched.h"

int fdes_t::lseek(off_t off, int whence)
{
	return ESPIPE;
}

int fdes_t::ioctl(int cmd, ulong arg)
{
	return ENOTTY;
}

int fdes_t::getdents(dirent_t * de, int nbyte)
{
	return ENOTDIR;
}

int fdvec_t::getslot()
{
	int i;

	for (i = 0; i <NOPEN ; i++) {
		if (!vec[i])
			return i;
	}
	return EMFILE;
}

int fdvec_t::getpair(int fd2[2])
{
	int i, n = 0;

	for (i = 0; i < NOPEN; i++)
		if (!vec[i]) {
			fd2[n++] = i;
			if (n == 2)
				return 0;
		}
	return EMFILE;
}

int fdvec_t::put(fdes_t * fdes)
{
	int e;

	if ((e = getslot()) < 0)
		return e;
	vec[e] = fdes;
	return e;
}

int fdvec_t::putpair(fdes_t ** fdes, int * fd2)
{
	int e;

	if (e = getpair(fd2))
		return e;
	vec[fd2[0]] = fdes[0];
	vec[fd2[1]] = fdes[1];
	return 0;
}

int fdvec_t::dup(int oldfd)
{
	int e;

	if (!valid(oldfd))
		return EBADF;
	if ((e = getslot()) < 0)
		return e; 
	(vec[e] = vec[oldfd])->refcnt++;
	return e;
}

int fdvec_t::dup2(int oldfd, int newfd)
{
	if (!valid(oldfd) || (newfd < 0) || (newfd >= NOPEN))
		return EBADF;
#warning "???"
	if (oldfd == newfd)
		return newfd;
	close(newfd);
	(vec[newfd] = vec[oldfd])->refcnt++;
	return newfd;
}

int fdvec_t::fdupfd(int oldfd, int lowest)
{
	for (; lowest < NOPEN; lowest++) {
		if (vec[lowest])
			continue;
		(vec[lowest] = vec[oldfd])->refcnt++;
		return lowest;
	}
	return EMFILE;
}

int fdvec_t::close(int fd)
{
	int e;
	fdes_t * fdes;

	if (e = get(fd, &fdes))
		return e;
	fdes->lose();
	vec[fd] = NULL;
	closemap.clr(fd);
	return 0;
}

void fdvec_t::closeonexec()
{
	for (int i = 0; i < NOPEN; i++) {
		if (closemap.tst(i)) {
			assert(vec[i]);
			closemap.clr(i);
			vec[i]->lose();
			vec[i] = NULL;
		}
	}
}

void fdvec_t::closeall()
{
	for (int i = 0; i < NOPEN; i++) {
		if (vec[i]) {
			vec[i]->lose();
			vec[i] = NULL;
		}
	}
}

fdvec_t * fdvec_t::clone()
{
	fdvec_t * x = new fdvec_t();

	for (int i = 0; i < NOPEN; i++)
		if (x->vec[i] = vec[i])
			vec[i]->hold();
	return x;
}

fdvec_t::fdvec_t()
{
	memset(this, 0, sizeof(*this));
}

fdvec_t::~fdvec_t()
{
	closeall();
}

asmlinkage int sysdup2(int oldfd, int newfd)
{
	if (lowfreepage())
		return ENOMEM;
	return curr->fdvec->dup2(oldfd, newfd);
}

asmlinkage int sysdup(int fd)
{
	if (lowfreepage())
		return ENOMEM;
	return curr->fdvec->dup(fd);
}

static inline int utok(int u)
{
	return u+1;
}

static inline int ktou(int k)
{
	return k-1;
}

asmlinkage int sysfcntl(int fd, int cmd, ulong arg)
{	
	int e;
	fdes_t * fdes;

	if (lowfreepage())
		return ENOMEM;
	if (e = fdtofdes(fd, &fdes))
		return e;
	switch (cmd) {
		case FDUPFD:
			return curr->fdvec->fdupfd(fd, arg);
		case FGETFD:
			return curr->fdvec->closemap.tst(fd);
		case FSETFD:
			if (arg&1)
				curr->fdvec->closemap.set(fd);
			else
				curr->fdvec->closemap.clr(fd);
			return 0;
		case FGETFL:
			return ktou(fdes->fdflags);
		case FSETFL:
			fdes->fdflags = utok(arg); 
			return 0;
		case FGETLK:
		case FSETLK:
		case FSETLKW:
			return ENOSYS;
	}
	return ENOSYS;
}

asmlinkage int sysclose(int fd)
{
	return curr->fdvec->close(fd);
}

asmlinkage int sysread(int fd, void * buf, int count)
{
	int e;
	fdes_t * fdes;

	if (lowfreepage())
		return ENOMEM;
	if (e = fdtofdes(fd, &fdes))
		return e; 
	if (!(fdes->fdflags & FDREAD))
		return EBADF;
	if (e = verw(buf, count))
		return e;
	return fdes->read(buf, count);
}

asmlinkage int syswrite(int fd, void * buf, int count)
{
	int e;
	fdes_t * fdes;

	if (lowfreepage())
		return ENOMEM;
	if (e = fdtofdes(fd, &fdes))
		return e; 
	if (!(fdes->fdflags & FDWRITE))
		return EBADF;
	if (e = verr(buf, count))
		return e;
	return fdes->write(buf, count);
}

asmlinkage int syslseek(int fd, off_t off, int whence)
{
	int e;
	fdes_t * fdes;

	if (e = fdtofdes(fd, &fdes))
		return e; 
	return fdes->lseek(off, whence);
}

asmlinkage int sysioctl(int fd, int cmd, ulong arg) 
{
	int e;
	fdes_t * fdes;

	if (lowfreepage())
		return ENOMEM;
	if (e = fdtofdes(fd, &fdes))
		return e;
	return fdes->ioctl(cmd, arg); 
}

asmlinkage int sysgetdents(int fd, dirent_t * de, int nbyte)
{
	int e;
	fdes_t * fdes;

	if (lowfreepage())
		return ENOMEM;
	if (e = fdtofdes(fd, &fdes))
		return e; 
	if (!(fdes->fdflags & FDREAD))
		return EBADF;
	if (e = verw(de, nbyte))
		return e;
	return fdes->getdents(de, nbyte);
}
