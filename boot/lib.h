#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>
#include "elf.h"

#define error(...) \
do { fprintf(stderr, __VA_ARGS__); perror(":"); exit(1); } while (0)
#define quit(...) \
do { fprintf(stderr, __VA_ARGS__); exit(1); } while (0)

static int fsize(int fd)
{
	struct stat state;

	if (fstat(fd, &state) < 0)
		error("stat");
	return state.st_size;
}

void * xmmap(char * file)
{
	int fd;
	void * result;
	
	fd = open(file, O_RDWR);
	if (fd < 0)
		error("open %s failed", file);
	result = mmap(NULL, fsize(fd), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (result == MAP_FAILED)
		error("mmap on %s failed", file);
	close(fd);
	return result;
}

static void validelfhdr(elfhdr_t * e)
{	
	assert(e->ident[EIMAG0] == ELFMAG0);
	assert(e->ident[EIMAG1] == ELFMAG1);
	assert(e->ident[EIMAG2] == ELFMAG2);
	assert(e->ident[EIMAG3] == ELFMAG3);
	assert(e->ident[EICLASS]== ELFCLASS32);
	assert(e->ident[EIDATA] == ELFDATA2LSB);
	assert(e->type == ETEXEC);
	assert(e->machine == EM386 || e->machine == EM486);
	//assert(e->phnum <= 3);
	assert(e->phentsize == sizeof(elfphdr_t));
}

int getelfphdr(char * head, elfphdr_t ** code, elfphdr_t ** data)
{
	int i, found = 0;
	elfhdr_t * e;
	elfphdr_t * p;

	e = (elfhdr_t *) head;
	validelfhdr(e);
	p = (elfphdr_t *)(head + e->phoff);
	for (i = 0; i < e->phnum; i++) {
		if (p[i].type != PTLOAD)
			continue;
		found++;
		if ((p[i].flags&PFR) && (p[i].flags&PFX))
			*code = p + i;
		else if ((p[i].flags&PFR) && (p[i].flags&PFW))
			*data = p + i;
		else assert(0);
	}
	return found;
}
