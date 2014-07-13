#include "root.h"
#include "sock.h"
#include "tcp.h"
#include "udp.h"
#include "raw.h"
#include <lib/errno.h>

int inetsocket(int domain, int type, int protocol)
{
	sock_t * s = NULL;
	int e;

	switch (type) {
		case SOCKSTREAM:
			s = new tcpsock_t();
			break;
		case SOCKDGRAM:
			s = new udpsock_t();
			break;
		case SOCKRAW:
			if (!suser())
				return EACCES;
			s = new rawsock_t(protocol);
			break;
	        default:
			return EPROTONOSUPPORT;
	}
	e = curr->fdvec->put(s);
	if (e < 0)
		delete s;
	return e;
}

int inetsocketpair(int domain, int type, int protocol, int* pair)
{
	return 0;
}
