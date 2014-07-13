#include <lib/root.h>
#include <lib/printf.h>
#include "ostream.h"

void ostream_t::write(char * fmt, ...)
{
	if (cursor + 32 > eroom)
		return;
	va_list ap;
	va_start(ap, fmt);
	int i = vsprintf(cursor, fmt, ap); /* should be vsnprintf */
	va_end(ap);
	cursor += i;
}
