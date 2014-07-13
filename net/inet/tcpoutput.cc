#include "root.h" 
#include "tcp.h"
#include "ip.h"

int tcpsock_t::resetpeer(u32_t seq, u32_t ack, u32_t saddr, u16_t sport, u32_t daddr, u16_t dport)
{
	pkt_t * p = newpkt(AUSER, LINKIPHLEN, sizeof(tcphdr_t));
	if (!p)
		return ENOMEM;
	tcphdr_t * th = (tcphdr_t*) p->data;
	th->sport = sport;
	th->dport = dport;
	th->seq = seq;
	th->ack = ack;
	th->setheadlen(sizeof(tcphdr_t));
	th->flags = TCPFPSH | TCPFRST;
	th->wnd = 0;
	th->chksum = 0;
	th->urgptr = 0;
	return ipoutput(p, IPPROTOTCP, saddr, daddr, &th->chksum);
}

void tcpsock_t::abort()
{
	if (state != SESTAB)
		return;
	stoptimer();
	change(SCLOSED);
}

int tcpsock_t::output(int priority, u8_t flags, void *data, 
		      int datalen, void *opt, int optlen)
{
	/* build a packet */
	pkt_t *pkt;
	int tcpheadlen = sizeof(tcphdr_t) + optlen;
	int e = allocsndbuf(&pkt, priority, LINKIPHLEN, tcpheadlen + datalen);
	if (e)
		return e;
	tcphdr_t *th = (tcphdr_t *) pkt->data;
	th->sport = lport;
	th->dport = fport;
	th->seq = htonl(sndnxt);
	if (flags & TCPFACK) {
		th->ack = htonl(rcvnxt);
		ndelack = 0;
		delacktm.stop();
	} else
		th->ack = htonl(0);
	th->setheadlen(tcpheadlen);
	th->flags = flags;
	th->wnd = htons(rcvwnd());
	th->chksum = 0;
	th->urgptr = 0;
	memcpy(th->opt, opt, optlen); /* works even if opt == NULL */
	memcpy(th->data(), data, datalen); /* works even if data == NULL */

	/* try to output */
	pkt->sendtick = passedtick;
	pkt->startseq = sndnxt;
	pkt->endseq = seqadd(sndnxt, datalen + th->synfinbyte());
	sndnxt += pkt->seqlen();
	sndwnd -= datalen;
	while (sndwnd < mss && priority != AKERN) {
		debug(TCPDBG, "sending blocked: sndwnd(%d) < mss(%d)\n", sndwnd, mss);
		WAIT(waitq); 
	}

	debug(NETFLOWDBG, "tcp output: sndwnd=%d [%s %d] --> [%s %d]\n", sndwnd, 
		inetntoa(laddr), ntohs(th->sport),
	     	inetntoa(faddr), ntohs(th->dport));

	/* pure ack packet will not be backed */
	if ((pkt->startseq == pkt->endseq) && !(flags & (TCPFSYN | TCPFFIN)))
		return ipoutput(pkt, IPPROTOTCP, laddr, faddr, &th->chksum);

	/* kick the packet's clone out of socket */
	unackedq.enqtail(pkt);
	pkt_t * c = pkt->clone(priority);
	if (!c) {
		delpkt(pkt);
		return ENOMEM;
	}
	th = (tcphdr_t*) c->data;
	if (!rexmittm.active())
		rexmittm.start(rto());
	return ipoutput(c, IPPROTOTCP, laddr, faddr, &th->chksum);
}

int tcpsock_t::sendflags(u8_t flags)
{
	return output(AKERN, flags | TCPFPSH);
}

/* Currently defined options include (kind indicated in octal):
      Kind     Length    Meaning
      ----     ------    -------
       0         -       End of option list.
       1         -       No-Operation.
       2         4       Maximum Segment Size. */
void tcpsock_t::parseopt(tcphdr_t *th)
{
	tcpopt_t * end = (tcpopt_t*) th->data();
	mss = 536; /* default MSS */
	for (tcpopt_t * o = th->opt; o < end; o = o->next()) {
		if (o->kind == 0)
			return;
		if (o->kind == 1)
			continue;
		if (o->kind == 2) {
			if (o->len != 4)
				continue;
			mss = ntohs(((tcpmssopt_t*)o)->mss);
		}
	}
}

int tcpsock_t::sendflagsmss(u8_t flags)
{
	tcpmssopt_t opt;
	opt.kind = 2;
	opt.len = 4;
	opt.mss = htons(ETHMTU - sizeof(iphdr_t) - sizeof(tcphdr_t));
	return output(AKERN, flags | TCPFPSH, NULL, 0, &opt, sizeof(opt));
}
