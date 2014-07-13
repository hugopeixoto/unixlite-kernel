#ifndef _LINUXIFETHER_H
#define _LINUXIFETHER_H

/* IEEE 802.3 Ethernet magic constants.  The frame sizes omit the preamble
   and FCS/CRC (frame check sequence). */
#define ETHPAD 2    /* make the ip header at 4 bytes boundary */
#define ETHALEN	6   /* Octets in one ethernet addr */
#define ETHHLEN	14  /* Total octets in header.  */
#define ETHMTU  1500 /* Max. octets in payload */
#define ETHMINFRAME 60
#define ETHMAXFRAME 1514

/* +--------+---------+---------+-------+---------+------+
 * | pad(2) |target(6)|source(6)|type(2)|mtu(1500)|crc(4)|
 * +--------+---------+---------+-------+---------+------+ */

/* These are the defined Ethernet Protocol ID's. */
#define ETHPLOOP 0x0060		/* Ethernet Loopback packet	*/
#define ETHPECHO 0x0200		/* Ethernet Echo packet		*/
#define ETHPPUP	0x0400		/* Xerox PUP packet		*/
#define ETHPIP	0x0800		/* Internet Protocol packet	*/
#define ETHPARP	0x0806		/* Address Resolution packet	*/
#define ETHPRARP 0x8035		/* Reverse Addr Res packet	*/
#define ETHPX25	0x0805		/* CCITT X.25			*/
#define ETHPIPX	0x8137		/* IPX over DIX			*/
#define ETHP8023 0x0001		/* Dummy type for 802.3 frames  */
#define ETHPAX25 0x0002		/* Dummy protocol id for AX.25  */
#define ETHPALL	0x0003		/* Every packet (be careful!!!) */

/* This is an Ethernet frame header. */
struct ethhdr_t {
	u8_t dst[ETHALEN];	/* destination eth addr	*/
	u8_t src[ETHALEN];	/* source ether addr	*/
	u16_t proto;		/* packet type ID field	*/
};

/* Ethernet statistics collection data. */
struct ethstat_t {
  int	rxpackets;			/* total packets received	*/
  int	txpackets;			/* total packets transmitted	*/
  int	rxerrors;			/* bad packets received		*/
  int	txerrors;			/* packet transmit problems	*/
  int	rxdropped;			/* no space in linux buffers	*/
  int	txdropped;			/* no space available in linux	*/
  int	multicast;			/* multicast packets received	*/
  int	collisions;

  /* detailed rxerrors: */
  int	rxlengtherrors;
  int	rxovererrors;			/* receiver ring buff overflow	*/
  int	rxcrcerrors;			/* recved pkt with crc error	*/
  int	rxframeerrors;		/* recv'd frame alignment error */
  int	rxfifoerrors;			/* recv'r fifo overrun		*/
  int	rxmissederrors;		/* receiver missed packet	*/

  /* detailed txerrors */
  int	txabortederrors;
  int	txcarriererrors;
  int	txfifoerrors;
  int	txheartbeaterrors;
  int	txwindowerrors;
};

extern char * ethaddrtoa(uchar *ethaddr);

#endif	/* LINUXIFETHERH */
