#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include "bootparam.h"
#include "lib.h"

void gluebootsect(char*);
void gluesetup(char*);
void gluekern(char*);

char image[SECTSIZE + SETUPSIZE + MAXKERNSIZE];
int imagesize;
char * bootsect = image;
char * setup = image + SECTSIZE;
char * kcode = image + SECTSIZE + SETUPSIZE, * kdata;
char * bootsectfile, * setupfile, * kernfile;
bootparam_t * param = (bootparam_t*)(image + SECTSIZE + XBOOTPARAM);

void sigsegv(int signo)
{
	quit("glue receive SIGSEGV\n");
}

int main(int argc, char ** argv)
{
	if (argc != 4)
		quit("usage: glue bootsect setup kern\n");
	signal(SIGSEGV, sigsegv);
  fprintf(stderr, "Hello?\n");
	gluebootsect(argv[1]);
	gluesetup(argv[2]);
	gluekern(argv[3]);
	imagesize = SECTSIZE + SETUPSIZE + param->kernsize;
	write(1, image, imagesize);
	return 0;
}

void gluebootsect(char * file)
{
	elfphdr_t * code, * data; 

	bootsectfile = (char*) xmmap(file);
	getelfphdr(bootsectfile, &code, &data);
  fprintf(stderr, "> %d <= %d\n", code->filesz, SECTSIZE);
	assert(!code->vaddr && code->filesz <= SECTSIZE && !data->filesz);
	memcpy(bootsect, bootsectfile + code->offset, code->filesz);
}

void gluesetup(char * file)
{
	elfphdr_t * code, * data; 

	setupfile = (char*) xmmap(file);
	getelfphdr(setupfile, &code, &data);
	assert(!code->vaddr && code->filesz <= SETUPSIZE && !data->filesz);
	memcpy(setup, setupfile + code->offset, code->filesz);
}

#define KCODEVADDR (KERNSTART + KERNPA1)
typedef unsigned long u32_t;
void gluekern(char * file) /* not include the bss */
{
	elfphdr_t * code, * data;
	u32_t * u32;

	kernfile = (char*) xmmap(file);
	getelfphdr(kernfile, &code, &data);
	assert(code->vaddr == KCODEVADDR);
	memcpy(kcode, kernfile + code->offset, code->filesz);
	kdata = kcode + data->vaddr - code->vaddr;
	memcpy(kdata, kernfile + data->offset, data->filesz);
	param->kernsize = (data->vaddr + data->filesz - code->vaddr + SECTSIZE - 1) & ~(SECTSIZE - 1);
	assert(param->kernsize < MAXKERNSIZE);
	param->kernentry = ((elfhdr_t *)kernfile)->entry;
	param->kerncksum = 0;
	for (u32 = (u32_t*)kcode; u32 < (u32_t*)(kcode+param->kernsize); u32++)
		param->kerncksum += *u32;
	fprintf(stderr, "ksize = %d(sect) cksum = %x\n", 
		param->kernsize/SECTSIZE, param->kerncksum);
}
