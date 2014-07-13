#ifndef _UDP_SOCKET_H
#define _UDP_SOCKET_H

#include <lib/queue.h>
#include <kern/sched.h> 
#include <net/lib/pkt.h>
#include <net/lib/sock.h>

struct udphdr_t {
        u16_t sport; /* source port */
        u16_t dport; /* destination port */
        u16_t totlen;
        u16_t chksum;
        char data[0];

	int datalen() { return ntohs(totlen) - sizeof(udphdr_t); } /* host byte order */
};

struct udpsock_t : public sock_t {
	CHAIN(hash, udpsock_t);
	CHAIN(all, udpsock_t);
	int backlog;
	pktq_t recvq; /* receive queue */
	/* the following fields is stored in network byte order */
	u32_t laddr; /* local address */
	u16_t lport; /* local port */
	u32_t faddr; /* foreign address */
	u16_t fport; /* foreign port */
        int mss;

	void dump();
	int getname(sockaddr_t * name, socklen_t * namelen, int peer);
	void input(pkt_t * frags);
        int output(pkt_t * pkt, u32_t daddr, u16_t dport);

	udpsock_t();
	virtual ~udpsock_t();
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
QUEUE(hash, udpsock_t);
QUEUE(all, udpsock_t);

extern void udpinput(pkt_t *pkt);

#endif
