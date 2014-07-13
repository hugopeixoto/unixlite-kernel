#include "root.h" 
#include <lib/errno.h>
#include <init/ctor.h>
#include <mm/allockm.h>
#include <mm/layout.h>
#include "udp.h"
#include "ip.h"

static Q(all, udpsock_t) allq;
typedef Q(hash, udpsock_t) hashq_t;
static int hsize, hmask;
static hashq_t *hashtab;

static inline hashq_t * hashfunc(u16_t lport)
{
	return hashtab + (ntohs(lport) & hmask);
}

__ctor(PRINET, SUBANY, initudphash)
{
	construct(&allq);
	hsize = min(32, nphysmeg);
	hmask = hsize - 1;
	hashtab = (hashq_t*) allocbm(hsize * sizeof(hashq_t));
	construct(hashtab, hsize);
}

static bool exist(u32_t laddr, u16_t lport)
{
	hashq_t * hashq = hashfunc(lport);
	udpsock_t * s;
	foreach (s, *hashq) {
		if ((s->laddr == INADDRANY || laddr == INADDRANY || s->laddr == laddr) &&
		    (s->lport == lport))
			return true;
	}
	return false;
}

static u16_t newudpport()
{
	/* well known ports ranges from 0 to 1023 */
	static u16_t begin = USERSOCK;
	while (exist(INADDRANY, begin))
		if (!++begin) /* overflow */
			begin = USERSOCK;
	return htons(begin);
}

void udpinput(pkt_t *pkt)
{
	iphdr_t * ih = (iphdr_t*) pkt->data;
	udphdr_t * uh = (udphdr_t*) (pkt->data + ih->headlen());

	if (uh->chksum && udpchksum(ih->saddr, ih->daddr, ih->datalen(), ih->data)) {
		printd("udp check sum failed\n");
		delpkt(pkt);
		return;
	}

	hashq_t * hashq = hashfunc(uh->dport);
        udpsock_t *s;
	/* demultiplex the packet */
	foreach (s, *hashq) {
		bool localmatch = (s->laddr == INADDRANY || s->laddr == ih->daddr) &&
			          (s->lport == uh->dport);
		if (!localmatch)
			continue;
		/* if s is not connected, ignore ih->saddr, uh->sport */
		if ((s->faddr == INADDRANY) ||  
		    (s->faddr == ih->saddr && s->fport == uh->sport)) {
			s->input(pkt);
			return;
		}
			
	}
	printd("no udp socket accept this packet(saddr=%s sport=%d daddr=%s dport=%d\n", 
        inetntoa(ih->saddr), ntohs(uh->sport), inetntoa(ih->daddr), ntohs(uh->dport));
	delpkt(pkt);
}

void udpsock_t::dump()
{
	printf("laddr = %s, lport = %d, faddr = %s, fport = %d\n", 
	       inetntoa(laddr), ntohs(lport), inetntoa(faddr), ntohs(fport));
}

udpsock_t::udpsock_t()
{
	backlog = 8;
	mss = ETHMTU - LINKIPUDPHLEN;

	laddr = INADDRANY;
	lport = newudpport();
	faddr = INADDRANY;
	fport = 0;
	nextall = prevall = nexthash = prevhash = NULL;
	allq.enqtail(this);
	hashfunc(lport)->enqtail(this);
}

void udpsock_t::input(pkt_t *pkt)
{
	recvq.enqtail(pkt);
	waitq.broadcast();
}

udpsock_t::~udpsock_t()
{
	allq.unlink(this);
	unlinkhash();
}

int udpsock_t::bind(sockaddr_t * me_, socklen_t socklen)
{
	sockaddrin_t * me = (sockaddrin_t *) me_;
	int e = verw(me, sizeof(sockaddrin_t));
	if (e < 0)
		return e;
	if (ntohs(me->port) < USERSOCK && !suser())
		return EACCES;
	if (exist(me->addr, me->port))
		return EADDRINUSE;
#warning "need check me->addr is one of the NIC's praddr"
	unlinkhash();
	laddr = me->addr;
	lport = me->port;
	hashfunc(lport)->enqtail(this);
	return 0;
}

