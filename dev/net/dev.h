#ifndef	_NETDEV_H
#define _NETDEV_H

#include <lib/queue.h>
#include "eth.h" 
#include <asm/io.h>
#include <asm/system.h>
#include <asm/irq.h>

struct pkt_t;
/* Structure defining a queue for a network interface. */
struct netdev_t { 
        CHAIN(,netdev_t);
        enum { NAMELEN = 16 };
	char name[NAMELEN];  /* name, e.g. ``en'' or ``lo'' */
	u8_t hwtype;
	u8_t hwaddrlen;  /* hardware address len */
	u8_t hwaddr[6];  /* hardware address */
	u8_t hwbcastaddr[6]; /* hardware broadcast address */
	u8_t prtype;
	u8_t praddrlen;  /* proto address len */
	u32_t praddr;    /* proto address */
	u32_t prbcastaddr;  /* proto broadcast address */
	u32_t netmask;
	int hwhdrlen;   /* hardward header len */
	short mtu;	/* maximum transmission unit    */
	short flags;	/* up/down, broadcast, etc.     */
	int metric;
        ethstat_t ethstat;

	netdev_t();

	void dump(pkt_t *pkt);
	void addhwhdr(pkt_t *pkt, u16_t proto, u8_t *hwaddr);
	void addhwbcasthdr(pkt_t *pkt, u16_t proto);
	void input(pkt_t *pkt);
        virtual int output(pkt_t *pkt, u32_t daddr);
        virtual int hardwareoutput(pkt_t *pkt);
};
QUEUE(,netdev_t);
typedef Q(,netdev_t) netdevq_t;
extern netdevq_t netdevq;

extern softirq_t *netsoftirq;
extern bool tothishost(u32_t daddr);
extern void addnetdev(netdev_t *netdev);
extern netdev_t* findnetdev(char *name);
#endif
