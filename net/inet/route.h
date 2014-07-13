#ifndef _ROUTE_TABLE_H
#define _ROUTE_TABLE_H

struct netdev_t;

struct route_t { /* route table entry */
	CHAIN(all, route_t);
	u32_t daddr;
	u32_t netmask;
	u32_t gateway;
        int metric;
	int refcnt; /* not used */
        int usecnt;
	int flags;
	netdev_t *netdev; /* net interface */
};
QUEUE(all, route_t);

extern int selectroute(u32_t daddr, netdev_t ** netdev, u32_t *nexthop);
extern int addlocalroute(netdev_t *netdev);
extern int addroute(int flags, u32_t daddr, u32_t netmask, u32_t gateway, netdev_t *netdev);
extern void ioctlroute(int cmd, void *arg);
extern int showroute(char *buf, int size);

#define RTFUP           0x0001 /* route usable */
#define RTFGATEWAY      0x0002 /* dst is gateway */
#define RTFHOST         0x0004 /* host entry(net otherwise) */

#endif
