#include <lib/root.h> 
#include <lib/printf.h>
#include "inet.h"

int inetequal(u32_t me, u32_t him)
{
	u32_t mask = 0xffffffff;

	if (me == him)
		return 1;
	for (int i = 4; i--; me >>= 8, him >>= 8, mask >>= 8) {
		if ((me & 0xff) == (him & 0xff))
			continue;
		if ((me == 0) || (me == mask))
			return 1;
		return 0;
	}
	return 1;
}

/* printf("%s %s", inet_ntoa(src), inet_ntoa(dst)); will be ok now */
char * inetntoa(u32_t net32)
{
	static char bufs[4][16];
	static uint index = 0;

	char * buf = bufs[index++ & 3];
	u8_t * p = (u8_t *) &net32;
	sprintf(buf, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
	return buf;
}

u32_t inetaton(char * c)
{
	u8_t tmp[4];

	for (int i = 0; i < 4; i++) {
		tmp[i] = 0;
		while ((*c >= '0') && (*c <='9')) {
			tmp[i] *= 10;
			tmp[i] += *c - '0';
			c++;
		}
		c++;
	}
	u32_t host32 = (tmp[0] << 24) | (tmp[1] << 16) | (tmp[2] << 8) | tmp[3];
	return htonl(host32);
} 

/* sums up all 16 bit words in a memory portion. also include any odd byte */
static u32_t sumup(void *v, int n8)
{
	u16_t * u16 = (u16_t *) v;
	u32_t sum = 0;

	for (int n16 = n8 >> 1; n16--; u16++)
		sum += *u16;
	if (n8 & 1)
		sum += *(u8_t*)u16;
        return sum;
}

static inline u16_t convert(u32_t sum)
{
	sum = (sum >> 16) + (sum & 0xffff);
	sum += sum >> 16;
	return ~(sum & 0xffff);
} 

u16_t ipchksum(void *v, int n8)
{
        u32_t sum = sumup(v, n8);
        return convert(sum); 
}

/* struct pseudo {
          u32_t saddr;
          u32_t daddr;
          u8_t zero;
          u8_t proto;
          u16_t datalen; */
/* saddr, daddr is in network-byte-order */
u16_t ipchksumpseudo(u32_t saddr, u32_t daddr, u8_t proto, u16_t datalen, void *data)
{
        u32_t sum = 0;
        sum += saddr & 0xffff;
        sum += saddr >> 16;
        sum += daddr & 0xffff;
        sum += daddr >> 16;
        sum += htons(datalen);
        sum += htons((u16_t)(proto)); 
        sum += sumup(data, datalen);
        return convert(sum);
}

u16_t udpchksum(u32_t saddr, u32_t daddr, u16_t datalen, void *data)
{
	return ipchksumpseudo(saddr, daddr, IPPROTOUDP, datalen, data);
}

u16_t tcpchksum(u32_t saddr, u32_t daddr, u16_t datalen, void *data)
{
	return ipchksumpseudo(saddr, daddr, IPPROTOTCP, datalen, data);
}

u32_t inetmask(u32_t ipaddr)
{
        ipaddr = ntohl(ipaddr);
        if (INCLASSA(ipaddr))
                return htonl(INCLASSANET);
        if (INCLASSB(ipaddr))
                return htonl(INCLASSBNET);
        return htonl(INCLASSCNET);
}
