#include <lib/root.h>
#include <lib/gcc.h>
#include <kern/sched.h>
#include <mm/allocpage.h>
#include <net/inet/sock.h>
#include <net/unix/stream.h>
#include <net/inet/debug.h>

struct {
	int (*socket)(int domain, int type, int proto);
	int (*socketpair)(int domain, int type, int proto, int fd[2]);
} createsocket[] = {
	{NULL, NULL}, /* AFUNSPEC */
	{unixsocket, unixsocketpair}, /* AFUNIX */
	{inetsocket, inetsocketpair}, /* AFINET */
};

int fdtosock(int fd, sock_t ** s)
{
	fdes_t * fdes;
	int e = fdtofdes(fd, &fdes);
	if (e)
		return e;
	if (fdes->type != SOCKFD)	
		return ENOTSOCK;
	*s = (sock_t*) fdes;
	return 0;
}

static int verwptrlen(void * ptr, socklen_t * len)
{
	int e;
	if (e = verw(len, sizeof(*len)))
		return e;
	if (e = verw(ptr, *len))
		return e;
	return e;
}

asmlinkage int syssocket(int domain, int type, int protocol)
{
	debug(SOCKDBG, "socket\n");
	if ((domain < 1) || (domain > PFEND))
		return EINVAL; 
	if (!(createsocket[domain].socket))
		return EPROTONOSUPPORT;
	return createsocket[domain].socket(domain, type, protocol);
}

asmlinkage int sysbind(int fd, sockaddr_t * addr, socklen_t addrlen)
{
	int e;
	sock_t * s;

	debug(SOCKDBG, "bind\n");
	if (e = fdtosock(fd, &s))
		return e;
	if (e = verr(addr, addrlen))
		return e;
	return s->bind(addr, addrlen);
}

asmlinkage int sysconnect(int fd, sockaddr_t * serv, socklen_t addrlen)
{
	int e;
	sock_t * s;

	debug(SOCKDBG, "connect\n");
	if (e = fdtosock(fd, &s))
		return e;
	if (e = verr(serv, addrlen))
		return e;
	return s->connect(serv, addrlen);
}

asmlinkage int syslisten(int fd, int backlog)
{
	int e;
	sock_t * s;

	debug(SOCKDBG, "listen\n");
	if (e = fdtosock(fd, &s))
		return e;
	return s->listen(backlog);
}

asmlinkage int sysaccept(int fd, sockaddr_t * client, socklen_t * addrlen)
{
	int e;
	sock_t * s;

	debug(SOCKDBG, "accept\n");
	if (e = fdtosock(fd, &s))
		return e;
	if (client) {
	}
	return s->accept(client, addrlen);
}

asmlinkage int sysgetsockname(int fd, sockaddr_t * name, socklen_t * namelen)
{
	int e;
	sock_t * s;

	debug(SOCKDBG, "getsockname\n");
	if (e = fdtosock(fd, &s))
		return e;
	if (e = verwptrlen(name, namelen))
		return e;
	return s->getsockname(name, namelen);
}

asmlinkage int sysgetpeername(int fd, sockaddr_t * name, socklen_t * namelen)
{
	int e;
	sock_t * s;

	debug(SOCKDBG, "getpeername\n");
	if (e = fdtosock(fd, &s))
		return e;
	if (e = verwptrlen(name, namelen))
		return e;
	return s->getpeername(name, namelen);
}

asmlinkage int syssocketpair(int domain, int type, int protocol, int fd[2])
{	
	debug(SOCKDBG, "socketpair\n");
	if ((domain < 1) || (domain > PFEND))
		return EINVAL; 
	if (!(createsocket[domain].socketpair))
		return EPROTONOSUPPORT;
	return createsocket[domain].socketpair(domain, type, protocol, fd);
}

asmlinkage int syssend(int fd, void * buf, size_t len, int flags)
{
	int e;
	sock_t * s;

	debug(SOCKDBG, "send\n");
	if (e = fdtosock(fd, &s))
		return e;
	if (e = verr(buf, len))
		return e;
	return s->send(buf, len, flags);
}

asmlinkage int sysrecv(int fd, void * buf, size_t len, int flags)
{
	int e;
	sock_t * s;

	debug(SOCKDBG, "recv\n");
	if (e = fdtosock(fd, &s))
		return e;
	if (e = verw(buf, len))
		return e;
	return s->recv(buf, len, flags);
}

asmlinkage int syssendto(int fd, void * buf, size_t len, int flags, 
	sockaddr_t * to, socklen_t tolen)
{
	int e; 
	sock_t * s;

	debug(SOCKDBG, "sendto\n");
	if (e = fdtosock(fd, &s))
		return e;
	if (e = verr(buf, len))
		return e;
	if (e = verr(to, tolen))
		return e;
	return s->sendto(buf, len, flags, to, tolen);
}

