#ifndef _UNIXSTREAMSOCK_H
#define _UNIXSTREAMSOCK_H

#include <lib/queue.h>
#include <kern/sched.h>
#include <kern/pipe.h>
#include <net/lib/sock.h>

struct sockaddrun_t {
	ushort family;
	char path[108];
};

struct unstsock_t;
struct inode_t;
typedef queue_tl<sizeof(sock_t), sizeof(sock_t) + sizeof(void*), unstsock_t>
unstsockq_t; 
struct unstsock_t : public sock_t {
	CHAIN(,unstsock_t); /* must be first */
	sockaddrun_t bindun;
	inode_t * bindi;
	unstsockq_t clientq;
	int retcode;
	int backlog; /* maximum legnth of incoming queue */
	pipe_t * input;
	pipe_t * output;

	void freeclient();
	unstsock_t();
	~unstsock_t();
	int bind(sockaddr_t * addr, socklen_t addrlen);
	int connect(sockaddr_t * serv, socklen_t addrlen);
	int listen(int backlog);
  	int accept(sockaddr_t * client, socklen_t * addrlen);
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

extern int unixsocket(int domain, int type, int protocol);
extern int unixsocketpair(int domain, int type, int protocol, int *vect);

#endif
