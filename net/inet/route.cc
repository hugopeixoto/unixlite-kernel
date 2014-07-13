#include "root.h" 
#include <lib/queue.h>
#include <lib/errno.h>
#include <lib/ostream.h>
#include <init/ctor.h>
#include <dev/net/loop.h>
#include <mm/mm.h>
#include "route.h"

static Q(all, route_t) allq; 
QCTOR(allq);
static route_t * defaultgw; /* default gateway */

/* this procdure is called after initnetdev */
__ctor(PRINET, SUB2ND, initroute)
{
}

static route_t *lookup(u32_t daddr)
{
	route_t * r;
	foreach (r, allq) {
		if (ipneteq(r->daddr, daddr, r->netmask))
			return r;
	}
	return defaultgw;
}

/* gw may equal daddr */
int selectroute(u32_t daddr, netdev_t **netdev, u32_t *nexthop)
{
        route_t *r = lookup(daddr);
        if (!r)
                return ENETUNREACH;
        *nexthop = (r->flags & RTFGATEWAY) ? r->gateway : daddr;
	*netdev = r->netdev;
	return 0; 
}

/* gateway must in the local network */
static netdev_t * findgateway(u32_t gateway)
{
        route_t * r;
        foreach (r, allq) {
                if (ipneteq(gateway, r->daddr, r->netmask))
                        return r->netdev;
        }
        return NULL;
}

void delroute(u32_t daddr)
{
        route_t * r = lookup(daddr);
	if (!r)
		return;
	if (r == defaultgw)
		defaultgw = NULL;
	r->unlinkall();
	delete r;
}

/**
 * local network: flags=0
 * remote network: flags=RTFGATEWAY
 * host on the remote network: flags=RTFHOST|RTFGATEWAY 
 * default gateway: flags=RTFGATEWAY daddr == INADDRANY 
 */
int addroute(int flags, u32_t daddr, u32_t netmask, u32_t gateway, netdev_t *netdev)
{
        if (flags & RTFHOST)
                netmask = 0xffffffff;
        if (flags & RTFGATEWAY) {
                netdev = findgateway(gateway);
                if (!netdev)
                        return ENETUNREACH;
        }
        delroute(daddr); /* remove old duplicate */
        route_t * r = new route_t();
	r->nextall = r->prevall = NULL;
        r->flags = RTFUP | flags;
        r->daddr = daddr;
        r->netmask = netmask;
        r->gateway = gateway;
        r->netdev = netdev;
        r->metric = 0;
        r->refcnt = 0;
        r->usecnt = 0;
	if (r->daddr == INADDRANY)
		r->flags |= RTFGATEWAY, defaultgw = r;
        allq.enqtail(r);
	return 0;
}

int addlocalroute(netdev_t * netdev)
{
        return addroute(0, netdev->praddr, netdev->netmask, 0, netdev);
}

int addremoteroute(u32_t daddr, u32_t netmask, u32_t gw)
{
	return addroute(RTFGATEWAY, daddr, netmask, gw, NULL);
}

static int adddefaultgw(u32_t gw)
{
	return addroute(RTFGATEWAY, INADDRANY, 0, gw, NULL);
}

int showroute(char *buf, int size)
{
	int e;

	if (e = verw(buf, size))
		return e;
	ostream_t os(buf, size);
	os.write("Kernel IP routing table\n");
	os.write("Destination\tGateway\t  Genmask\tFlags\tMetric\tRef\tUse\tIface\n");
	route_t * r;
	foreach (r, allq) {
		os.write("%s\t%s\t  %s\t%x\t%d\t%d\t%d\t%s\n", 
			 inetntoa(r->daddr), inetntoa(r->gateway), inetntoa(r->netmask),
			 r->flags, r->metric, r->refcnt, r->usecnt, 
			 r->netdev ? r->netdev->name : "*");
	}
	return os.written();
}

#if 0
void addroute(struct route *r)
{
        if (r->family != AFINET)
                return EAFNOTSUPPORT;
        addroute(r->flags, r->daddr, r->netmask, r->gateway, r->netdev);
}

void ioctlroute(int cmd, void *arg)
{
        switch (cmd) {
                case SIOCADDRT:
                        if (!suser())
                                return EPERM;
                        addroute((route_t*)arg);
                        return 0;
                case SIOCDELRT:
                        if (!suser())
                                return EPERM;
                        delroute((route_t*)arg);
                        return 0;
                default:
                        return EINVAL;
        }
}
#endif
