#include "root.h"
#include "tcp.h"

/* NOTE: asssume the packet donesn't contain the SYN/FIN byte */
void pkt_t::movestartseqto(u32_t newstartseq)
{
	if (tcpsock_t::seqlt(startseq, newstartseq))
		delhead(tcpsock_t::seqlen(startseq, newstartseq));
	else
		addhead(tcpsock_t::seqlen(newstartseq, startseq));
	startseq = newstartseq;
}

void tcpsock_t::inputclosed(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th)
{
	delpkt(pkt);
}

void tcpsock_t::inputlisten(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th)
{
	if (!(th->flags & TCPFSYN)) {
		debug(TCPDBG, "listening socket receive packet without SYN set: %s %s\n", 
		       inetntoa(ih->saddr), inetntoa(ih->daddr));
		resetpeer(ih, th);
		delpkt(pkt);
		return;
	}
        tcpsock_t * c = new tcpsock_t(this, ih, th);
        c->sendflagsmss(TCPFSYN | TCPFACK);
	c->change(SSYNRECV);
	delpkt(pkt);
}

void tcpsock_t::inputsynsent(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th)
{
	if (!(th->flags & TCPFSYN) || !th->ackeq(sndnxt)) {
		printd("inputsynsent:flags=%x, seq=%u, sndnxt=%u\n", th->flags, ntohs(th->seq), sndnxt);
		delpkt(pkt);
		return;
	}
	initirs(th);
        sendflags(TCPFACK);
	change(SESTAB);
	waitq.broadcast();
	delpkt(pkt);
}

void tcpsock_t::inputsynrecv(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th)
{
	if (!th->ackeq(sndnxt)) {
		printd("inputsynrecv:invalid packet received\n");
		delpkt(pkt);
		return;
	}
	change(SESTAB);
	listening->waitq.broadcast(); /* listening socket will be unblocked from accept */
	delpkt(pkt);
}

/* NOTE: the packet may be linked into socket's receive packet queue or be deleted */
void tcpsock_t::inputdata(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th)
{
	if (++ndelack == MAXDELACK)
		sendflags(TCPFACK);

	/* NOTE: fin byte maybe arrived out of sequence */
	if (th->flags & TCPFFIN) {
		if (finrecved) {
			debug(TCPDBG, "duplicate fin received\n");
			delpkt(pkt);
			return;
		}
		finrecved = true;
		waitq.broadcast(); /* notify the task blocked in tcpsock_t::recv */
		if (seqadd(pkt->startseq, 1) == pkt->endseq) {
			rcvnxt = seqadd(rcvnxt, 1);
			delpkt(pkt);
			return;
		}
		printd("fin is pigged %u %u\n", pkt->startseq, pkt->endseq);
		seqsub(pkt->endseq, 1);
	}
	if (seqlt(pkt->endseq, rcvnxt)) {
		printd("inputdata:%u %u\n", pkt->endseq, rcvnxt);
		delpkt(pkt);
		return;
	}
	if (seqlt(pkt->startseq, rcvnxt))
		pkt->movestartseqto(rcvnxt);

	if (pkt->startseq == pkt->endseq)
		return;
	if (allocrcvbuf(pkt) < 0) {
		warn("tcpsock drop the packet due to lack of recv buffer\n");
		delpkt(pkt);
		return;
	}
	if (rcvnxt == pkt->startseq) { /* the most-likely case */
		recvq.enqtail(pkt);
		rcvnxt = pkt->endseq;
		pkt_t *p;
		foreachsafe(p, oosrecvq) {
			if (rcvnxt == p->startseq) {
				oosrecvq.unlink(p);
				recvq.enqtail(p);
				rcvnxt = pkt->endseq;
			} else
				break;
		}
	} else { /* insert to the out of sequence receive queue */
		debug(TCPDBG, "receive out of sequence packet!\n");
		pkt_t *p, *next = NULL, *prev = NULL;
		foreach (p, oosrecvq) {
			if (seqlt(pkt->endseq, p->startseq)) {
				next = p;
				break;
			}
			prev = p;
		}
		/* insert the pkt between next and prev */
		if (prev) {
			if (seqlt(pkt->startseq, prev->endseq))
				pkt->movestartseqto(prev->endseq);
		} else if (next) {
			pkt->prepend(next);
		} else 
			oosrecvq.enqtail(pkt);
	}
	waitq.broadcast();
	check();
}

/* Because inputd packet maybe fragmented on the way, so this function must
   assume what received is a list of fragment. */ 
