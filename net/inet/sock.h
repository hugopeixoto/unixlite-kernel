#ifndef _NETINETSOCK_H
#define _NETINETSOCK_H

extern int inetsocket(int domain, int type, int protocol);
extern int inetsocketpair(int domain, int type, int protocol, int *vect);
#endif
