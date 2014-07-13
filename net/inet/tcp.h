#ifndef _TCPSOCKET_H
#define _TCPSOCKET_H

#include <lib/queue.h>
#include <net/lib/sock.h>
#include <kern/timer.h>
#include <kern/sched.h>

#define TCPFFIN 0x01 
#define TCPFSYN 0x02 
#define TCPFRST 0x04
#define TCPFPSH 0x08 
#define TCPFACK 0x10 
#define TCPFURG 0x20

struct tcpopt_t {
	u8_t kind;
	u8_t len;
	tcpopt_t * next() { return (tcpopt_t*) ((char*)this + len); }
} __attribute__((packed));

struct tcpmssopt_t : public tcpopt_t {
	u16_t mss; /* mss = mtu - sizeof(ip header) - sizeof(tcp header) */
} __attribute__((packed));

struct tcphdr_t {
	u16_t sport;
	u16_t dport;
	u32_t seq;
	u32_t ack;
	u8_t __hlen;
	u8_t flags;
	u16_t wnd;
	u16_t chksum;
	u16_t urgptr;
	tcpopt_t opt[0];

	void dump();
	/* head length field format: 
 	 * unsigned res:4;
 	 * unsigned hlen:4; in unit of four byte, include options  */
	int getheadlen() { return (__hlen >> 2) & ~3; };
	void setheadlen(int hlen) { __hlen = (hlen << 2) & 0xf0; };
	void *data() { return (char*)this + getheadlen(); }
	int synfinbyte()
	{
		int count = 0;
		if (flags & TCPFSYN)
			count++;
		if (flags & TCPFFIN)
			count++;
		return count;
	}
	bool ackeq(u32_t seq) { return (flags & TCPFACK) && (htonl(seq) == ack); }
} __attribute__((packed));


/*RFC793

  Send Sequence Space
                   1         2          3          4      
              ----------|----------|----------|---------- 
                     SND.UNA    SND.NXT    SND.UNA        
                                          +SND.WND        
        1 - old sequence numbers which have been acknowledged  
        2 - sequence numbers of unacknowledged data            
        3 - sequence numbers allowed for new data transmission 
        4 - future sequence numbers which are not yet allowed  
  The send window is the portion of the sequence space labeled 3

  Receive Sequence Space
                       1          2          3      
                   ----------|----------|---------- 
                          RCV.NXT    RCV.NXT        
                                    +RCV.WND        

        1 - old sequence numbers which have been acknowledged  
        2 - sequence numbers allowed for new reception         
        3 - future sequence numbers which are not yet allowed  */

struct tcpsock_t;
typedef queue_tl<sizeof(sock_t),sizeof(sock_t) + sizeof(void*),tcpsock_t> 
Q(incoming,tcpsock_t);
struct tcpsock_t : public sock_t {
	CHAIN(incoming,tcpsock_t); /* must be the first element */
	CHAIN(all,tcpsock_t);
        CHAIN(lhash,tcpsock_t); /* lport as the hash key */
        CHAIN(fhash,tcpsock_t); /* fport as the hash key */
	enum tcpstate_t {	
		SCLOSED,
		SLISTEN,
		SSYNSENT,
		SSYNRECV,
		SESTAB,
		SFINWAIT1,
		SFINWAIT2,
		STIMEWAIT,
		SCLOSEWAIT,
		SLASTACK,
		SCLOSING,
	};
	tcpstate_t state;
	/* the following fields is stored in network byte order */
	u32_t laddr; /* local address */
	u32_t faddr; /* foreign address */
	u16_t lport; /* local port */
	u16_t fport; /* foreign port */
	Q(incoming, tcpsock_t) incomingq;
	tcpsock_t *listening;
	int mss;

	/* snduna = unackedq.head()->seq sndnxt = unacked.tail()->endseq */
        u32_t iss; /* initial send sequence */
	pktq_t unackedq; 
        u32_t sndnxt; 
        int sndwnd; /* recv buffer of peer point */
	int output(int priority, u8_t flags, 
		     void*data = NULL, int datalen = 0,
		     void *opt = NULL, int optlen = 0);
	int sendflags(u8_t flags);
	int sendflagsmss(u8_t flags);
	int resetpeer(u32_t seq, u32_t ack, u32_t laddr_, u16_t lport_, 
		      u32_t faddr_, u16_t fport_);
	int resetpeer(iphdr_t *ih, tcphdr_t *th)
	{
		u32_t seq = ntohl(th->ack);
		u32_t ack = ntohl(th->seq) + th->synfinbyte();
		return resetpeer(seq, ack, ih->daddr, th->dport, ih->saddr, th->sport);
	}
	int resetpeer()
	{
		return resetpeer(sndnxt, rcvnxt, laddr, lport, faddr, fport);
	}
	void abort();
	void parseopt(tcphdr_t *th);

