#include <lib/root.h>
#include <lib/gcc.h>
#include <lib/errno.h>
#include <kern/sched.h>
#include <mm/allocpage.h>
#include "pipe.h"

pipe_t::pipe_t()
{
	nreader = nwriter = 1;
	page = (char*) allocpage(AKERN);
	init(page, PAGESIZE);
} 

pipe_t::~pipe_t()
{
	freepage(page);
	waitq.broadcast();
}

void pipe_t::rlose()
{
	assert(nreader > 0);
	if (--nreader)
		return;
	waitq.broadcast();
	if (!nwriter)
		delete this;
}

void pipe_t::wlose()
{
	assert(nwriter > 0);
	if (--nwriter)
		return;
	waitq.broadcast();
	if (!nreader)
		delete this;
}

int pipe_t::read(void * buf, int count)
{
	while (empty()) {
		if (!nwriter)
			return 0;
		waitq.wait();
		if (curr->hassig())
			return EINTR;
	}
	int n = getc((char*)buf, count);
	waitq.broadcast();
	return n;
}

int pipe_t::write(void * bufvoid, int count)
{
	char * buf = (char*) bufvoid;
	char * obuf = buf, * ebuf = buf + count;
	while (buf < ebuf) {
		if (!nreader) {
			curr->post(SIGPIPE);
			return EPIPE;
		}
		if (full()) {
			waitq.wait();
			if (curr->hassig())
				return EINTR;
			continue;
		}
		buf += putc(buf, ebuf - buf);
		waitq.broadcast();
	}
	return buf - obuf;
}

pipefd_t::~pipefd_t()
{
	(fdflags & FDREAD) ? pipe->rlose() : pipe->wlose();
}

int pipefd_t::read(void * buf, int count)
{
	if (!(fdflags & FDREAD))
		return EINVAL;
	return pipe->read(buf, count);
}

int pipefd_t::write(void * buf, int count)
{
	if (!(fdflags & FDWRITE))
		return EINVAL;
	return pipe->write(buf, count);
}

int pipefd_t::lseek(off_t off, int whence)
{
	return ESPIPE;
}

int pipefd_t::ioctl(int cmd, ulong arg) 
{
	return ENOTTY;
}

int pipefd_t::readdir(dirent_t * de, int count)
{
	return ENOTDIR;
}

asmlinkage int syspipe(int fd2[2])
{
	pipe_t * pipe;
	fdes_t * fdes[2];
	int e;

	if (e = verw(fd2, sizeof(int)*2))
		return e;
	pipe = new pipe_t();
	fdes[0] = new pipefd_t(FDREAD, pipe);
	fdes[1] = new pipefd_t(FDWRITE, pipe);
	if (e = curr->fdvec->putpair(fdes, fd2)) {
		fdes[0]->lose();
		fdes[1]->lose();
		return e;
	}
	return 0;
}
