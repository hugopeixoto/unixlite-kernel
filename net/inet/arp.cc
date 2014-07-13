#include "root.h" 
#include <lib/queue.h> 
#include <lib/string.h>
#include <init/ctor.h>
#include <net/lib/sock.h>
#include <net/inet/inet.h>
#include <dev/net/eth.h>
#include <kern/timer.h>
#include <mm/allockm.h>
#include "arp.h"

/* ARP Flag values. */
#define	ATFINUSE	0x01		/* entry in use			*/
#define ATFCOM		0x02		/* completed entry (ha valid)	*/
#define	ATFPERM	        0x04		/* permanent entry		*/
#define	ATFPUBL	        0x08		/* publish entry		*/
#define	ATFUSETRAILERS	0x10		/* has requested trailers	*/

/* ARP ioctl request. */
struct arpreq_t {
	sockaddr_t praddr; /* protocol addr */
	sockaddr_t hwaddr; /* hardware addr */
	int flags;
};

enum {  AEINVAL, /* entry contain invalid content */
        AEWAITING, /* entry is waiting for reply arrive */
        AERESOLVED, /* entry has been resolved */
};

enum { ARPOPREQUEST = 1,
       ARPOPREPLY = 2,
       /* hardware type */
       ARPHWNETROM = 0,
       ARPHWETHER = 1,  
       ARPHWEXPETHER = 2, /* experimental ethernet */
       ARPHWAX25 = 3,
};

struct arphdr_t {
        u16_t hwtype;
        u16_t prtype;
        u8_t hwaddrlen;
        u8_t praddrlen;
        u16_t opcode;
        u8_t end[0];

        /* sender hw addr */
        u8_t * sha() { return end; } 
        /* sender pr addr */
        u8_t * spa() { return end + hwaddrlen; } 
        /* target hw addr */
        u8_t * tha() { return end + hwaddrlen + praddrlen; } 
        /* target pr addr */
        u8_t * tpa() { return end + hwaddrlen + praddrlen + hwaddrlen; } 
} __attribute__((packed));

#define SETPR(obj, a, b, c) do { obj->prtype = a; obj->praddrlen = b; \
obj->praddr = c; } while (0)

#define SETHW(obj, a, b, c) do { obj->hwtype = a; obj->hwaddrlen = b; \
memcpy(obj->hwaddr, c, b); } while (0)

struct arpent_t {
	CHAIN(hash, arpent_t);
	CHAIN(lru, arpent_t);
	int stat;
	int refcnt;
	u16_t hwtype;		/* type of hardware address	*/
	u16_t prtype;		/* type of protocol address	*/
	u8_t hwaddrlen;		/* length of hardware address	*/
	u8_t praddrlen;		/* length of protocol address	*/
	u8_t hwaddr[ETHHLEN];
	u32_t praddr;		/* network byte order!!! */
	netdev_t *netdev;
	pktq_t waitreplyq;
	timer_t rexmittm;
	int nrexmit; /* number of rexmit */

	arpent_t();
	void rexmitto();
	void invalidate(); 
	void replyarrive(netdev_t *netdev);
	void dump();
};
QUEUE(hash, arpent_t);
QUEUE(lru, arpent_t);

/*1) in the lru list, head element is least-recently-used, 
     tail element is most-recently-used.
  2) entry is always in the lru list even if its stat is INVALID.
  3) entry is always in the hash list even if its stat is INVALID, so  
     lookup() must skip invalid entry. */
#define ARPTABSIZE 32
#define HSIZE (ARPTABSIZE - 1) 
#define HMASK (HSIZE - 1)
typedef Q(hash,arpent_t) hashq_t; 
static hashq_t hashtab[HSIZE];
static Q(lru,arpent_t) lruq;
static arpent_t arptab[ARPTABSIZE];

static inline hashq_t * hashfunc(u32_t praddr)
{
	return hashtab + (ntohl(praddr) & HMASK); 
}

arpent_t::arpent_t()
{
	static uchar index;
	stat = AEINVAL;
	refcnt = 0;

	SETPR(this, ETHPIP, 4, mkipaddr(192, 168, 0, index++));
	hwtype = ARPHWETHER;
	hwaddrlen = 6;
	memset(hwaddr, 0, hwaddrlen);

	rexmittm.init(_1s, (vfp_t)&arpent_t::rexmitto, this);
	nrexmit = 0;

	nextlru = prevlru = NULL;
	lruq.enqtail(this);
	nexthash = prevhash = NULL;
	hashfunc(praddr)->enqtail(this);
}

__ctor(PRINET, SUBANY, initarptab)
{
        construct(hashtab, HSIZE);
        construct(&lruq);
	construct(arptab, ARPTABSIZE); /* this must be done last */
}

void arpent_t::dump()
{
	printf("ipaddr:%s hwaddr:%s\n", inetntoa(praddr), ethaddrtoa(hwaddr));
}

void arpent_t::invalidate()
{
	if (stat == AEINVAL)
		return;
	stat = AEINVAL;
	nrexmit = 0;
	rexmittm.stop();
	pkt_t * pkt;
	while (pkt = waitreplyq.deqhead())
		delpkt(pkt);
	netdev = NULL;
}

void arprequest(u16_t prtype, u8_t praddrlen, u32_t praddr, netdev_t * dev);
enum { MAXREXMIT = 3 };
void arpent_t::rexmitto()
{
	if (nrexmit++ >= MAXREXMIT) {
		invalidate();
		return;
	}
	arprequest(prtype, praddrlen, praddr, netdev);
}

