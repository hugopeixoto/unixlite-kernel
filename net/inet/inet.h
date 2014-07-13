#ifndef _LINUXINET_H
#define _LINUXINET_H

static __inline__ unsigned long int
__ntohl(unsigned long int x)
{
	__asm__("xchgb %b0,%h0\n\t"	/* swap lower bytes	*/
		"rorl $16,%0\n\t"	/* swap words		*/
		"xchgb %b0,%h0"		/* swap higher bytes	*/
		:"=q" (x)
		: "0" (x));
	return x;
}

static __inline__ unsigned long int
__constant_ntohl(unsigned long int x)
{
	return (((x & 0x000000ff) << 24) |
		((x & 0x0000ff00) <<  8) |
		((x & 0x00ff0000) >>  8) |
		((x & 0xff000000) >> 24));
}

static __inline__ unsigned short int
__ntohs(unsigned short int x)
{
	__asm__("xchgb %b0,%h0"		/* swap bytes		*/
		: "=q" (x)
		:  "0" (x));
	return x;
}

static __inline__ unsigned short int
__constant_ntohs(unsigned short int x)
{
	return (((x & 0x00ff) << 8) |
		((x & 0xff00) >> 8));
}

#define __htonl(x) __ntohl(x)
#define __htons(x) __ntohs(x)
#define __constant_htonl(x) __constant_ntohl(x)
#define __constant_htons(x) __constant_ntohs(x)

#define ntohl(x) \
(__builtin_constant_p((x)) ? \
 __constant_ntohl((x)) : \
 __ntohl((x)))

#define ntohs(x) \
(__builtin_constant_p((x)) ? \
 __constant_ntohs((x)) : \
 __ntohs((x)))

#define htonl(x) \
(__builtin_constant_p((x)) ? \
 __constant_htonl((x)) : \
 __htonl((x)))

#define htons(x) \
(__builtin_constant_p((x)) ? \
 __constant_htons((x)) : \
 __htons((x)))

/* host to little endian */
extern inline u16_t h2le16(u16_t u16)
{
	return u16;
};

extern inline u32_t h2le32(u32_t u32)
{
	return u32;
};

/* little endian to host */
extern inline u16_t le2h16(u16_t u16)
{
	return u16;
};

extern inline u32_t le2h32(u32_t u32)
{
	return u32;
};

/* Standard well-defined IP protocols.  */
enum {
  IPPROTOIP = 0,		/* Dummy protocol for TCP		*/
  IPPROTOICMP = 1,		/* Internet Control Message Protocol	*/
  IPPROTOGGP = 2,		/* Gateway Protocol (deprecated)	*/
  IPPROTOTCP = 6,		/* Transmission Control Protocol	*/
  IPPROTOEGP = 8,		/* Exterior Gateway Protocol		*/
  IPPROTOPUP = 12,		/* PUP protocol				*/
  IPPROTOUDP = 17,		/* User Datagram Protocol		*/
  IPPROTOIDP = 22,		/* XNS IDP protocol			*/
  IPPROTORAW = 255,		/* Raw IP packets			*/
  IPPROTOMAX
};

#define __SOCKSIZE__	16	/* sizeof(struct sockaddr) */
struct sockaddrin_t {
	u16_t family; 	/* Address family	*/
	u16_t port;	/* Port number		*/
 	u32_t addr;	/* Internet address	*/
	u8_t __pad[8];	/* Pad to size of `struct sockaddr'. */
} __attribute__((packed));

/*
 * Definitions of the bits in an Internet address integer.
 * On subnets, host and network parts are found according
 * to the subnet mask, not these masks.
 */
#define	INCLASSA(a)	((((long int) (a)) & 0x80000000) == 0)
#define	INCLASSANET	0xff000000
#define	INCLASSANSHIFT	24
#define	INCLASSAHOST	(0xffffffff & ~INCLASSANET)
#define	INCLASSAMAX	128

#define	INCLASSB(a)	((((long int) (a)) & 0xc0000000) == 0x80000000)
#define	INCLASSBNET	0xffff0000
#define	INCLASSBNSHIFT	16
#define	INCLASSBHOST	(0xffffffff & ~INCLASSBNET)
#define	INCLASSBMAX	65536

#define	INCLASSC(a)	((((long int) (a)) & 0xc0000000) == 0xc0000000)
#define	INCLASSCNET	0xffffff00
#define	INCLASSCNSHIFT	8
#define	INCLASSCHOST	(0xffffffff & ~INCLASSCNET)

