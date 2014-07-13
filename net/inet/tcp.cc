#include "root.h" 
#include <init/ctor.h>
#include <mm/allockm.h>
#include <kern/sched.h>
#include <dev/net/eth.h>
#include "tcp.h"
#include "ip.h"

static Q(all, tcpsock_t) allq;
typedef Q(lhash, tcpsock_t) lhashq_t;
typedef Q(fhash, tcpsock_t) fhashq_t;
typedef Q(fhash, tcpsock_t) listenhashq_t;
static int hashsize, hashmask;
static lhashq_t *lhashtab;
static fhashq_t *fhashtab;
static listenhashq_t *listenhashtab;

__ctor(PRINET, SUBANY, inittcphash)
{
	construct(&allq);

	hashsize = 32;
	hashmask = hashsize - 1;

	lhashtab = (lhashq_t*) allocbm(hashsize * sizeof(lhashq_t));
	construct(lhashtab, hashsize);
	fhashtab = (fhashq_t*) allocbm(hashsize * sizeof(fhashq_t));
	construct(fhashtab, hashsize);
	listenhashtab = (fhashq_t*) allocbm(hashsize * sizeof(fhashq_t));
	construct(listenhashtab, hashsize);
}

static inline lhashq_t * lhashfunc(u16_t lport)
{
	return lhashtab + (ntohs(lport) & hashmask);
}

/* a busy server has many connections which has the same local port, 
   so we use the foreign port as the hash key */
static inline fhashq_t * fhashfunc(u16_t fport)
{
	return fhashtab + (ntohs(fport) & hashmask);
}

static inline listenhashq_t * listenhashfunc(u16_t lport)
{
	return listenhashtab + (ntohs(lport) & hashmask);
}

static bool exist(u32_t laddr, u16_t lport, bool reuseaddr)
{
	lhashq_t * lhashq = lhashfunc(lport);
	tcpsock_t * s;
	foreach (s, *lhashq) {
 		if ((s->laddr == INADDRANY || laddr == INADDRANY || s->laddr == laddr) &&
		    (s->lport == lport) && 
		    !(reuseaddr && s->state == tcpsock_t::STIMEWAIT))
			return true;
	}
	return false;
}

static u16_t newtcpport()
{
	/* well known ports ranges from 0 to 1023 */
	static u16_t begin = USERSOCK;
	while (exist(INADDRANY, begin, false))
		if (!++begin) /* overflow */
			begin = USERSOCK;
	return htons(begin);
}

void tcphdr_t::dump()
{
	printf("sport:%d dport:%d seq:%u ack:%u wnd:%d\n", 
	       ntohs(sport), ntohs(dport), ntohl(seq), ntohl(ack), ntohs(wnd));
	printf("fin:%d syn:%d rst:%d psh:%d ack:%d urg:%d\n", 
                flags & TCPFFIN, flags & TCPFSYN, flags & TCPFRST, 
                flags & TCPFPSH, flags & TCPFACK, flags & TCPFURG);
}


int tcpsock_t::rcvwnd()
{
	if (currcvbuf < LINKIPTCPHLEN + mss)
		return 0;
	return currcvbuf;
}

/* <-- (seq=1 len=1 ack=1) (seq=2 len=0 ack=2) (seq=2 len=0 ack=3) --- */
void tcpsock_t::updatesndwnd(tcphdr_t *th)
{
	u32_t seq = ntohl(th->seq);
	u32_t ack = ntohl(th->ack);
	u16_t newsndwnd = ntohs(th->wnd);
	if (seqlt(lastseq, seq) ||
	    seqeq(lastseq, seq) && (th->flags & TCPFACK) && seqlt(lastack, ack) ||
	    (th->flags & TCPFACK) && seqeq(lastack, ack) && (sndwnd < newsndwnd)) {
		lastseq = seq;
		lastack = ack;
		sndwnd = newsndwnd;
	} else
		debug(TCPDBG, "update sndwnd:lastseq=%u lastack=%u seq=%u ack=%u\n", 
		      lastseq, lastack, seq, ack);
}

