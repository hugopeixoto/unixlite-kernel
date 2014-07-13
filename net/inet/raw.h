#ifndef _RAW_SOCKET_H
#define _RAW_SOCKET_H

#include <lib/queue.h>
#include <kern/sched.h>
#include <net/lib/pkt.h>
#include <net/lib/sock.h>

struct rawsock_t : public sock_t{
	CHAIN(all,rawsock_t);
	int backlog;
	u8_t proto;
	u32_t laddr;
	u32_t faddr;
	pktq_t recvq;
	
	rawsock_t(u8_t proto_);
	virtual ~rawsock_t();

	int belongtome(pkt_t *pkt);
	int bind(sockaddr_t * me, socklen_t addrlen);
	int connect(sockaddr_t * serv, socklen_t addrlen);
	int listen(int backlog);
  	int accept(sockaddr_t * cli, socklen_t * addrlen);
	int getsockname(sockaddr_t * name, socklen_t * namelen);
	int getpeername(sockaddr_t * name, socklen_t * namelen);
	int send(void * buf, size_t len, int flags);
	int recv(void * buf, size_t len, int flags);
	int sendto(void * buf, size_t len, int flags, sockaddr_t * to,
            socklen_t tolen);
	int recvfrom(void * buf, size_t len, int flags, sockaddr_t * from, 
            socklen_t * fromlen);
	int shutdown(int how);
	int setsockopt(int level, int optname, void* optval, socklen_t optlen);
	int getsockopt(int level, int optname, void* optval, socklen_t* optlen);
	int sendmsg(const msghdr_t * mh, int flags);
	int recvmsg(msghdr_t * mh, int flags);
};
QUEUE(all,rawsock_t);
extern int rawinput(pkt_t *pkt);
#endif
