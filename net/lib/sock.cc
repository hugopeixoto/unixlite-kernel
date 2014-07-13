#include <lib/root.h>
#include <lib/errno.h>
#include <mm/allockm.h>
#include "pkt.h"
#include "sock.h"
#include <net/lib/if.h>
#include <net/lib/route.h>
#include <net/lib/sockios.h>
#include <dev/net/dev.h>
#include <net/inet/inet.h>
#include <net/inet/route.h>
#include <net/inet/debug.h>

sock_t::sock_t()
{
	type = SOCKFD;
	fdflags = FDREAD | FDWRITE;
	backlog = 0;
	solinger.on = 0;
	solinger.seconds = 0;
	soreuseaddr = 0;
	sodebug = 0;
	sokeepalive = 0;
	sooobinline = 0;
	sopriority = 0;
	sobroadcast = 0;
	maxsndbuf = cursndbuf = MAXSNDBUF;
	maxrcvbuf = currcvbuf = MAXRCVBUF;
}

sock_t::~sock_t()
{
}

static int ifreqioctl(int cmd, ulong arg)
{
	ifreq_t *ifreq = (ifreq_t*) arg;
	int e;
	if (e = verw(ifreq, sizeof(ifreq_t)))
		return e;

	sockaddrin_t * sin = (sockaddrin_t*) &ifreq->u.addr;
	sin->family = AFINET;
	sin->port = 0;
	netdev_t *netdev = findnetdev(ifreq->name);
	if (!netdev)
		return EINVAL;
	switch (cmd) {
	case SIOCGIFFLAGS:
		ifreq->u.flags = netdev->flags;
		break;
	case SIOCSIFFLAGS:
		netdev->flags = ifreq->u.flags;
		break;

	case SIOCGIFADDR:
		sin->addr = netdev->praddr;
		break;
	case SIOCSIFADDR:
		netdev->praddr = sin->addr;
		break;

	case SIOCGIFBRDADDR:
		sin->addr = netdev->prbcastaddr;
		break;
	case SIOCSIFBRDADDR:
		netdev->prbcastaddr = sin->addr;
		break;

	case SIOCGIFDSTADDR:
		break;
	case SIOCSIFDSTADDR:
		break;

	case SIOCGIFNETMASK:
		sin->addr = netdev->netmask;
		break;
	case SIOCSIFNETMASK:
		netdev->netmask = sin->addr;
		break;

	case SIOCGIFMETRIC:
		ifreq->u.metric = netdev->metric;
		break;
	case SIOCSIFMETRIC:
		netdev->metric = ifreq->u.metric;
		break;

	case SIOCGIFMTU:
		ifreq->u.mtu = netdev->mtu;
		break;
	case SIOCSIFMTU:
		netdev->mtu = ifreq->u.mtu;
		break;

	case SIOCGIFMEM:
		break;
	case SIOCSIFMEM:
		break;
 
	case SIOCGIFHWADDR:
		memcpy(ifreq->u.hwaddr.data, netdev->hwaddr, netdev->hwaddrlen);
		break;
	case SIOCSIFHWADDR:
		memcpy(netdev->hwaddr, ifreq->u.data, netdev->hwaddrlen);
		break;
	}
	return 0;
}

int ifconfioctl(int cmd, ulong arg)
{
	ifconf_t *ifc = (ifconf_t*) arg;
	int e;
	if (e = verw(ifc, sizeof(ifconf_t)))
		return e;
	if (e = verw(ifc->u.buf, ifc->buflen))
		return e;
	int num = ifc->buflen / sizeof(ifreq_t);
	ifreq_t *ifr = ifc->u.ifreq;
	netdev_t *netdev;
	foreach (netdev, netdevq) {
		strcpy(ifr->name, netdev->name);
		sockaddrin_t *sin = (sockaddrin_t*) &ifr->u.addr;
		sin->family = AFINET;
		sin->addr = netdev->praddr;
		ifr++;
		if (--num == 0)
			break;
	}
	ifc->buflen = (char*)ifr - (char*)ifc->u.ifreq;
	return 0;
}

static u32_t ip(sockaddr_t *sa)
{
	return ((sockaddrin_t*)sa)->addr;
}

static int routeioctl(int cmd, ulong arg)
{
	int e;
	linuxrt_t * l = (linuxrt_t*) arg;
	if (e = verw(l, sizeof(linuxrt_t)))
		return e;
	if (e = verrstr(l->dev))
		return e;
	netdev_t * netdev = findnetdev(l->dev);
	if (cmd == SIOCADDRT)
		addroute(l->flags, ip(&l->dstaddr), ip(&l->netmask), ip(&l->gateway), netdev);
	else if (cmd == SIOCDELRT)
		;//delroute(ip(&l->dstaddr));
	return 0;
}

