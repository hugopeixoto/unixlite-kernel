#include <lib/root.h>
#include <lib/printf.h>
#include <init/ctor.h>
#include <net/lib/pkt.h>
#include <net/inet/inet.h>
#include <net/inet/route.h>
#include <net/inet/arp.h>
#include <net/inet/ip.h>
#include <net/inet/udp.h>
#include <net/inet/tcp.h>
#include <net/inet/debug.h>
#include <net/lib/if.h>
#include "eth.h"
#include "dev.h"
#include "loop.h"

netdevq_t netdevq;
softirq_t * netsoftirq;
static pktq_t backlog;
extern void netsoftisr(void *);

__ctor(PRINET, SUB1ST, initnetdev)
{
	construct(&netdevq);
	construct(&backlog);
	netsoftirq = allocsoftirq(netsoftisr, NULL);

	netdevq.enqtail(loopnetdev = new loopnetdev_t());
	addlocalroute(loopnetdev);
}

/* strip ethernet header before pass this packet to upper layer */
void netdev_t::input(pkt_t* pkt)
{
	pkt->inputdev = this;
	backlog.enqtail(pkt);
	netsoftirq->request();
}

static void netrx(pkt_t *pkt)
{
       	ethhdr_t * eh;
	eh = (ethhdr_t*) pkt->data;
	pkt->delhead(ETHHLEN);
	switch (ntohs(eh->proto)) {
		case ETHPIP:
			ipinput(pkt);
			break;
		case ETHPARP:
			arpinput(pkt);
			break;
		default:
			debug(LINKDBG, "netdev_t::input: unknown protocol:%x\n", 
			ntohs(eh->proto));
	}
}

void netsoftisr(void *data)
{
	pkt_t * pkt;
	while (cli(), pkt = backlog.deqhead(), sti(), pkt)
		netrx(pkt);
}

void addnetdev(netdev_t * dev)
{
	static int ethx;
	sprintf(dev->name, "eth%d", ethx++);
	netdevq.enqtail(dev);
}

bool tothishost(u32_t daddr)
{
	netdev_t * netdev;
	foreach (netdev, netdevq) {
		if (netdev->praddr == INADDRANY || netdev->praddr == daddr)
			return true;
	}
	return false;
}

netdev_t *findnetdev(char *name)
{
	netdev_t * netdev;
	foreach (netdev, netdevq) {
		if (strcmp(name, netdev->name) == 0)
			return netdev;
	}
	return NULL;
}

void netdev_t::dump(pkt_t *pkt)
{
	ethhdr_t *eh = (ethhdr_t*) pkt->data;
	iphdr_t *ih = (iphdr_t*) (eh + 1);
	udphdr_t *uh = (udphdr_t*) (ih + 1);
	tcphdr_t *th = (tcphdr_t*) (ih + 1);

	printf("PACKET:\t");
	printf("(%s,%s)\t", ethaddrtoa(eh->src), ethaddrtoa(eh->dst));
	if (ntohs(eh->proto) != ETHPIP)
		return;
	switch (ih->proto) {
	case IPPROTOUDP:
		printf("(%s,%d) (%s,%d)\n",inetntoa(ih->saddr), ntohs(uh->sport), 
		       inetntoa(ih->daddr), ntohs(uh->dport));
		break;
	case IPPROTOTCP:
		printf("(%s,%d) (%s,%d)\n",inetntoa(ih->saddr), ntohs(th->sport), 
		       inetntoa(ih->daddr), ntohs(th->dport));
		break;
	}
}

void netdev_t::addhwhdr(pkt_t * pkt, u16_t proto, u8_t* dsthwaddr)
{
	ethhdr_t * eh;

	eh = (ethhdr_t*) pkt->addhead(sizeof(ethhdr_t));
	eh->proto = htons(proto);
	memcpy(eh->src, hwaddr, hwaddrlen);
	memcpy(eh->dst, dsthwaddr, hwaddrlen);
}

void netdev_t::addhwbcasthdr(pkt_t * pkt, u16_t proto)
{
	addhwhdr(pkt, proto, hwbcastaddr);
}

netdev_t::netdev_t() 
{
	next = prev = NULL;
	bzero(name, NAMELEN);

	hwtype = 0; 
	hwaddrlen = ETHALEN;
	bzero(hwaddr, hwaddrlen);
	memset(hwbcastaddr, 0xff, ETHALEN);

	prtype = ETHPIP;
	praddrlen = 4;
	praddr = 0;
	prbcastaddr = 0;
	netmask = 0;

	hwhdrlen = ETHHLEN;
	mtu = ETHMTU;
	metric = 0;
	flags = IFF_UP | IFF_RUNNING;
        bzero(&ethstat, sizeof(ethstat_t));
}

int netdev_t::output(pkt_t *pkt, u32_t daddr)
{
	if (daddr != praddr) {
		arpoutput(pkt, daddr, this);
		return 0;
	}
	ethhdr_t * eh = (ethhdr_t*) pkt->addhead(sizeof(ethhdr_t));
	eh->proto = htons(ETHPIP);
	input(pkt);
	return 0;
}

int netdev_t::hardwareoutput(pkt_t *pkt)
{
        return 0;
}

char * ethaddrtoa(uchar *ethaddr)
{
	static char buf[19*4];
	static int index;

	char *ret, *b;
	b = ret = buf + index *19;
	index = (index + 1) & 3;
	for (int i = 0; i < 6; i++) {
		sprintf(b, "%02x:", ethaddr[i]);
		b += 3;
	};
	*--b = 0;
	return ret;
}