void tcpsock_t::inputestab(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th)
{
	inputdata(pkt, ih, th);
	if (finrecved)
		change(SCLOSEWAIT);
}

void tcpsock_t::inputfinwait1(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th)
{
	bool ackfin = th->ackeq(sndnxt); /* pkt maybe deleted after inputdata */
	inputdata(pkt, ih, th);
	if (finrecved && ackfin) {
		sendflags(TCPFACK);
		entertimewait();
	} else if (finrecved) {
		sendflags(TCPFACK);
		change(SCLOSING);
	} else if (ackfin) {
		change(SFINWAIT2);
	}
}

void tcpsock_t::inputfinwait2(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th)
{
	inputdata(pkt, ih, th);
	if (finrecved) {
		sendflags(TCPFACK);
		entertimewait();
	}
}

void tcpsock_t::inputtimewait(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th)
{
	delpkt(pkt);
}

void tcpsock_t::inputclosewait(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th)
{
	inputdata(pkt, ih, th);
	if (finrecved)
		sendflags(TCPFACK);
}

void tcpsock_t::inputlastack(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th)
{
	if (th->ackeq(sndnxt)) {
		sendflags(TCPFACK);
		change(SCLOSED);
	} else
		debug(TCPDBG, "ACK=%d ack=%u sndnxt=%u\n", th->flags & TCPFACK, ntohl(th->ack), sndnxt);
	delpkt(pkt);
}

void tcpsock_t::inputclosing(pkt_t *pkt, iphdr_t *ih, tcphdr_t *th)
{
	bool ackfin = (th->flags & TCPFACK) && (ntohl(th->ack) == sndnxt);
	inputdata(pkt, ih, th);
	if (ackfin) {
		sendflags(TCPFACK);
		entertimewait();
	} else
		debug(TCPDBG, "ACK=%d ack=%u sndnxt=%u\n", th->flags & TCPFACK, ntohl(th->ack), sndnxt);
}

void tcpsock_t::updatertt(tick_t newrtt)
{
	rtt = (rtt + newrtt) / 2; 
	rtt = minmax(MINRTT, rtt, MAXRTT);
}

/* for the simplcity of the alogrithm:
   1) if the retransmit timer is over, we only retransmit the first packet 
   2) we ack the whole packet  */
void tcpsock_t::ackarrived(u32_t ack)
{
	pkt_t * pkt;
	int npkt = 0;

	foreachsafe (pkt, unackedq) {
		if (ack < pkt->endseq)
			break;
		assert(passedtick >=pkt->sendtick);
		updatertt(passedtick - pkt->sendtick);
		unackedq.unlink(pkt);
		freesndbuf(pkt);
		npkt++;
	}
	if (!npkt)
		return;
	nrexmit = 0;
	rexmittm.stop();
	if (!unackedq.empty())
		rexmittm.start(rto());
}

/* ip header and tcp header has been stripped */
void tcpsock_t::input(pkt_t * pkt, iphdr_t * ih, tcphdr_t * th)
{
	debug(NETFLOWDBG, "tcpinput: wnd=%d [%s %d] --> [%s %d]\n", ntohs(th->wnd), 
		inetntoa(ih->saddr), ntohs(th->sport),
	     	inetntoa(ih->daddr), ntohs(th->dport));
	pkt->startseq = ntohl(th->seq);
	int tcpdatalen = (char*)ih + ntohs(ih->totlen) - (char*)&th->opt[0];
	pkt->endseq = seqadd(pkt->startseq, tcpdatalen + th->synfinbyte());
	if (seqgt(pkt->startseq, pkt->endseq)) {
		printd("invalid sequence start=%u end=%u\n", pkt->startseq, pkt->endseq);
		delpkt(pkt);
		return;
	}

	if (th->flags & TCPFRST) {
		abort();
		return;
	}
	if (state >= SESTAB)
		updatesndwnd(th);
	if (th->flags & TCPFACK)
		ackarrived(ntohl(th->ack));

#define CASE(state, action) case state: input##action(pkt, ih, th); break;
	switch (state) {
		CASE(SCLOSED, closed);
		CASE(SLISTEN, listen);
		CASE(SSYNSENT, synsent);
		CASE(SSYNRECV, synrecv);
		CASE(SESTAB, estab);
		CASE(SCLOSING, closing);
		CASE(SFINWAIT1, finwait1);
		CASE(SFINWAIT2, finwait2);
		CASE(STIMEWAIT, timewait);
		CASE(SCLOSEWAIT, closewait);
		CASE(SLASTACK, lastack);
	}
}
