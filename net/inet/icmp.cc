#include "root.h"
#include "ip.h" 
#include "icmp.h"
#include <dev/net/eth.h>

/* the layout of icmp reply packet:
	char link[LINKHLEN];
	char iphdr[sizeof(iphdr_t)];
	char icmphdr[sizeof(icmphdr_t)];
	char old iphdr[sizeof(iphdr_t)];
	char old ipdata[8]; */
void icmpoutput(pkt_t *oldpkt, int type, int code)
{
	pkt_t * pkt = newpkt(AKERN, LINKIPHLEN, sizeof(icmphdr_t) + sizeof(iphdr_t) + 8);
	if (!pkt)
		return;
	icmphdr_t *ich = (icmphdr_t*) pkt->data;
	ich->type = type;
	ich->code = code;
	ich->chksum = 0;
	iphdr_t *oldih = (iphdr_t*) oldpkt->data;
	memcpy(pkt->data + sizeof(icmphdr_t), oldih, sizeof(iphdr_t) + 8);
	ich->chksum = ipchksum(ich, pkt->datalen);
	ipoutput(pkt, IPPROTOICMP, oldih->daddr, oldih->saddr);
}

/* we don't reuse the request pkt */
void icmpechorequest(pkt_t *oldpkt, iphdr_t *oldih, icmphdr_t *oldich)
{
	int ichtotlen = oldih->datalen(); /* include icmp header and data */
	pkt_t * pkt = newpkt(AUSER, LINKIPHLEN, ichtotlen);
	if (!pkt)
		return;
	memcpy(pkt->data, oldich, ichtotlen);
	icmphdr_t *ich = (icmphdr_t*) pkt->data;
	ich->type = ICTECHOREPLY;
	ich->code = 0;
	ich->chksum = 0;
	ich->chksum = ipchksum(ich, ichtotlen);
	ipoutput(pkt, IPPROTOICMP, oldih->daddr, oldih->saddr);
}

/* return > 0 if the packet is processed(and deleted)
   ip header is not strriped yet */
int icmpinput(pkt_t *pkt)
{
	iphdr_t * ih = (iphdr_t*) pkt->data;
	icmphdr_t * ich = (icmphdr_t*)(ih + 1);

	if (ipchksum(ich, ih->datalen())) {
		warn("icmp chksum failed!\n");
		delpkt(pkt);
		return 1;
	}
	switch (ich->type) {
		case ICTECHOREQUEST:
			icmpechorequest(pkt, ih, ich);
			delpkt(pkt);
			return 1;
		case ICTECHOREPLY:
			break;
		case ICTDSTUNREACH:
			break;
		case ICTSRCQUENCH:
		case ICTREDIRECT:
		case ICTTIMEEXCEEDED:
		case ICTPARAPROBLEM:
		case ICTTIMESTAMPREQUEST:
		case ICTTIMESTAMPREPLY:

		case ICTINFOREQUEST:
		case ICTINFOREPLY:

		case ICTADDRREQUEST:
		case ICTADDRREPLY:
			break;
		default:
			printd("icmpinput:unknown type\n");
	}
	return 0;
}
