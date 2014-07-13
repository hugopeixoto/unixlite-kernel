#include "root.h"
#include <lib/errno.h>
#include <init/ctor.h>
#include <mm/allockm.h>
#include <mm/layout.h>
#include "raw.h"
#include "ip.h"

typedef Q(all,rawsock_t) allq_t;
static allq_t allq;

__ctor(PRINET,SUBANY,initrawhash)
{
	construct(&allq);
}

static bool exist(u8_t proto, u32_t laddr)
{
	rawsock_t *s;
	foreach (s, allq) {
		if ((s->proto == proto) &&
		    (s->laddr == INADDRANY || laddr == INADDRANY || s->laddr == laddr))
			return true;
	}
	return false;
}

/* return >0 if the input packet is accepted by at least one raw socket */
int rawinput(pkt_t *pkt)
{
	pkt_t *p;
	int count = 0;
	rawsock_t *s;

	foreach (s, allq) {
		if (!s->belongtome(pkt))
			continue;
		if (count++ == 0) {
			p = pkt;
		} else {
			p = pkt->clone(AUSER);
			if (!p) 
				return 0;
		}
		s->recvq.enqtail(p);
		s->waitq.broadcast();
	}
	return count;
}

int rawsock_t::belongtome(pkt_t *pkt)
{
	iphdr_t *ih = (iphdr_t*) pkt->data;
	return ((proto == 0 || proto == ih->proto) &&
		(laddr == INADDRANY || laddr != ntohl(ih->daddr)) &&
		(faddr == INADDRANY || faddr != ntohl(ih->saddr)));
}

rawsock_t::rawsock_t(u8_t proto_)
{
	backlog = 8;
	proto = proto_;
	laddr = INADDRANY;
	faddr = INADDRANY;
	nextall = prevall = NULL;
	allq.enqtail(this);
}

rawsock_t::~rawsock_t()
{
	if (nextall)
		unlinkall();
}

int rawsock_t::sendto(void *buf, size_t buflen, int flags, sockaddr_t *to_,
		      socklen_t tolen)
{
	sockaddrin_t *to = (sockaddrin_t*) to_;
	if (tolen != sizeof(sockaddrin_t))
		return EINVAL;
	pkt_t *pkt = newpkt(AUSER, LINKIPHLEN, buflen);
	if(!pkt)
		return ENOMEM;
	memcpy(pkt->data, buf, buflen);
	return ipoutput(pkt, proto, laddr, to->addr);
}

int rawsock_t::recvfrom(void *buf, size_t len, int flags, sockaddr_t *from_,
			socklen_t *fromlen)
{
	sockaddrin_t *from = (sockaddrin_t *)from_;
	if(from) {
		from->family = AFINET;
		from->addr = laddr;
	}
	while(recvq.empty())
		WAIT(waitq);
	return recvq.fetch(buf,len);
}

int rawsock_t::bind(sockaddr_t * me_, socklen_t addrlen)
{
	sockaddrin_t *me = (sockaddrin_t *) me_;
	int e = verw(me, sizeof(sockaddrin_t));
	if(e < 0)
		return e;
	if(exist(proto, me->addr))
		return EADDRINUSE;
	laddr = me->addr;
	return 0;
}

int rawsock_t::connect(sockaddr_t * serv_, socklen_t addrlen)
{
	sockaddrin_t *serv = (sockaddrin_t *)serv_;
	int e = verr(serv, sizeof(*serv));
	if(e < 0)
		return e;
	faddr = serv->addr;
	return 0;
}

int rawsock_t::listen(int backlog)
{
	return 0;
}

int rawsock_t::accept(sockaddr_t * cli, socklen_t * addrlen)
{
	return 0;
}

int rawsock_t::getsockname(sockaddr_t * name, socklen_t * namelen)
{
	return 0;
}

int rawsock_t::getpeername(sockaddr_t * name, socklen_t * namelen)
{
	return 0;
}

int rawsock_t::send(void * buf, size_t len, int flags)
{
	return 0;
}

int rawsock_t::recv(void * buf, size_t len, int flags)
{
	return recvfrom(buf,len,flags,NULL,NULL);
}

int rawsock_t::shutdown(int how)
{
	return 0;
}

int rawsock_t::setsockopt(int level, int optname, void* optval, socklen_t optlen)
{
	return 0;
}

int rawsock_t::getsockopt(int level, int optname, void* optval, socklen_t* optlen)
{
	return 0;
}

int rawsock_t::sendmsg(const msghdr_t * mh, int flags)
{
	return 0;
}

int rawsock_t::recvmsg(msghdr_t * mh, int flags)
{
	return 0;
}