static char * statstr[] = {"CLOSED", "LISTEN", "SYNSENT", "SYNRECV", "ESTAB", 
"FINWAIT1", "FINWAIT2", "TIMEWAIT", "CLOSEWAIT", "LASTACK", "CLOSING"};
void tcpsock_t::dump()
{
	printf("%s [%s::%d] [%s::%d] ", statstr[state], inetntoa(laddr), 
	       ntohs(lport), inetntoa(faddr), ntohs(fport));
	printf("sndnxt=%u sndwnd=%d rcvnxt=%u rcvwnd=%d\n", sndnxt, sndwnd, rcvnxt, 
	       rcvwnd());
}

void tcpsock_t::check()
{
	return;
#if DEBUG
	pkt_t * p;
	if (p = unackedq.tail()) {
		if (p->endseq != sndnxt)
			printf("%x %u %u %u\n", this, p->startseq, p->endseq, sndnxt);
		assert(p->endseq == sndnxt);
	}
	if (p = recvq.tail())
		assert(p->endseq == rcvnxt);
	if (p = oosrecvq.head())
		assert(seqlt(rcvnxt, p->startseq));
	if (unackedq.empty()) {
		//assert(cursndbuf == maxsndbuf);
	}
	if (recvq.empty() && oosrecvq.empty()) 	{
		//printf("%d %d\n", currcvbuf, maxrcvbuf); 
		//assert(currcvbuf == maxrcvbuf);
	}
#endif
}

void tcpsock_t::change(tcpstate_t newstate)
{
	debug(TCPDBG, "%s-->%s\n", statstr[state], statstr[newstate]);
	state = newstate;
}

/* ready to send syn bit */
void tcpsock_t::initiss()
{
	static u32_t tmp = 123;
	iss = sndnxt = lastack = tmp;
	tmp += passedtick;
}

/* recv peer's syn bit */
void tcpsock_t::initirs(tcphdr_t *th)
{
	irs = lastseq = ntohl(th->seq);
	rcvnxt = seqadd(irs, 1);
	sndwnd = ntohs(th->wnd);
	mss = ETHMTU - (sizeof(iphdr_t) + sizeof(tcphdr_t));
	parseopt(th);
}

void tcpsock_t::changelocal(u32_t newladdr)
{
	laddr = newladdr;
}

void tcpsock_t::changelocal(u32_t newladdr, u16_t newlport)
{
	laddr = newladdr;
	lport = newlport;
	unlinklhash();
	lhashfunc(newlport)->enqtail(this);
}

void tcpsock_t::changeforeign(u32_t newfaddr, u16_t newfport)
{
	faddr = newfaddr;
	fport = newfport;
	unlinkfhash();
	fhashfunc(newfport)->enqtail(this);
}

void tcpsock_t::initaddr(u32_t laddr_, u16_t lport_, u32_t faddr_, u16_t fport_)
{
	nextall = prevall = NULL; 
	allq.enqtail(this); 
        nextincoming = previncoming = listening = NULL; 

	laddr = laddr_;
	lport = lport_;
	nextlhash = prevlhash = NULL; 
	lhashfunc(lport)->enqtail(this); 

	faddr = faddr_;
	fport = fport_;
	nextfhash = prevfhash = NULL; 
	fhashfunc(fport)->enqtail(this);
}

tcpsock_t::tcpsock_t()
{
	state = SCLOSED;
	initaddr(INADDRANY, newtcpport(), INADDRANY, 0);

	mss = ETHMTU - (sizeof(iphdr_t) + sizeof(tcphdr_t));
	assert(mss == 1460);

	iss = sndnxt = lastack = 0;
	irs = rcvnxt = lastseq = 0;
	finrecved = false;
	sndwnd = 0;

	congwnd = 0;
	slowstartthresh = 0;

	nrexmit = 0;
	ndelack = 0;
	inittimer();
}

/* the listening socket will create a new socket on accept the connection 
   @l - the listening socket  */
