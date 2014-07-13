#ifndef	_KERNFDES_H
#define	_KERNFDES_H

#include <lib/bitmap.h>
#include <lib/errno.h>

/* file descriptor type */
#define PIPEFD 1
#define REGFD 2
#define BLKFD 3
#define CHRFD 4
#define SOCKFD 5

/* file descriptor flags */
#define FDREAD	0x0001
#define FDWRITE 0x0002
#define FDAPPEND 0x0008
#define FDNONBLOCK 0x0010

struct dirent_t;
/* file descriptor */
struct fdes_t {
 	int type;
	int fdflags;
	int refcnt;

	fdes_t() { refcnt = 1; }
	void hold() { assert(refcnt >= 0); refcnt++; };
	virtual void lose() { assert(refcnt > 0); if (--refcnt) return; delete this; }
	virtual ~fdes_t() {};
	virtual int read(void * buf, int count) = 0;
	virtual int write(void * buf, int count) = 0;
	virtual int lseek(off_t off, int whence);
	virtual int ioctl(int cmd, ulong arg);
	virtual int getdents(dirent_t * de, int nbyte);
};

#define NOPEN 64
class fdvec_t {
	fdes_t * vec[NOPEN];
	int getslot();
	int getpair(int * fd2);
public: bitmap_tl <NOPEN> closemap;
	int valid(int fd) { return (fd >= 0 && fd < NOPEN) && vec[fd]; }
	int get(int fd, fdes_t ** result)
	{
		*result = NULL;
		if (!valid(fd))
			return EBADF;
		*result = vec[fd];
		return 0;
	}
	int put(fdes_t * fdes);
	int putpair(fdes_t ** fdes, int * fd2);
	int dup(int oldfd);
	int dup2(int oldfd, int newfd);
	int fdupfd(int oldfd, int lowest); 
	int close(int fd);
	void closeonexec();
	void closeall();
	fdvec_t * clone();
	fdvec_t();
	~fdvec_t();
};

/* open/fcntl - OSYNC isn't implemented yet */
#define OACCMODE	0003
#define ORDONLY	    	00
#define OWRONLY	    	01
#define ORDWR		02
#define OCREAT		0100	/* not fcntl */
#define OEXCL		0200	/* not fcntl */
#define ONOCTTY	  	0400	/* not fcntl */
#define OTRUNC		01000	/* not fcntl */
#define OAPPEND	 	02000
#define ONONBLOCK	04000
#define ONDELAY		ONONBLOCK
#define OSYNC		010000

#define FDUPFD		0	/* dup */
#define FGETFD		1	/* get f_flags */
#define FSETFD		2	/* set f_flags */
#define FGETFL		3	/* more flags (cloexec) */
#define FSETFL		4
#define FGETLK		5
#define FSETLK		6
#define FSETLKW		7

#define FSETOWN		8	/*  for sockets. */
#define FGETOWN		9	/*  for sockets. */

/* for F[GET|SET]FL */
#define FD_CLOEXEC	1	/* actually anything with low bit set goes */

#define FRDLCK		0
#define FWRLCK		1
#define FUNLCK		2

/* For bsd flock () */
#define FEXLCK		4	/* or 3 */
#define FSHLCK		8	/* or 4 */

struct flock_t {
	short type;
	short whence;
	off_t start;
	off_t len;
	pid_t pid;
};

#endif
