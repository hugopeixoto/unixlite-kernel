#include <lib/root.h>
#include <lib/errno.h>
#include <mm/allocpage.h>
#include <fs/inode.h>
#include <kern/sched.h>
#include "stream.h"

static void pair(unstsock_t * s0, unstsock_t * s1)
{
	s0->output = s1->input = new pipe_t();
	s0->input = s1->output = new pipe_t();
}

unstsock_t::unstsock_t()
{
	type = SOCKFD;
	fdflags = 0;
	refcnt = 1;
	bindi = NULL;
	next = prev = NULL;
	input = output = NULL;
}

unstsock_t::~unstsock_t()
{
	freeclient();
	if (bindi) {
		assert(bindi->unstsock == this);
		bindi->unstsock = NULL;
		bindi->lose();
	}
	if (next) unlink();
	shutdown(2);
}

void unstsock_t::freeclient()
{
	unstsock_t * c;
	while (c = clientq.deqhead()) {
		c->waitq.signal();
		c->retcode = ECONNREFUSED;
	}
}

#define UNPATH offsetof(sockaddrun_t, path)
/* verify write */
static int verwsun(sockaddrun_t * sun, socklen_t addrlen)
{
	int e;

	if (sun->family != AFUNIX)
		return EINVAL;
 	if ((addrlen <= UNPATH) || (addrlen > sizeof(sockaddrun_t)))
		return EINVAL;
	if ((e = verw(sun, addrlen)) || (e = verrstr(sun->path)))
		return e;
	return 0;
}

int unstsock_t::bind(sockaddr_t * addr, socklen_t addrlen)
{
	int e;
	inode_t * i;

	if (bindi)
		return EINVAL;
	sockaddrun_t * sun = (sockaddrun_t*) addr;
	if (e = verwsun(sun, addrlen))
		return e;
	domkfifo(sun->path, 0666);
	if (e = namei(ISTAT, sun->path, &i))
		return e;
	if (bindi || i->unstsock) { /* namei may sleep */
		i->lose();
		return EINVAL;
	}
	(bindi = i)->unstsock = this;
	return 0;
}

int unstsock_t::connect(sockaddr_t * addr, socklen_t addrlen)
{
	int e;
	inode_t * servi;
	unstsock_t * serv;
	sockaddrun_t * sun;
	
	if (input || output || next || prev)
		return EINVAL;
	sun = (sockaddrun_t*) addr;
	if (e = verwsun(sun, addrlen))
		return e;
	if (e = namei(ISTAT, sun->path, &servi))
		return e;
	serv = servi->unstsock;
	serv->clientq.enqtail(this);
	serv->waitq.signal();
	retcode = 0;
	waitq.wait();
	if (curr->hassig())
		retcode = EINTR;
	servi->lose();
	return retcode;
}

int unstsock_t::listen(int backlog_)
{
	return 0;
}

int unstsock_t::accept(sockaddr_t * clientaddr, socklen_t * addrlen)
{
	unstsock_t * server, * client;
	int fd;

	while (!(client = clientq.deqhead())) {
		waitq.wait();
		if (curr->hassig()) {
			freeclient();
			return EINTR;
		}
	}
	client->waitq.signal();
	server = new unstsock_t();
	pair(client, server);
	fd = curr->fdvec->put(server);
	if (fd < 0) {
		server->lose();
		client->retcode = ECONNREFUSED;
	}
	return fd;
}

int unstsock_t::getsockname(sockaddr_t * name, socklen_t * namelen)
{
	return EOPNOTSUPP;
}

int unstsock_t::getpeername(sockaddr_t * name, socklen_t * namelen)
{
	return EOPNOTSUPP;
}

int unstsock_t::send(void * buf, size_t len, int flags)
{
	return output ? output->write(buf, len) : EIO;
}

int unstsock_t::recv(void * buf, size_t len, int flags)
{
	return input ? input->read(buf, len) : EIO;
}

int unstsock_t::sendto(void * buf, size_t len, int flags, sockaddr_t * to,
    socklen_t tolen)
{
	return EOPNOTSUPP;
}

int unstsock_t::recvfrom(void * buf, size_t len, int flags, sockaddr_t * 
    from, socklen_t* fromlen)
{
	return EOPNOTSUPP;
}

int unstsock_t::shutdown(int how)
{
	switch (how) {
		case 0:	if (input) input->rlose(), input = NULL;
			break;
		case 1: if (output) output->wlose(), output = NULL;
			break;
		case 2:	if (input) input->rlose(), input = NULL;
			if (output) output->wlose(), output = NULL;
			break;
	}
	return 0;
}

int unstsock_t::setsockopt(int level, int optname, void* optval, 
    socklen_t optlen)
{
	return EOPNOTSUPP;
}

int unstsock_t::getsockopt(int level, int optname, void* optval, 
    socklen_t* optlen)
{
	return EOPNOTSUPP;
}

int unstsock_t::sendmsg(const msghdr_t * mh, int flags)
{
	return EOPNOTSUPP;
}

int unstsock_t::recvmsg(msghdr_t * mh, int flags)
{
	return EOPNOTSUPP;
}

int unixsocket(int domain, int type, int proto)
{
	sock_t * s = NULL;
	int e;

	switch (type) {
		case SOCKSTREAM:
			s = new unstsock_t();
			break;
		case SOCKDGRAM:
		case SOCKRAW:
			return EPROTONOSUPPORT;
	}
	e = curr->fdvec->put(s);
	return (e < 0) ? s->lose(), e : e; 
}

int unixsocketpair(int domain, int type, int proto, int fd[2])
{
	int e;
	unstsock_t * s[2];

	if (e = verw(fd, 2*sizeof(int)))
		return e;
	if (type != SOCKSTREAM)
		return EOPNOTSUPP;
	s[0] = new unstsock_t();
	s[1] = new unstsock_t();
	if (e = curr->fdvec->putpair((fdes_t**)s, fd)) {
		s[0]->lose();
		s[1]->lose();
		return e;
	}
	pair(s[0], s[1]);
	return 0;
}
