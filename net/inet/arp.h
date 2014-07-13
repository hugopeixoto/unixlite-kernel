#ifndef	_NETINETARP_H
#define _NETINETARP_H

struct pkt_t;
struct netdev_t;
extern void arpinput(pkt_t *pkt);
extern void arpoutput(pkt_t *pkt, u32_t praddr, netdev_t *dev);
#endif
