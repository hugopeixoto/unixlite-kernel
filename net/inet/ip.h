#ifndef	_INETIP_H
#define _INETIP_H

#include "inet.h"

struct ipstat_t { 
	ulong badver;           /* bad version number */
	ulong total;		/* total packets received */
	ulong badchksum;	/* checksum bad */
	ulong tooshort;		/* packet too short */
	ulong toosmall;		/* not enough data */
	ulong badhlen;		/* ip header length < data size */
	ulong badlen;		/* ip length < ip header length */
	ulong fragments;	/* fragments received */
	ulong fragdropped;	/* frags dropped (dups, out of space) */
	ulong fragtimeout;	/* fragments timed out */
	ulong forward;		/* packets forwarded */
	ulong cantforward;	/* packets rcvd for unreachable dest */
	ulong redirectsent;	/* packets forwarded on same net */
	ulong noproto;		/* unknown or unsupported protocol */
	ulong delivered;	/* packets consumed here */
	ulong localout;		/* total ip packets generated here */
	ulong odropped;		/* lost packets due to nobufs, etc. */
	ulong reassembled;	/* total packets reassembled ok */
	ulong fragmented;	/* output packets fragmented ok */
	ulong ofragments;	/* output fragments created */
	ulong cantfrag;		/* don't fragment flag was set, etc. */
};

struct pkt_t;
struct netdev_t;

extern ipstat_t ipstat;
extern void ipaddhead(pkt_t *pkt, u8_t proto, u32_t saddr, u32_t daddr);
extern void ipinput(pkt_t *pkt);
extern int ipoutput(pkt_t * pkt, u32_t daddr);
extern int ipoutput(pkt_t *pkt, u8_t proto, u32_t saddr, u32_t daddr, u16_t *chksum=NULL);
extern void ipforward(pkt_t *pkt);
extern pkt_t * ipreass(pkt_t * pkt); 
extern int selectroute(u32_t daddr, netdev_t **netdev, u32_t *nexthop);
//extern u32_t which(u32_t saddr, u32_t daddr);

#endif
