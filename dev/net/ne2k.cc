#include <lib/root.h>
#include <lib/printf.h>
#include <net/lib/pkt.h>
#include <asm/irq.h>
#include <dev/pci/bios.h>
#include <init/ctor.h>
#include <net/inet/inet.h>
#include "ne2k.h"
#include "eth.h"

/* pcifindclass(PCI_CLASS_NETWORK_ETHERNET, index, &bus, &dev) donesn't work in qemu */
__ctor(PRINET, SUBANY, probene2k)
{
	u8_t bus, dev;
	int index = 0;
	if (pcifinddev(PCI_VENDOR_ID_REALTEK, 0x8029, index, &bus, &dev) == BIOS_SUCC) {
		addnetdev(new ne2kdev_t(bus, dev));
		index++;
#warning "ne2k probe"
	}
}

#include <net/inet/route.h>
ne2kdev_t::ne2kdev_t(u8_t bus, u8_t dev)
{
#if 0
static int count;
if (count == 0) {
praddr = inetaton("172.20.0.2");
netmask = inetaton("255.255.0.0");
} else {
praddr = inetaton("192.168.0.2");
netmask = inetaton("255.255.255.0");
}
count++;
addlocalroute(this);
#endif
	pcirdcfgb(bus, dev, PCI_INTERRUPT_LINE, &irqno);
	pcirdcfgd(bus, dev, PCI_BASE_ADDRESS_0, &iobase);
	iobase &= PCI_BASE_ADDRESS_IO_MASK;
	printf("ne2k.cc: PCI BIOS report RealTek-8029 at IO=%#x IRQ=%d\n", iobase, irqno);
	allocirq(&ne2kdev_t::isr, this, irqno);

	txing = 0;
        txstartpg = 16 * 1024 / NEPG;
        txendpg = txstartpg + (ETHMTU + NEPG) / NEPG; 
        rxstartpg = txendpg;
        rxendpg = 32 * 1024 / NEPG;
	rxpgnum = rxendpg - rxstartpg;

	/* stop */
        command(CRPG0 | CRNODMA | CRSTP);
	neout8(0x3f, PG0IMR); /* enable INT when bit is on */
	neout8(0xff, PG0ISR); /* ack the INT */

	/* config */
	neout8(0, PG0TCR);
	neout8(RCRAB, PG0RCR);
	wordmode = 1; /* NE2000 */
	neout8(DCRFT1 | DCRLS | (wordmode?DCRWTS:0), PG0DCR); /* 8bytes, normal, word */

	/* receive ring */
        neout8(rxstartpg, PG0PSTART);
        neout8(rxendpg, PG0PSTOP);
        neout8(rxstartpg, PG0BNRY); /* read pointer */ 
        neout8(CRPG1, rxstartpg, PG1CURR); /* write pointer */

	/* remote/local dma */
	neout8(0, PG0RSAR0);
	neout8(0, PG0RSAR1);
	neout8(0, PG0RBCR0);
	neout8(0, PG0RBCR1);
	neout8(0, PG0TPSR);
	neout8(0, PG0TBCR0);
	neout8(0, PG0TBCR1);

	/* read NIC's physical address */
	u8_t rom[16];
	remoteread(0, rom, 16);
	for (int i = 0; i < ETHALEN; i++) {
		hwaddr[i] = rom[i * 2];
		neout8(CRPG1, hwaddr[i], PG1PAR0 + i);
	}
	neout8(0x3f, PG0IMR); /* enable INT when bit is on */
	neout8(0xff, PG0ISR); /* ack the INT */
}

int ne2kdev_t::waitrdc()
{
	int retry = 100;
	while (retry--)
		if (nein8(PG0ISR) & ISRRDC)
			break;
	if (!retry)
		warn("waitrdc failed\n");
	neout8(ISRRDC, PG0ISR); /* ack the INT */
	return retry;
}

void ne2kdev_t::isr(int irqno_)
{
	int stat = nein8(PG0ISR);

#define IGNORE(flag) if (stat & flag) warn(#flag)
	IGNORE(ISRRDC);
	IGNORE(ISRCNT);
	IGNORE(ISROVW);
	IGNORE(ISRTXE);
	IGNORE(ISRRXE);
	if (stat & ISRPTX)
		isrptx();
	if (stat & ISRPRX)
		isrprx();
	neout8(0xff, PG0ISR); /* ack the interrupt */
}

void ne2kdev_t::isrptx()
{
	txing = 0;
	pkt_t * pkt = backlog.deqhead();
	if (pkt)
		hardwareoutput(pkt);
}

/* NOTE: 
   1) in the word transfer mode, only use the outw/inw instruction to transfer data.
   2) BNRY point to the first received packet which has not been fetched by host, 
      CURR point to the vacant location to receive another packet
      (BNRY == CURR) means the receive ring is empty. */
