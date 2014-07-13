#ifndef _KERNPIPE_H
#define _KERNPIEP_H
#include <kern/fdes.h>
#include <lib/fifoq.h>
#include <kern/sched.h>

struct pipe_t : protected fifoq_t {
	waitq_t waitq;
	char * page;
	int nreader;
	int nwriter;

	pipe_t();
	~pipe_t();
	void rhold() { nreader++; }
	void whold() { nwriter++; }
	void rlose();
	void wlose();
	int read(void * buf, int count);
	int write(void * buf, int count);
};

struct pipefd_t : public fdes_t {
	pipe_t * pipe;

	pipefd_t(int flags, pipe_t * p)
	{
		type = PIPEFD;
		fdflags = flags;
		refcnt = 1;
		pipe = p;
	}
	~pipefd_t();
	int read(void * buf, int count);
	int write(void * buf, int count);
	int lseek(off_t off, int whence);
	int ioctl(int cmd, ulong arg); 
	int readdir(dirent_t * de, int count);
};

#endif
