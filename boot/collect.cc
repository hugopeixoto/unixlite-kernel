#include <stdio.h>
#include <stdlib.h>
#include "lib.h"
#include "collect.h"

int main(int argc, char ** argv)
{
	char * file;
	elfphdr_t * code, * data;
	unsigned long * src, * dst;

	if (argc != 2)
		quit("Usage: collect file\n");
	file = (char*) xmmap(argv[1]);
	getelfphdr(file, &code, &data);
	src = ctorlist;
	dst = (unsigned long *)(file + CTORLISTVADDR - data->vaddr + data->offset);
	while (*dst++ = *src++)
		;
	return 0;
}