int udpsock_t::connect(sockaddr_t * serv_, socklen_t socklen)
{
	sockaddrin_t * serv = (sockaddrin_t *) serv_;
	int e = verr(serv, sizeof(*serv));
	if (e < 0)
		return e;
	faddr = serv->addr;
	fport = serv->port;
	return 0;
}

int udpsock_t::listen(int backlog_)
{
	backlog = backlog_;
	return 0;
}

int udpsock_t::accept(sockaddr_t * client, socklen_t * addrlen)
{
	return EOPNOTSUPP;
}

int udpsock_t::getname(sockaddr_t * name, socklen_t * namelen, int peer)
{
	int e;
	if (e = verw(namelen, sizeof(socklen_t))) 
		return e;
	if (*namelen < sizeof(sockaddrin_t))
		return EINVAL;
	if (e = verw(name, sizeof(sockaddr_t))) 
		return e;
	sockaddrin_t * sin = (sockaddrin_t*) name;
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

int udpsock_t::getsockname(sockaddr_t * name, socklen_t * namelen)
{
	return getname(name, namelen, 0);
}

int udpsock_t::getpeername(sockaddr_t * name, socklen_t * namelen)
{
	return getname(name, namelen, 1);
}

/* laddr maybe equal INADDRANY, we compute the chksum after routing */
int udpsock_t::output(pkt_t * pkt, u32_t daddr, u16_t dport)
{
	udphdr_t * uh = (udphdr_t*)pkt->addhead(sizeof(udphdr_t));
	uh->sport = lport;
	uh->dport = dport; 
	uh->totlen = htons(pkt->datalen);
	uh->chksum = 0;
        return ipoutput(pkt, IPPROTOUDP, laddr, daddr, &uh->chksum);
}

int udpsock_t::send(void * buf, size_t buflen, int flags)
{
        if (faddr == INADDRANY)
                return EINVAL;
        return 0;
}

int udpsock_t::sendto(void * buf, size_t buflen, int flags, sockaddr_t * to_,
            socklen_t tolen)
{
	sockaddrin_t * to = (sockaddrin_t*) to_;
        while (buflen) {
                size_t once = min(mss, buflen);
                pkt_t * pkt = newpkt(AUSER, LINKIPUDPHLEN, once);
		if (!pkt)
			return ENOMEM;
		memcpy(pkt->data, buf, once);
		int e;
                if (e = output(pkt, to->addr, to->port)) {
			delpkt(pkt);
			return e;
		}
                buf += once;
                buflen -= once;
        }
        return 0;
}

int udpsock_t::sendmsg(const msghdr_t *mh, int flags)
{
        return 0;
}

int udpsock_t::recv(void * buf, size_t len, int flags)
{
        return recvfrom(buf, len, flags, NULL, NULL);
}

int udpsock_t::recvfrom(void * buf, size_t len, int flags, 
        sockaddr_t *from_, socklen_t *fromlen)
{
        sockaddrin_t * from = (sockaddrin_t*) from_;
        while (recvq.empty())
		WAIT(waitq);
        pkt_t * pkt = recvq.deqhead();
	iphdr_t * ih = (iphdr_t*) pkt->data;
	pkt->delhead(ih->headlen());
	udphdr_t * uh = (udphdr_t*) pkt->data;
	pkt->delhead(sizeof(udphdr_t));

        int once = min(len, pkt->datalen);
        memcpy(buf, pkt->data, once); 
        if (from) {
                from->family = AFINET;
                from->addr = ih->saddr;
                from->port = uh->sport;
        }
        if (flags & MSGPEEK)
                recvq.enqhead(pkt);
        return once; 
}

int udpsock_t::recvmsg(msghdr_t *mh, int flags)
{
	return 0;
}

int udpsock_t::shutdown(int how)
{
	return 0;
}

int udpsock_t::setsockopt(int level, int optname, void* optval, socklen_t optlen)
{
	return 0;
}

int udpsock_t::getsockopt(int level, int optname, void* optval, socklen_t* optlen)
{
	return 0;
}
