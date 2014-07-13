#include "root.h"

void assertfail(char * expr, char * file, int line, const char * pretty)
{
	printf("%s:%d:%s() '%s' failed\n", file, line, pretty, expr);
	panic("assert fail\n");
}
