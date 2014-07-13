#ifndef	_PACKET_H
#define _PACKET_H

#include <lib/queue.h>
#include <lib/string.h>
#include <mm/allockm.h>

struct pkt_t;
struct netdev_t;
struct pkb_t { /* packet buf */
	short refcnt;
	short roomlen;
	char room[0];
};

/*              addhead          addtail
             <------------    ------------>
             ------------>    <------------
                delhead          deltail 
   +---------------+----------------+---------------+ 
   |   head room   |      data      |   tail room   |  
   +---------------+----------------+---------------+   */
struct pkt_t {
	CHAIN(,pkt_t);
	char *data;
	int datalen;
	pkb_t *pkb;
	netdev_t *inputdev; /* which interface recv this packet */
	tick_t sendtick;
	u32_t startseq;
	u32_t endseq;
	ulong magic;
	int seqlen() { return (int)(u32_t)(endseq - startseq); }
	void movestartseqto(u32_t newstartseq);

	pkt_t * clone(int priority);
	int roomlen() { return pkb->roomlen; };
	char * room() { return pkb->room; };
	char * eroom() { return room() + roomlen(); }; /* end of room */
	char * edata() { return data + datalen; }; /* end of data */
	int headroomlen() { return data - room(); };
	int tailroomlen() { return eroom() - edata(); }; 
	void reserve(int headlen);
	void * addhead(int delta);
        void * addhead(void *buf, int len);
	void delhead(int delta);
	void delhead(void *buf, int len);
	void addtail(int delta);
	void addtail(void *buf, int len);
	void deltail(int delta);
	void deltail(void *buf, int len);
	void trimto(int len);
	void check() 
        {
#if DEBUG
                if (!data) 
                        panic("call pkt_t::reserve() first"); 
		assert(magic == 0x7483721);
                assert(room() <= data);
		assert(edata() <= eroom());
#endif
        }
};
QUEUE(,pkt_t);

extern pkt_t * newpkt(int priority, int headlen, int datalen, int taillen = 0);
extern void delpkt(pkt_t *p);

extern inline void * pkt_t::addhead(int delta)
{
	data -= delta;
	datalen += delta;
	check();
	return data;
}

extern inline void * pkt_t::addhead(void *buf, int len)
{
	memcpy(addhead(len), buf, len);
	return data;
}

extern inline void pkt_t::delhead(int delta)
{
	data += delta;
	datalen -= delta;
	check();
}

extern inline void pkt_t::delhead(void * buf, int delta)
{
	memcpy(buf, data, delta);
	delhead(delta);
}

extern inline void pkt_t::addtail(int delta)
{
	datalen += delta;
	check();
}

extern inline void pkt_t::addtail(void * buf, int delta)
{
	memcpy(data + datalen, buf, delta);
	datalen += delta;
	check();
}

extern inline void pkt_t::trimto(int len)
{
	datalen = len;
	check();
}

class pktq_t : public Q(pkt_t,) {
public:
	int peek(void * buf, int len);
	int fetch(void * buf, int len, int *freedroom = NULL);
	void zapall();
};

#define LINKHLEN (2 + 14)
#define LINKIPHLEN (LINKHLEN + 20)
#define LINKIPUDPHLEN (LINKIPHLEN + 8)
#define LINKIPICMPHLEN (LINKIPHLEN + 8)
#define LINKIPTCPHLEN (LINKIPHLEN + 20)

#endif