	/* recv variables specified in rfc793 */
        u32_t irs; /* initial recv sequence */
	bool finrecved;
	pktq_t recvq;
	pktq_t oosrecvq; /* out of sequence */
        u32_t rcvnxt; /* receive next */
        int rcvwnd(); /* receive window */

	void initiss();
	void initirs(tcphdr_t *th);
        u32_t lastseq; /* sequence number used for last window update */
        u32_t lastack; /* ack number used for last window update */
	void updatesndwnd(tcphdr_t * th);

        /* congestion avoidence, not implemented yet */
        int congwnd;
        int slowstartthresh;

        /* timer management */
	enum { MAXREXMIT = 8, 
	       MAXDELACK = 2 };
	int nrexmit;
	int ndelack;
        timer_t rexmittm;
        timer_t delacktm;
        timer_t keepalivetm; /* not implemented */
        timer_t persisttm; /* not implemented */
	timer_t timewaittm;
	timer_t destroytm;
        void inittimer();
	void stoptimer();
	void rexmit(pkt_t * pkt);
	void entertimewait();
	void rexmitto();
	void delackto();
	void keepaliveto();
	void persistto();
	void timewaitto();
	void destroyto();

        /* retransmit timeout */
	enum {MINRTT = 4*_10ms, INITIALRTT = 2*_100ms, MAXRTT = 2*_1s };
	tick_t rtt; /* round trip time estimation */
	tick_t rto() { return rtt * 2; } /* round trip timeout */
	void updatertt(tick_t newrtt);

	/* various input functions correspond to tcp-state */
	void ackarrived(u32_t ack);
	void input(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th);
	void inputclosed(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th);
	void inputlisten(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th);
	void inputsynsent(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th);
	void inputsynrecv(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th);
	void inputdata(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th);
	void inputestab(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th);
	void inputfinwait1(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th);
	void inputfinwait2(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th);
	void inputtimewait(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th);
	void inputclosewait(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th);
	void inputlastack(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th);
	void inputclosing(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th);

	/* ctor/dtor */
	void dump();
	void check();
	void change(tcpstate_t newstate);
	void initaddr(u32_t laddr_, u16_t lport_, u32_t faddr_, u16_t fport_);
	void changelocal(u32_t newladdr);
	void changelocal(u32_t newladdr, u16_t newlport);
	void changeforeign(u32_t newfaddr, u16_t newfport);
	tcpsock_t();
	tcpsock_t(tcpsock_t *t, iphdr_t *ih, tcphdr_t *th);
	~tcpsock_t();
	void lose();
	
        /* bsd socket interface */ 
	int bind(sockaddr_t *me, socklen_t addrlen);
	int connect(sockaddr_t *serv, socklen_t addrlen);
	int listen(int backlog);
	tcpsock_t * fetchclient();
  	int accept(sockaddr_t *cli, socklen_t *addrlen);
	int getname(sockaddr_t *name, socklen_t *namelen, int peer);
	int getsockname(sockaddr_t *name, socklen_t *namelen);
	int getpeername(sockaddr_t *name, socklen_t *namelen);
	int send(void *buf, size_t len, int flags);
	int recv(void *buf, size_t len, int flags);
	int sendto(void *buf, size_t len, int flags, sockaddr_t *to,
            socklen_t tolen);
	int recvfrom(void *buf, size_t len, int flags, sockaddr_t *from, 
            socklen_t*fromlen);
	int shutdown(int how);
	int setsockopt(int level, int optname, void*optval,socklen_t optlen);
	int getsockopt(int level, int optname, void*optval,socklen_t*optlen);
	int sendmsg(const msghdr_t *mh, int flags);
	int recvmsg(msghdr_t *mh, int flags);

	/* seq compare */
	static u32_t seqadd(u32_t from, int size) { return from + (u32_t)size; }
	static u32_t seqsub(u32_t from, int size) { return from - (u32_t)size; }
	static int seqlen(u32_t from, u32_t to) { return (int)(u32_t)(to - from); }
	static bool seqeq(u32_t a, u32_t b) { return a == b; }
	static bool seqlt(u32_t a, u32_t b) { return (s32_t)(a - b) < 0; }
	static bool seqlteq(u32_t a, u32_t b) { return (s32_t)(a - b) <= 0; }
	static bool seqgt(u32_t a, u32_t b) { return (s32_t)(a - b) > 0; }
	static bool seqgteq(u32_t a, u32_t b) { return (s32_t)(a - b) >= 0; }
	static bool seqlteqlt(u32_t a, u32_t b, u32_t c)
	{
		return seqlteq(a, b) && seqlt(b, c); 
	}
	static bool seqlteqlteq(u32_t a, u32_t b, u32_t c)
	{
		return seqlteq(a, b) && seqlteq(b, c); 
	}
};
QUEUE(all, tcpsock_t);
QUEUE(lhash, tcpsock_t);
QUEUE(fhash, tcpsock_t);

extern u32_t tcpinitialseq;
extern void tcpinput(pkt_t *pkt);

#endif