/* send all the packets queued in the waitreplyq */
void arpent_t::replyarrive(netdev_t *netdev)
{
	pkt_t * pkt;
	debug(ARPDBG, "arp-reply arrived\n");
	while (pkt = waitreplyq.deqhead()) {
		netdev->addhwhdr(pkt, ETHPIP, hwaddr);
		netdev->hardwareoutput(pkt);
	}
}

/* invalid entry will be ignored */
static arpent_t * lookup(u16_t prtype, u8_t praddrlen, u32_t praddr)
{
	hashq_t * hashq = hashfunc(praddr);
	arpent_t * ae;
        foreach (ae, *hashq) {
		if ((ae->stat != AEINVAL) && (ae->prtype == prtype) && 
                    (ae->praddrlen == praddrlen) && (ae->praddr == praddr))
			return ae;
        }
       	return NULL;
}

static arpent_t * get(u16_t prtype, u8_t praddrlen, u32_t praddr)
{
	arpent_t * ae = lookup(prtype, praddrlen, praddr);
	if (ae) {
		lruq.unlink(ae);
                lruq.enqtail(ae);
		return ae;
	}

	ae = lruq.deqhead();
        lruq.enqtail(ae);
	ae->unlinkhash();
	hashfunc(praddr)->enqhead(ae);
	ae->invalidate();
	SETPR(ae, prtype, praddrlen, praddr);
	return ae;
}

/* broadcast an arp request packet */ 
void arprequest(u16_t prtype, u8_t praddrlen, u32_t praddr, netdev_t * dev)
{
	int totlen = sizeof(arphdr_t) + (6 + 4) * 2;
	pkt_t *pkt = newpkt(AKERN, ETHPAD + ETHHLEN, totlen);
	arphdr_t * ah = (arphdr_t*) pkt->data; 

	ah->opcode = htons(ARPOPREQUEST);
	ah->hwtype = htons(ARPHWETHER);
	ah->prtype = htons(prtype);
	ah->hwaddrlen = dev->hwaddrlen;
	ah->praddrlen = dev->praddrlen;

	memcpy(ah->sha(), dev->hwaddr, dev->hwaddrlen);
	memcpy(ah->spa(), (u8_t*) &dev->praddr, dev->praddrlen);
	memset(ah->tha(), 0, dev->hwaddrlen);
	memcpy(ah->tpa(), (u8_t*) &praddr, praddrlen);

	dev->addhwbcasthdr(pkt, ETHPARP);
	dev->hardwareoutput(pkt);
}

/* output the IP packet */
void arpoutput(pkt_t *pkt, u32_t praddr, netdev_t *dev)
{
	u16_t prtype = ETHPIP;
	u8_t praddrlen = 4;
	arpent_t *ae = get(prtype, praddrlen, praddr);

	if (ae->stat == AEINVAL) {
		debug(ARPDBG, "send arp-request, waiting for arp-reply\n");
		ae->waitreplyq.enqtail(pkt);
		ae->stat = AEWAITING;
		SETPR(ae, prtype, praddrlen, praddr);
		ae->netdev = dev;
		ae->rexmittm.start();
		arprequest(prtype, praddrlen, praddr, dev);
		return;
	}
	if (ae->stat == AEWAITING) {
		debug(ARPDBG, "join the waiting for arp-reply queue\n");
		ae->waitreplyq.enqtail(pkt);
		return;
	}
	if (ae->stat == AERESOLVED) {
		debug(ARPDBG, "arp has been resovled\n");
		dev->addhwhdr(pkt, ETHPIP, ae->hwaddr);
		dev->hardwareoutput(pkt);
		return;
	}
	panic("arp entry's stat is invalid\n");
}

/* output the delayed IP packet */
void arpreply(pkt_t *pkt)
{
	netdev_t * dev = pkt->inputdev;
	arphdr_t * ah = (arphdr_t*) pkt->data;

	/* don't modify old packet's hwaddrlen hwtye praddrlen prtype */
	ah->opcode = htons(ARPOPREPLY);
	memcpy(ah->tha(), ah->sha(), ah->hwaddrlen);
	memcpy(ah->tpa(), ah->spa(), ah->praddrlen);
	memcpy(ah->sha(), dev->hwaddr, dev->hwaddrlen);
	memcpy(ah->spa(), (u8_t*) &dev->praddr, dev->praddrlen);

	dev->addhwhdr(pkt, ETHPARP, ah->tha());
	dev->hardwareoutput(pkt);
}

/* pkt's ethernet header has been striped */
void arpinput(pkt_t * pkt)
{
	arphdr_t * ah = (arphdr_t*) pkt->data;
	u32_t spa = *cast(u32_t*, ah->spa());
	u32_t tpa = *cast(u32_t*, ah->tpa());
	arpent_t * ae = get(htons(ah->prtype), ah->praddrlen, spa);

	if ((ae->praddrlen != 4) || (ae->hwaddrlen != 6)) {
		printd("arpinput: receive packet with wrong field\n");
		delpkt(pkt);
		return;
	}
	ae->stat = AERESOLVED;
	SETPR(ae, ETHPIP, 4, spa);
	SETHW(ae, ARPHWETHER, 6, ah->sha());
	ae->nrexmit = 0;
	ae->rexmittm.stop();

	u16_t opcode = ntohs(ah->opcode);
	if (opcode == ARPOPREQUEST) {
		if (!tothishost(tpa)) { /* discard this broadcast packet */
			delpkt(pkt); 
			return;
		}
		arpreply(pkt); /* reuse this packet */
		return;
	} else if (opcode == ARPOPREPLY) {
		ae->replyarrive(pkt->inputdev);
	} else
		printf("unknown arp opcode\n");
	delpkt(pkt);
}