void ne2kdev_t::isrprx()
{
        pkt_t *pkt; 
	u8_t currpg = nein8(CRPG1, PG1CURR);
	u8_t bnrypg = nein8(PG0BNRY);
	while (bnrypg != currpg) {
		dp8390pkthdr_t hdr;
		remoteread(bnrypg * NEPG, &hdr, sizeof(hdr));
		/* check hdr.nextpage */
		if (hdr.nextpage < rxstartpg || hdr.nextpage >= rxendpg) {
			warn("strange nextpage\n");
			neout8(bnrypg = currpg, PG0BNRY); /* discard all the packet */
			break;
		}
		/* check the consistence between hdr.totlen and hdr.nextpage */
		u8_t diff;
		if (bnrypg <= hdr.nextpage)
			diff = hdr.nextpage - bnrypg;
		else
			diff = rxpgnum + hdr.nextpage - bnrypg;
		/* 4 bytes for CRC */
		if (diff * NEPG != roundup(hdr.totlen + 4, NEPG)) {
			warn("ne2k:diff=%d diff*256=%d hdr.totlen=%d\n", 
			       diff, diff*NEPG, hdr.totlen);
			neout8(bnrypg = currpg, PG0BNRY); /* discard all the packet */
			break;
		}
		/* check hdr.totlen */
		int pktlen = hdr.totlen - sizeof(hdr);
		if (pktlen < ETHMINFRAME || pktlen > ETHMAXFRAME) {
			warn("received packet has invalid length:%d\n", pktlen);
			goto go;
		}

		/* fetch the packet */
		if ((pkt = newpkt(ADRIVER, ETHPAD, pktlen)) == NULL) {
			warn("ne2k: newpkt failed\n");
			goto go;
		}
		remotereadrxring(bnrypg * NEPG + sizeof(hdr), pkt->data, pktlen);
		input(pkt);
go:		neout8(bnrypg = hdr.nextpage, PG0BNRY);
	}
}

/* linux's driver is not correct */
int ne2kdev_t::remotereadrxring(u16_t ringoffset, void *buf, int buflen)
{
	if (ringoffset + buflen <= rxendpg * NEPG)
		return remoteread(ringoffset, buf, buflen);
	int firstpart = rxendpg * NEPG - ringoffset;
	remoteread(ringoffset, buf, firstpart);
	remoteread(rxstartpg * NEPG, buf + firstpart, buflen - firstpart);
	return 0;
}

int ne2kdev_t::remoteread(u16_t ringoffset, void *buf, int buflen)
{
	neout16(ringoffset, PG0RSAR0, PG0RSAR1);
	int readlen = (wordmode && (buflen & 1)) ? buflen + 1 : buflen;
	assert(ringoffset + readlen <= rxendpg * NEPG);
	neout16(readlen, PG0RBCR0, PG0RBCR1);
	command(CRSTA | CRRDMA);

	if (wordmode) {
		insw(iobase + PG0DATA, buf, buflen >> 1);
		char pad[2];
		if (buflen & 1) {
			insw(iobase + PG0DATA, pad, 1);
			((char*)buf)[buflen - 1] = pad[0];
		}
	} else
		insb(iobase + PG0DATA, buf, buflen);
	waitrdc();
        return buflen;
}

/* 1) the transmit buffer only contain one packet 
   2) we only use inw/outw to access the data port when the NIC is in word mode 
   3) buf[buflen] maybe not accessble */
int ne2kdev_t::remotewrite(u16_t ringoffset, void *buf, int buflen)
{
        int writelen = (wordmode && (buflen & 1)) ? buflen + 1 : buflen;
	assert(ringoffset + writelen <= txendpg * NEPG);
        neout16(ringoffset, PG0RSAR0, PG0RSAR1);
        neout16(writelen, PG0RBCR0, PG0RBCR1);
	command(CRSTA | CRWDMA);
        if (wordmode) {
                outsw(iobase + PG0DATA, buf, buflen >> 1);
                if (buflen & 1) {
                        char pad[2];
                        pad[0] = ((char*)buf)[buflen - 1];
			outsw(iobase + PG0DATA, pad, 1);
                }
        } else 
                outsb(iobase + PG0DATA, buf, buflen);
	waitrdc();
        return buflen;
}

/* this function maybe called during interrupt  */ 
int ne2kdev_t::hardwareoutput(pkt_t* pkt)
{
	psw_t psw;

	psw.save(), cli();
	if (nein8(CR) & CRTXP) {
//	if (txing) {
		printd("@@ne2k output packet blocked!\n");
		backlog.enqtail(pkt);
		psw.restore();
		return 0;
	}
	txing = 1;
	remotewrite(txstartpg * NEPG, pkt->data, pkt->datalen);
        /* kick the packet out of NIC's rom */
	neout8(txstartpg, PG0TPSR); 
	int junk = (pkt->datalen < ETHMINFRAME) ? ETHMINFRAME : pkt->datalen;
	neout16(junk, PG0TBCR0, PG0TBCR1);
	command(CRNODMA | CRTXP | CRSTA);
	delpkt(pkt);
	psw.restore();
	return 0; 
}