int sock_t::ioctl(int cmd, ulong arg)
{

	if (cmd == SIOCGIFCONF)
		return ifconfioctl(cmd, arg);
	if (cmd == SIOCADDRT || cmd == SIOCDELRT)
		return routeioctl(cmd, arg);
	return ifreqioctl(cmd, arg);
}

int sock_t::read(void * buf, int count)
{
	return recv(buf, count,  0);
}

int sock_t::write(void * buf, int count)
{
	return send(buf, count,  0);
}

int sock_t::allocsndbuf(pkt_t **pp, int flags, int headlen, int datalen)
{
	int roomlen = headlen + datalen;
        /* if flags is AKERN, force allocation */
	while (flags != AKERN && cursndbuf < roomlen) {
		debug(TCPDBG, "wait for sndbuf\n");
		WAIT(waitq);
	}
	*pp = newpkt(flags, headlen, datalen);
	if (!*pp)
		return ENOMEM;
	cursndbuf -= roomlen;
	return 0;
}

void sock_t::freesndbuf(pkt_t *p)
{
	cursndbuf += p->roomlen();
	waitq.broadcast();
	delpkt(p);
}

int sock_t::allocrcvbuf(pkt_t *p)
{
	if (currcvbuf < p->roomlen())
		return ENOMEM;
	currcvbuf -= p->roomlen();
	return 0;
}

void sock_t::freercvbuf(pkt_t *p)
{
	freercvbuf(p->roomlen());
	delpkt(p);
}

/* set the SOL_SOCKET option */
int sock_t::setsocketopt(int optname, void * optvalptr, socklen_t optlen)
{
/* NOTE: memcpy(&optval, optval_, optlen) is not worked on the big-endian machine */
	int optval = 0;
#define GUESS(type) if (optlen == sizeof(type)) { optval = *(type*)optvalptr; }
	GUESS(char) else GUESS(short) else GUESS(int);
  	switch(optname) {
		case SOTYPE:
		case SOERROR:
		case SODONTROUTE:
			return 0;

		case SOBROADCAST:
			sobroadcast = optval;
			return 0;
		case SODEBUG:
			if (!suser())
				return EPERM;
			sodebug = optval;
		case SOSNDBUF:
			maxsndbuf = minmax(MINSNDBUF, optval, MAXSNDBUF);
			return 0;
		case SORCVBUF:
			maxrcvbuf = minmax(MINRCVBUF, optval, MAXRCVBUF);
			return 0;
		case SOREUSEADDR:
			soreuseaddr = optval;
			return 0;
		case SOLINGER:
			if (optlen != sizeof(linger_t))
				return EINVAL;
			solinger = *(linger_t*)optvalptr;
			return 0;
		case SOKEEPALIVE:
			sokeepalive = optval;
			return 0;
	 	case SOOOBINLINE:
			sooobinline = optval;
			return 0;
	        case SOPRIORITY:
			sopriority = optval;
			return 0;
		default:
		  	return(ENOPROTOOPT);
  	}
	return 0;
}

/* get the SOL_SOCKET option */
int sock_t::getsocketopt(int optname, void * optvalptr, socklen_t * optlen)
{
	int optval = 0;
  	switch(optname) {
		case SOERROR:
		case SODONTROUTE:
			return 0;

		case SODEBUG:
			optval = sodebug;
			break;
		case SOBROADCAST:
			optval = sobroadcast;
			break;
		case SOLINGER: 
			if (*optlen != sizeof(linger_t))
				return EINVAL;
			*(linger_t*)optvalptr = solinger;
			return 0;
		case SOSNDBUF:
			optval = maxsndbuf;
			break;
		case SORCVBUF:
			optval = maxrcvbuf;
			break;
		case SOREUSEADDR:
			optval = soreuseaddr;
			break;
		case SOKEEPALIVE:
			optval = sokeepalive;
			break;
		case SOTYPE:
			/* optval = SOCKSTREAM */
			break;
		case SOOOBINLINE:
			optval = sooobinline;
			break;
		case SOPRIORITY:
			optval = sopriority;
			break;
		default:
			return(ENOPROTOOPT);
	}
	if (*optlen != sizeof(int))
		return EINVAL;
	*optlen = sizeof(int);
	*(int*)optvalptr = optval;
  	return(0);
}