tcpsock_t::tcpsock_t(tcpsock_t * l, iphdr_t *ih, tcphdr_t *th)
{
	state = SLISTEN;
	initaddr(ih->daddr, th->dport, ih->saddr, th->sport);
	listening = l;
	l->incomingq.enqtail(this);

	initiss();
	initirs(th);
	finrecved = false;
	
	congwnd = 0;
	slowstartthresh = 0;

	nrexmit = 0;
	ndelack = 0;
	stoptimer();
	inittimer();
}

void tcpsock_t::lose()
{
	assert(refcnt > 0);
	if (--refcnt)
		return;
	shutdown(2);
	destroytm.start();
}

tcpsock_t::~tcpsock_t()
{
	if (nextall)
		allq.unlink(this);
	if (nextlhash)
		unlinklhash();
	if (nextfhash)
		unlinkfhash();
	if (nextincoming)
		unlinkincoming();
	unackedq.zapall();
	recvq.zapall();
	oosrecvq.zapall();
}

/* the bind operation succeed in case that:
   1) bind the socket(named A) to a port that is not used.
   2) bind the socket(named A) to a port that is already used by 
      another socket(named B), but 
      - the socket A has SO_REUSEADDR flags set on
      - the socket B is in the TIME_WAIT state */
int tcpsock_t::bind(sockaddr_t *me_, socklen_t myaddrlen)
{
	sockaddrin_t * me = (sockaddrin_t*) me_;
	if (state != SCLOSED)
		return EINVAL;
	if (myaddrlen != sizeof(sockaddrin_t))
		return EINVAL;
	if (me->port < USERSOCK && !suser())
		return EACCES;
	if (exist(me->addr, me->port, soreuseaddr))
		return EADDRINUSE;
	changelocal(me->addr, me->port);
        return 0;
}

int tcpsock_t::listen(int backlog_)
{
	backlog = backlog_;
	if (state != SCLOSED)
		return EINVAL;
	/* register the socket into the listening hash table */
	state = SLISTEN;
	unlinkfhash();
	listenhashfunc(lport)->enqtail(this);
        return 0;
}

int tcpsock_t::connect(sockaddr_t *serv_, socklen_t socklen)
{
	if (state != SCLOSED || faddr)
		return EISCONN;
	sockaddrin_t * serv = (sockaddrin_t*) serv_;
	netdev_t *netdev;
	u32_t nexthop;
	int e = selectroute(serv->addr, &netdev, &nexthop);
	if (e)
		return EHOSTUNREACH;
	changelocal(netdev->praddr);
	changeforeign(serv->addr, serv->port);
	initiss();
	sndwnd = 8096; /* assume the peer socket's send buffer */
        sendflagsmss(TCPFSYN);
	change(SSYNSENT);
        while (state != SESTAB)
                WAIT(waitq);
        return 0;
}

tcpsock_t* tcpsock_t::fetchclient()
{
	tcpsock_t *t;
	foreach (t, incomingq) {
		if (t->state == SESTAB) {
			incomingq.unlink(t);
			assert(t->listening == this);
			t->listening = NULL;
			return t;
		}
	}
	return NULL;
}

/* 1) listening socket wait for SYN 
   2) new socket wait for ACK */
int tcpsock_t::accept(sockaddr_t *sa, socklen_t *salen)
{
	if (sa && *salen != sizeof(sockaddrin_t))
		return EINVAL;
	tcpsock_t * client;
	while ((client = fetchclient()) == NULL)
		WAIT(waitq);
	if (sa)
		client->getsockname(sa, salen);
	int e = curr->fdvec->put(client);
	if (e < 0)
		delete client;
	return e;
}

int tcpsock_t::getname(sockaddr_t *name, socklen_t *namelen, int peer)
{
	if (*namelen != sizeof(sockaddrin_t))
		return EINVAL;
	sockaddrin_t *sin = (sockaddrin_t *) name;
	sin->family = AFINET;
	if (peer) {
		sin->port = fport;
		sin->addr = faddr;
	} else {
		sin->port = lport;
		sin->addr = laddr;
	}
	 *namelen = sizeof(sockaddrin_t);
	return 0;
}

int tcpsock_t::getsockname(sockaddr_t *name, socklen_t *namelen)
{
	return getname(name, namelen, 0);
}

