#include "root.h" 
#include "tcp.h"
#include "ip.h"

#define INITTM(x, peroid) \
x##tm.init(peroid, (vfp_t)&tcpsock_t::x##to, this)
void tcpsock_t::inittimer()
{
	rtt = 4*_10ms;
	INITTM(rexmit, 5*_100ms);
	INITTM(delack, 2*_100ms);
	INITTM(keepalive, 0);
	INITTM(persist, 0);
	INITTM(timewait, 1*_1s);
	INITTM(destroy, 1*_1s);
}

void tcpsock_t::stoptimer()
{
	rexmittm.stop();
	delacktm.stop();
	keepalivetm.stop();
	persisttm.stop();
	timewaittm.stop();
	destroytm.stop();
}

/* NOTE: we only rexmit the first packet */
void tcpsock_t::rexmitto()
{
	debug(TCPDBG, "rexmit timeout rtt = %d(ticks) %d(ms)\n", rtt, rtt * 10);
//	((tcphdr_t*)(unackedq.head()->data))->dump();
	if (++nrexmit >= MAXREXMIT) {
		debug(TCPDBG, "rexmit too much!\n");
		return;
	}
	pkt_t *pkt = unackedq.head();
	if (!pkt)
		return;
	pkt_t *c = pkt->clone(AKERN);
	if (!c) {
		delpkt(pkt);
		return;
	}
	tcphdr_t * th = (tcphdr_t*) c->data;
	th->chksum = 0;
	rexmittm.restart(rto());
	ipoutput(c, IPPROTOTCP, laddr, faddr, &th->chksum);
}

void tcpsock_t::delackto()
{
	debug(TCPDBG, "delack timeout\n");
	sendflags(TCPFACK);
}

void tcpsock_t::keepaliveto()
{
	debug(TCPDBG, "keepalive timeout\n");
}

void tcpsock_t::persistto()
{
	debug(TCPDBG, "persit timeout\n");
}

void tcpsock_t::timewaitto()
{
	debug(TCPDBG, "timewait timeout\n");
	if (!refcnt)
		delete this;
}

void tcpsock_t::entertimewait()
{
	change(STIMEWAIT);
	rexmittm.stop();
	delacktm.stop();
	persisttm.stop();
	keepalivetm.stop();
	timewaittm.start();
}

/* NOTE: timewaitto/destroyto maybe called simutaneously */
void tcpsock_t::destroyto()
{
	debug(TCPDBG, "destroy timeout\n");
	if (!refcnt)
		delete this;
}
