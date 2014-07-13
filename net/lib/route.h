/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Global definitions for the IP router interface.
 *
 * Version:	@(#)route.h	1.0.3	05/27/93
 *
 * Authors:	Original taken from Berkeley UNIX 4.3, (c) UCB 1986-1988
 *		for the purposes of compatibility only.
 *
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef _LINUX_ROUTE_H
#define _LINUX_ROUTE_H

/* This structure gets passed by the SIOCADDRT and SIOCDELRT calls. */
struct linuxrt_t {
	unsigned long hash; /* hash key for lookups */
	sockaddr_t dstaddr; /* target address */
	sockaddr_t gateway; /* gateway addr (RTF_GATEWAY) */
	sockaddr_t netmask; /* target network mask (IP)	*/
	short flags;
	short refcnt;
	unsigned long usecnt;
	void *ifnet;
	short metric; /* +1 for binary compatibility! */
	char *dev; /* forcing the device at add	*/
	unsigned long mss;  /* per route MTU/Window */
	unsigned long window; /* Window clamping */
	unsigned short irtt; /* Initial RTT */
};

#define	RTFUP		0x0001		/* route usable		  	  */
#define	RTFGATEWAY	0x0002		/* destination is a gateway	  */
#define	RTFHOST	        0x0004		/* host entry (net otherwise)	  */
#define RTFREINSTATE	0x0008		/* reinstate route after tmout	  */
#define	RTFDYNAMIC	0x0010		/* created dyn. (by redirect)	  */
#define	RTFMODIFIED	0x0020		/* modified dyn. (by redirect)	  */
#define RTFMSS		0x0040		/* specific MSS for this route	  */
#define RTFWINDOW	0x0080		/* per route window clamping	  */
#define RTFIRTT	        0x0100		/* Initial round trip time	  */
#define RTFREJECT	0x0200		/* Reject route			  */


#endif	/* _LINUX_ROUTE_H */