int tcpsock_t::getpeername(sockaddr_t *name, socklen_t *namelen)
{
	return getname(name, namelen, 1);
}

/* send a fin without close the socket */
int tcpsock_t::shutdown(int how)
{
	tcpstate_t newstate;
	if (state == SESTAB)
		newstate = SFINWAIT1;
	else if (state == SCLOSEWAIT)
		newstate = SLASTACK;
	else
		return ENOTCONN;
	sendflags(TCPFFIN | TCPFACK);
	change(newstate);
        return 0;
}

int tcpsock_t::setsockopt(int level, int optname, void *optval, socklen_t optlen)
{
        return 0;
}

int tcpsock_t::getsockopt(int level, int optname, void *optval, socklen_t *optlen)
{
        return 0;
}

/* mss sndbuf sndwnd */
int tcpsock_t::send(void *buf, size_t buflen, int flags)
{
	if (state < SESTAB)
		return ENOTCONN;
	check();
	int left = buflen;
	while (left) {
		int once = min(mss, left);
		int e = output(AUSER, TCPFACK | TCPFPSH, buf, once);
		if (e)
			return e;
		left -= once, buf += once;
	}
	return buflen;
}

int tcpsock_t::sendto(void *buf, size_t len, int flags, sockaddr_t *to_,
    socklen_t tolen)
{
	sockaddrin_t * to = (sockaddrin_t*) to_;
	if (tolen != sizeof(sockaddrin_t))
		return EINVAL;
	if (to->family != AFINET || to->addr != faddr || to->port != fport)
		return EINVAL;
	return send(buf, len, flags);
}

int tcpsock_t::sendmsg(const msghdr_t *mh, int flags)
{
	return 0;
}

int tcpsock_t::recv(void *buf, size_t len, int flags)
{
	if (state < SESTAB)
		return ENOTCONN;
	while (recvq.empty()) {
		if (finrecved)
			return 0;
		WAIT(waitq);
	}
	check();
	if (flags & MSGPEEK)
		return recvq.peek(buf, len);
	int freedroom;
	int e = recvq.fetch(buf, len, &freedroom);
	freercvbuf(freedroom);
	return e;
}

int tcpsock_t::recvfrom(void *buf, size_t len, int flags, sockaddr_t *from_, 
    socklen_t *fromlen)
{
	sockaddrin_t * from = (sockaddrin_t*) from_;
	if (from) {
		if (*fromlen != sizeof(sockaddrin_t))
			return EINVAL;
		from->family = AFINET;
		from->addr = faddr;
		from->port = fport;
	}
        return recv(buf, len, flags);
}

int tcpsock_t::recvmsg(msghdr_t *mh, int flags)
{
	return 0;
}

void tcpinput(pkt_t *pkt)
{
	iphdr_t * ih = (iphdr_t *) pkt->data;
	pkt->delhead(ih->headlen());
	tcphdr_t * th = (tcphdr_t *) pkt->data;
	pkt->delhead(th->getheadlen());

	if (tcpchksum(ih->saddr, ih->daddr, ih->datalen(), ih->data)) {
		printd("tcp check sum failed\n");
		delpkt(pkt);
		return;
	}

#define localmatch ((s->laddr == INADDRANY || s->laddr == ih->daddr) \
	&& (s->lport == th->dport))
#define foreignmatch ((s->faddr == ih->saddr) && (s->fport == th->sport))

	/* lookup for perfect match, use th->sport as the key */
	tcpsock_t * s;
	fhashq_t * fhashq = fhashfunc(th->sport);
	foreach (s, *fhashq) {
		if (s->state == tcpsock_t::SCLOSED || s->state == tcpsock_t::STIMEWAIT)
			continue;
		if (localmatch && foreignmatch) {
			s->input(pkt, ih, th);
			return;
		}
	}

	/* lookup for half match, use th->dport as the key */
	listenhashq_t * listenhashq = listenhashfunc(th->dport);
	foreach (s, *listenhashq) {
		assert(s->state == tcpsock_t::SLISTEN);
		if (localmatch) {
			s->input(pkt, ih, th);
			return;
		}
	}
        delpkt(pkt);
}
