#include <lib/root.h>
#include <lib/string.h>
#include <init/ctor.h>
#include <mm/allockm.h>
#include "pkt.h"

static pool_t * pktpool;
__ctor(PRINET, SUBANY, initpktpool)
{
	pktpool = new pool_t("pktpool", sizeof(pkt_t));
}

pkt_t * newpkt(int priority, int headlen, int datalen, int taillen)
{
	int roomlen = headlen + datalen + taillen;
	pkt_t * p = (pkt_t*) pktpool->alloc(priority);
	if (!p)
		return NULL;
	pkb_t * pkb = (pkb_t*) allockm(priority, sizeof(pkb_t) + roomlen);
	if (!pkb) {
		pktpool->free(p);
		return NULL;
	}
	p->next = p->prev = NULL;
	pkb->refcnt = 1;
	pkb->roomlen = roomlen;
	p->pkb = pkb;
	p->data = pkb->room + headlen;
	p->datalen = datalen;
	p->inputdev = NULL;
	p->magic = 0x7483721;
	return p;
}

void delpkt(pkt_t *p)
{
	assert(p->magic == 0x7483721);
	assert(p->pkb->refcnt > 0);
	if (!--p->pkb->refcnt)
		freekm(p->pkb);
	p->magic = 0x3721dead;
	pktpool->free(p);
}

/* clone() doesn't alloc a new sock buffer and copy the content 
   from the origianl sock buffer, it only increase the sock 
   buffer's refcnt by one */
pkt_t * pkt_t::clone(int priority)
{
	assert(pkb && pkb->refcnt);
	pkt_t * x = (pkt_t*) pktpool->alloc(priority); 
	if (!x)
		return NULL;
	x->next = x->prev = NULL;	
	x->data = data;
	x->datalen = datalen;
	x->pkb = pkb;
	x->pkb->refcnt++;
	x->inputdev = inputdev;
	x->startseq = startseq;
	x->endseq = endseq;
	x->sendtick = sendtick;
	x->magic = 0x7483721;
	return x;
}

int pktq_t::peek(void * buf, int len)
{
	int nread = 0;
        pkt_t * pkt;
        foreach (pkt, *this) {
		int once = min(len, pkt->datalen);
		memcpy(buf, pkt->data, once);
		nread += once;
		buf += once;
		len -= once;
		if (!len) break;
	} 
	return nread;
}

int pktq_t::fetch(void * buf, int len, int * freedroom)
{
	int nread = 0;
	if (freedroom) *freedroom = 0;
        pkt_t * pkt;

        foreachsafe (pkt, *this) {
		int once = min(len, pkt->datalen);
                pkt->delhead(buf, once);
		if (!pkt->datalen) {
			if (freedroom) *freedroom += pkt->roomlen();
			pkt->unlink();
			delpkt(pkt);
		}
		nread += once;
		buf += once;
		len -= once;
		if (!len) break;
	} 
	return nread;
}

void pktq_t::zapall()
{
	pkt_t * p;
	while (p = deqhead())
		delpkt(p);
}
