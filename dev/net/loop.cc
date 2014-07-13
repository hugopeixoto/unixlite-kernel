#include <lib/root.h>
#include <lib/string.h>
#include <net/inet/ip.h>
#include <net/lib/pkt.h>
#include "loop.h"

loopnetdev_t *loopnetdev;

loopnetdev_t::loopnetdev_t()
{
	strcpy(name, "lo");
	/* hardware address make no sense for loop device */
	hwhdrlen = 0;
	mtu = ETHMTU;
	praddr = mkipaddr(127, 0, 0, 1);
	netmask = mkipaddr(255, 0, 0, 0);
}

loopnetdev_t::~loopnetdev_t()
{
}

int loopnetdev_t::output(pkt_t* pkt, u32_t daddr)
{
	ethhdr_t * eh = (ethhdr_t*) pkt->addhead(sizeof(ethhdr_t));
	eh->proto = htons(ETHPIP);
	input(pkt);
        return 0;
}
