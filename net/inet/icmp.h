#ifndef	_NETICMP_H
#define _NETICMP_H

/* ICMP TYPE */
enum {  ICTECHOREPLY = 0,         // Echo Reply
        ICTDSTUNREACH = 3,         // Destination Unreachable
        ICTSRCQUENCH = 4,         // Source Quench
        ICTREDIRECT = 5,          // Redirect (change route)
        ICTECHOREQUEST = 8,       // Echo Request
        ICTTIMEEXCEEDED = 11,     // Time Exceeded
        ICTPARAPROBLEM = 12,      // Parameter Problem
        ICTTIMESTAMPREQUEST = 13, // Timestamp Request
        ICTTIMESTAMPREPLY = 14,   // Timestamp Reply
        ICTINFOREQUEST = 15,      // Information Request
        ICTINFOREPLY = 16,        // Information Reply
        ICTADDRREQUEST = 17,      // Address Mask Request
        ICTADDRREPLY = 18,        // Address Mask Reply
};

/* ICMP CODE */
enum {	ICCNETUNREACH = 0,
	ICCHOSTUNREACH = 1,
	ICCPROTOUNREACH = 2,
	ICCPORTUNREACH = 3,
	ICCFRAGDF = 4,            // fragmentation needed and DF set
};

struct icmphdr_t {
	u8_t type;
	u8_t code;
	u16_t chksum;
}; 

struct icmpecho_t {
	u16_t id;
	u16_t seq;
};

struct icmpredirect_t {
	u32_t gw;
};

extern void icmpoutput(pkt_t* pkt, int type, int code);
extern int icmpinput(pkt_t* pkt);

#endif
