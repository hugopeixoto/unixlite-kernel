#ifndef	_LINUX_SOCKET_H
#define _LINUX_SOCKET_H


/* Socket types. */
#define SOCKSTREAM	1	/* stream (connection) sock	*/
#define SOCKDGRAM	2	/* datagram (conn.less) sock	*/
#define SOCKRAW		3	/* raw sock			*/
#define SOCKRDM		4	/* reliably-delivered message	*/
#define SOCKSEQPACKET	5	/* sequential packet sock	*/
#define SOCKPACKET	10	/* linux specific way of	*/
				/* getting packets at the dev	*/
				/* level.  For writing rarp and	*/
				/* other similiar things on the	*/
				/* user level.			*/

/* Supported address families. */
#define AFUNSPEC	0
#define AFUNIX		1
#define AFINET		2
#define AFEND		3

/* Protocol families, same as address families. */
#define PFUNSPEC	AFUNSPEC
#define PFUNIX		AFUNIX
#define PFINET		AFINET
#define PFEND		AFEND

/* Flags we can use with send/ and recv. */
#define MSGOOB		1
#define MSGPEEK		2

/* Setsockoptions(2) level. Thanks to BSD these must match IPPROTO_xxx */
#define SOLSOCKET	1
#define SOLIP		0
#define SOLIPX		256
#define SOLAX25		257
#define SOLTCP		6
#define SOLUDP		17

/* For setsockoptions(2) */
#define SODEBUG		1
#define SOREUSEADDR	2
#define SOTYPE		3
#define SOERROR		4
#define SODONTROUTE	5
#define SOBROADCAST	6
#define SOSNDBUF	7
#define SORCVBUF	8
#define SOKEEPALIVE	9
#define SOOOBINLINE	10
#define SONO_CHECK	11
#define SOPRIORITY	12
#define SOLINGER	13

struct linger_t {
	int on;
	int seconds;
};

/* IP options */
#define IPTOS			1
#define	IPTOSLOWDELAY		0x10
#define	IPTOSTHROUGHPUT		0x08
#define	IPTOSRELIABILITY	0x04
#define IPTTL			2

/* IPX options */
#define IPXTYPE		1

/* AX.25 options */
#define AX25WINDOW	1

/* TCP options - this way around because someone left a set in the c library includes */
#define TCP_NODELAY	1
#define TCP_MAXSEG	2

/* The various priorities. */
#define SOPRIINTERACTIVE	0
#define SOPRINORMAL		1
#define SOPRIBACKGROUND	2


#define SYSSOCKET	1		/* syssock(2)		*/
#define SYSBIND		2		/* sysbind(2)			*/
#define SYSCONNECT	3		/* sysconnect(2)		*/
#define SYSLISTEN	4		/* syslisten(2)		*/
#define SYSACCEPT	5		/* sysaccept(2)		*/
#define SYSGETSOCKNAME	6		/* sysgetsockname(2)		*/
#define SYSGETPEERNAME	7		/* sysgetpeername(2)		*/
#define SYSSOCKETPAIR	8		/* syssockpair(2)		*/
#define SYSSEND		9		/* syssend(2)			*/
#define SYSRECV		10		/* sysrecv(2)			*/
#define SYSSENDTO	11		/* syssendto(2)		*/
#define SYSRECVFROM	12		/* sysrecvfrom(2)		*/
#define SYSSHUTDOWN	13		/* sysshutdown(2)		*/
#define SYSSETSOCKOPT	14		/* syssetsockopt(2)		*/
#define SYSGETSOCKOPT	15		/* sysgetsockopt(2)		*/
#define SYSSENDMSG	16		/* syssendmsg(2)		*/
#define SYSRECVMSG	17		/* sysrecvmsg(2)		*/

typedef unsigned int socklen_t;
struct sockaddr_t {
	u16_t	family;		/* address family, AFxxx	*/
	u8_t	data[14];	/* 14 bytes of protocol address	*/
};
struct msghdr_t {
	int shit;
};

#include <kern/fdes.h>
#include <kern/sched.h>


struct pkt_t;
struct sock_t : public fdes_t {
	/* socket option */
	linger_t solinger;
	bool soreuseaddr;
	bool sodebug;
	bool sokeepalive;
	bool sooobinline;
	bool sopriority;
	bool sobroadcast;
	int setsocketopt(int optname, void * optval, socklen_t optlen);
	int getsocketopt(int optname, void * optval, socklen_t * optlen);

	int backlog;
	waitq_t waitq;

	enum { 	MINSNDBUF = 8 * 1024, MAXSNDBUF = 32 * 1024,
		MINRCVBUF = 8 * 1024, MAXRCVBUF = 32 * 1024 };
	int maxsndbuf;
	int maxrcvbuf;
	int cursndbuf;
	int currcvbuf;
	int allocsndbuf(pkt_t **pp, int flags, int headlen, int datalen);
	void freesndbuf(pkt_t *p);
	int allocrcvbuf(pkt_t *p);
	void freercvbuf(pkt_t *p);
	void freercvbuf(int delta) { currcvbuf += delta; }

	sock_t();
	virtual ~sock_t();
	/* interface derived from fdesc */
	int ioctl(int cmd, ulong arg);
	int read(void * buf, int count);
	int write(void * buf, int count);
	
	/* bsd socket interface */
	virtual int bind(sockaddr_t * addr, socklen_t addrlen) = 0;
	virtual int connect(sockaddr_t * serv, socklen_t addrlen) = 0;
	virtual int listen(int backlog) = 0;
  	virtual int accept(sockaddr_t * client, socklen_t * addrlen) = 0;
	virtual int getsockname(sockaddr_t * name, socklen_t * namelen) = 0;
	virtual int getpeername(sockaddr_t * name, socklen_t * namelen) = 0;
	virtual int send(void * buf, size_t len, int flags) = 0;
	virtual int recv(void * buf, size_t len, int flags) = 0;
	virtual int sendto(void * buf, size_t len, int flags, sockaddr_t * to,
                    socklen_t tolen) = 0;
	virtual int recvfrom(void * buf, size_t len, int flags, sockaddr_t * 
                    from, socklen_t * fromlen) = 0;
	virtual int shutdown(int how) = 0;
	virtual int setsockopt(int level, int optname, void * optval, 
                    socklen_t optlen) = 0;
	virtual int getsockopt(int level, int optname, void * optval,
                    socklen_t * optlen) = 0;
	virtual int sendmsg(const msghdr_t * mh, int flags) = 0;
	virtual int recvmsg(msghdr_t * mh, int flags) = 0;
};

#endif
