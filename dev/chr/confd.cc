#include <lib/root.h>
#include "confd.h"
#include "condev.h"

confd_t::~confd_t()
{
}

int confd_t::read(void * buf, int count)
{
	return condev->read(buf, count);
}

int confd_t::write(void * buf, int count)
{
	return condev->write(buf, count);
}

int confd_t::ioctl(int cmd, ulong arg)
{
	return condev->ioctl(cmd, arg);
}