#define	INCLASSD(a)	((((long int) (a)) & 0xf0000000) == 0xe0000000)
#define	INMULTICAST(a)	INCLASSD(a)

#define	INEXPERIMENTAL(a) ((((long int) (a)) & 0xe0000000) == 0xe0000000)
#define	INBADCLASS(a)   ((((long int) (a)) & 0xf0000000) == 0xf0000000)

/* Address to accept any incoming messages. */
#define	INADDRANY       ((unsigned long int) 0x00000000)

/* Address to send to all hosts. */
#define	INADDRBROADCAST	((unsigned long int) 0xffffffff)

/* Address indicating an error return. */
#define	INADDRNONE      0xffffffff

/* indicate ipaddr is not allocated yet */
#define INADDRNULL      0xf0000000

/* Network number for local host loopback. */
#define	INADDRLOOPBACKNET   0x7f000000 

/* Address to loopback in software to local host.  */
#define	INADDRLOOPBACK  0x7f000001 /* 127.0.0.1 */

#define IPHDRINCL	2		/* raw packet header option	*/

#define IPOPTEND	0
#define IPOPTNOP	1	/* no operation */
#define IPOPTSECURITY	130
#define IPOPTLSRR	131	/* loose source routing */
#define IPOPTSSRR	137	/* strict source routing */
#define IPOPTRR	        7	/* route record */
#define IPOPTSTREAMID	136
#define IPOPTTIMESTAMP	68

/* 
 * Option type is viewed as three field:
 * 1 bit : copy flag
 * 2 bit : option class
 * 5 bit : option number
 *
 * Option length count the option-type byte and option-length byte as well as
 * the option-data bytes.
 *
 * Option pointer is relative to this option, and it count from 1 not 0;
 */
#define MAXROUTE 9
struct ipoptroute_t {
	u8_t	type;
	u8_t	len;
	u8_t	ptr;		/* the smallest legal value is 4 */
	u32_t	addr[MAXROUTE];
}__attribute__((packed));

struct ipopt_timestamp_t {
	u8_t	type;
	u8_t	len;	
	u8_t	ptr;		/* the smallest legal value is 5 */
	unsigned flag:4;
	unsigned overflow:4;
	u32_t	data[MAXROUTE];
}__attribute__((packed)); 

#define IPFRAGOFF 	0x1fff 
#define IPFRAGMF	0x2000 /* more frag */
#define IPFRAGDF	0x4000 /* don't frag */
#define IPFRAGRESERVE	0x8000

#define IPVER4 4
struct iphdr_t {
	unsigned ihl:4;		/* head length, in uint of four bytes */
	unsigned ver:4;		/* version */
	u8_t tos;		/* type of service */ 
	u16_t totlen;		/* totoal len, include header */
	u16_t id;
	u16_t frag;
	u8_t ttl;		/* time to live */
	u8_t proto;		/* protocol */
	u16_t chksum;
	u32_t saddr;
	u32_t daddr;
	char data[0];

	int headlen() { return ihl << 2; }; /* host byte */
	int datalen() { return ntohs(totlen) - headlen(); }; /* host byte */
	u16_t fragoff()	{ return (frag & IPFRAGOFF) << 3; };
	/* whether this packet has been fragmented */
	bool fraged() { return ntohs(frag) & (IPFRAGMF + IPFRAGOFF); }
} __attribute__((packed)); 



extern inline int ipneteq(u32_t me, u32_t him, u32_t mask)
{
        return ((me ^ him) & mask) == 0; 
}
extern u32_t inetaton(char *ascii); 
extern char * inetntoa(u32_t net); 
extern u32_t inetmask(u32_t ipaddr);
extern u16_t ipchksum(void *v, int n8);
extern u16_t ipchksumpseudo(u32_t saddr, u32_t daddr, u8_t proto, 
                              u16_t datalen, void *data);
extern u16_t udpchksum(u32_t saddr, u32_t daddr, u16_t datalen, void *data);
extern u16_t tcpchksum(u32_t saddr, u32_t daddr, u16_t datalen, void *data);

#define mkipaddr(a,b,c,d) ((a) + ((b)<<8) + ((c)<<16) + ((d)<<24))
#define USERSOCK 1024 
#endif	/* _LINUXINH */