asmlinkage int sysrecvfrom(int fd, void * buf, size_t len, int flags, 
	sockaddr_t * from, socklen_t * fromlen)
{
	int e;
	sock_t * s;

	debug(SOCKDBG, "recvfrom\n");
	if (e = fdtosock(fd, &s))
		return e;
	if (e = verw(buf, len))
		return e;
	if (from && (e = verwptrlen(from, fromlen)))
		return e;
	return s->recvfrom(buf, len, flags, from, fromlen);
}

asmlinkage int sysshutdown(int fd, int how)
{
	int e;
	sock_t * s;

	debug(SOCKDBG, "shutdown\n");
	if (e = fdtosock(fd, &s))
		return e;
	return s->shutdown(how);
}

asmlinkage int syssetsockopt(int fd, int level, int optname, void * optval, 
	socklen_t optlen)
{
	int e; 
	sock_t * s;

	debug(SOCKDBG, "setsockopt\n");
	if (e = fdtosock(fd, &s))
		return e;
	if (e = verr(optval, optlen))
		return e;
	if (level == SOLSOCKET)
		return s->setsocketopt(optname, optval, optlen);
	return s->setsockopt(level, optname, optval, optlen);
}

asmlinkage int sysgetsockopt(int fd, int level, int optname, void * optval,
	socklen_t * optlen)
{
	int e;
	sock_t * s;

	debug(SOCKDBG, "getsockopt\n");
	if (e = fdtosock(fd, &s))
		return e;
	if (e = verwptrlen(optval, optlen))
		return e;
	if (level == SOLSOCKET)
		return s->getsocketopt(optname, optval, optlen);
	return s->getsockopt(level, optname, optval, optlen);
}

asmlinkage int syssendmsg(int fd, const msghdr_t * mh, int flags)
{
	int e;
	sock_t * s;

	debug(SOCKDBG, "sendmsg\n");
	if (e = fdtosock(fd, &s))
		return e;
	if (e = verr((void*)mh, sizeof(msghdr_t)))
		return e;
	return s->sendmsg(mh, flags);
}

asmlinkage int sysrecvmsg(int fd, msghdr_t * mh, int flags)
{
	int e;
	sock_t * s;

	debug(SOCKDBG, "recvmsg\n");
	if (e = fdtosock(fd, &s))
		return e;
	if (e = verr(mh, sizeof(msghdr_t)))
		return e;
	return s->recvmsg(mh, flags);
}

#define A2(t0, t1) (t0)argv[0], (t1)argv[1]
#define A3(t0, t1, t2) A2(t0, t1), (t2)argv[2]
#define A4(t0, t1, t2, t3) A3(t0, t1, t2), (t3)argv[3]
#define A5(t0, t1, t2, t3, t4) A4(t0, t1, t2, t3), (t4)argv[4] 
#define A6(t0, t1, t2, t3, t4, t5) A5(t0, t1, t2, t3, t4), (t5)argv[5]
static uchar argc[18]={0,3,3,3,2,3,3,3,4,4,4,6,6,2,5,5,3,3};
asmlinkage int syssocketcall(int call, ulong * argv)
{
	int e;

	if (call < 1 || call > SYSRECVMSG)
		return EINVAL;
	if (e = verr(argv, sizeof(ulong) * argc[call]))
		return e;
	if (lowfreepage())
		return ENOMEM;
	switch (call) {
		case SYSSOCKET:
			return syssocket(A3(int, int, int));
		case SYSBIND:
			return sysbind(A3(int, sockaddr_t*, int));
		case SYSCONNECT:
			return sysconnect(A3(int, sockaddr_t*, int));
		case SYSLISTEN:
			return syslisten(A2(int, int));
		case SYSACCEPT:
			return sysaccept(A3(int, sockaddr_t*, socklen_t*));
		case SYSGETSOCKNAME:
			return sysgetsockname(A3(int, sockaddr_t*, socklen_t*)); 
		case SYSGETPEERNAME:
			return sysgetpeername(A3(int, sockaddr_t*, socklen_t*)); 
		case SYSSOCKETPAIR:
			return syssocketpair(A4(int, int, int, int*));
		case SYSSEND:
			return syssend(A4(int, void*, int, int));
		case SYSSENDTO:
			return syssendto(A6(int, void*, int, int, 
			       sockaddr_t*, int));
		case SYSRECV:
			return sysrecv(A4(int, void*, int, int));
		case SYSRECVFROM:
			return sysrecvfrom(A6(int, void*, int, int, 
			       sockaddr_t*, socklen_t*));
		case SYSSHUTDOWN:
			return sysshutdown(A2(int, int));
		case SYSSETSOCKOPT:
			return syssetsockopt(A5(int, int, int, void*, int));
		case SYSGETSOCKOPT:
			return sysgetsockopt(A5(int, int, int, void*, socklen_t*));
		case SYSSENDMSG:
			return syssendmsg(A3(int, msghdr_t*, int));
		case SYSRECVMSG:
			return sysrecvmsg(A3(int, msghdr_t*, int));
	}
	return EINVAL; 
}
