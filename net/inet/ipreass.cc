#include "root.h" 
#include <kern/timer.h>
#include <net/inet/icmp.h>

struct ipfragq_t : public pktq_t { /* fragment queue */
	CHAIN(hash, ipfragq_t);
        timer_t timer;
        u32_t id;
        bool lastfragarrived;

	pkt_t * arrive(pkt_t * pkt);
	pkt_t * glue();
	int complete();
};
QUEUE(hash, ipfragq_t);

typedef Q(hash, ipfragq_t) hashq_t; 
#define HSIZE 16
#define HMASK (HSIZE - 1)
static hashq_t hashtab[HSIZE];
static inline hashq_t * hashfunc(u32_t id)
{
	return hashtab + (id & HMASK); 
}

/**
 * When a new packet arrive, this packet will be inserted into a sorted 
 * list according to the fragment offset, this function use the following 
 * fact to accerate the insert operation :
 * 
 * Assmuse sender will send eight packet 1-2-3-4-5-6-7-8, these eight 
 * packets may reach in an orderless manner, the receive sequence maybe 
 * 2-1-3-4-5-6-8-7. however, it is nearly impossible that receiver receive
 * 8-7-6-5-4-3-2-1.
 * 
 * If new arrived packet is overlaped with old packet, old will be overrided
 */ 
pkt_t * ipfragq_t::arrive(pkt_t *pkt)
{
        iphdr_t * ih = (iphdr_t*) pkt->data;
        id = ih->id;
        return pkt;
}

/* reassemble ip packet */
pkt_t * ipreass(pkt_t* pkt)
{
	iphdr_t* ih = (iphdr_t*) pkt->data;
        hashq_t * hashq = hashfunc(ih->id);
        ipfragq_t * ifq;
        foreach (ifq, *hashq) {
                if (ifq->id == ih->id)
                        return ifq->arrive(pkt);
        }
	ifq = new ipfragq_t();
        hashq->enqtail(ifq);
	return ifq->arrive(pkt);
}
