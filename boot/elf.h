#ifndef _LINUXELF_H
#define _LINUXELF_H

typedef unsigned long elfaddr_t;
typedef unsigned short elfhalf_t;
typedef unsigned long elfoff_t;
typedef long elfsword_t;
typedef unsigned long elfword_t;

/* These constants are for the segment types stored in the image headers */
#define PTNULL    0
#define PTLOAD    1
#define PTDYNAMIC 2
#define PTINTERP  3
#define PTNOTE    4
#define PTSHLIB   5
#define PTPHDR    6
#define PTLOPROC  0x70000000
#define PTHIPROC  0x7fffffff

/* These constants define the different elf file types */
#define ETNONE   0
#define ETREL    1
#define ETEXEC   2
#define ETDYN    3
#define ETCORE   4
#define ETLOPROC 5
#define ETHIPROC 6

/* These constants define the various ELF target machines */
#define EMNONE  0
#define EMM32   1
#define EMSPARC 2
#define EM386   3
#define EM68K   4
#define EM88K   5
#define EM486   6		/* Perhaps disused */
#define EM860   7
#define EMPPC   20

/* This is the info that is needed to parse the dynamic section of the file */
#define DTNULL		0
#define DTNEEDED	1
#define DTPLTRELSZ	2
#define DTPLTGOT	3
#define DTHASH		4
#define DTSTRTAB	5
#define DTSYMTAB	6
#define DTRELA		7
#define DTRELASZ	8
#define DTRELAENT	9
#define DTSTRSZ		10
#define DTSYMENT	11
#define DTINIT		12
#define DTFINI		13
#define DTSONAME	14
#define DTRPATH 	15
#define DTSYMBOLIC	16
#define DTREL	        17
#define DTRELSZ		18
#define DTRELENT	19
#define DTPLTREL	20
#define DTDEBUG		21
#define DTTEXTREL	22
#define DTJMPREL	23
#define DTLOPROC	0x70000000
#define DTHIPROC	0x7fffffff

/* This info is needed when parsing the symbol table */
#define STBLOCAL  0
#define STBGLOBAL 1
#define STBWEAK   2

#define STTNOTYPE  0
#define STTOBJECT  1
#define STTFUNC    2
#define STTSECTION 3
#define STTFILE    4

#define ELF32STBIND(x) ((x) >> 4)
#define ELF32STTYPE(x) (((unsigned int) x) & 0xf)

/* Symbolic values for the entries in the auxiliary table
   put on the initial stack */
#define ATNULL   0		/* end of vector */
#define ATIGNORE 1		/* entry should be ignored */
#define ATEXECFD 2		/* file descriptor of program */
#define ATPHDR   3		/* program headers for program */
#define ATPHENT  4		/* size of program header entry */
#define ATPHNUM  5		/* number of program headers */
#define ATPAGESZ 6		/* system page size */
#define ATBASE   7		/* base address of interpreter */
#define ATFLAGS  8		/* flags */
#define ATENTRY  9		/* entry point of program */
#define ATNOTELF 10		/* program is not ELF */
#define ATUID    11		/* real uid */
#define ATEUID   12		/* effective uid */
#define ATGID    13		/* real gid */
#define ATEGID   14		/* effective gid */

struct elfdyn_t
{
  elfsword_t tag;
  union
  {
    elfsword_t val;
    elfaddr_t ptr;
  }
  u;
};

extern elfdyn_t DYNAMIC[];

/* The following are used with relocations */
#define ELF32RSYM(x) ((x) >> 8)
#define ELF32RTYPE(x) ((x) & 0xff)

#define R386NONE	0
#define R38632		1
#define R386PC32	2
#define R386GOT32	3
#define R386PLT32	4
#define R386COPY	5
#define R386GLOBDAT	6
#define R386JMPSLOT	7
#define R386RELATIVE	8
#define R386GOTOFF	9
#define R386GOTPC	10
#define R386NUM		11

struct elf32rel_t
{
  elfaddr_t offset;
  elfword_t info;
};

struct elf32rela_t
{
  elfaddr_t offset;
  elfword_t info;
  elfsword_t addend;
};

struct elf32sym_t
{
  elfword_t name;
  elfaddr_t value;
  elfword_t size;
  unsigned char info;
  unsigned char other;
  elfhalf_t shndx;
};

#define EINIDENT	16

struct elfhdr_t
{
  unsigned char ident[EINIDENT];
  elfhalf_t type;
  elfhalf_t machine;
  elfword_t version;
  elfaddr_t entry;		/* Entry point */
  elfoff_t phoff;
  elfoff_t shoff;
  elfword_t flags;
  elfhalf_t ehsize;
  elfhalf_t phentsize;
  elfhalf_t phnum;
  elfhalf_t shentsize;
  elfhalf_t shnum;
  elfhalf_t shstrndx;
};

/* These constants define the permissions on sections in the program
   header, pflags. */
#define PFR		0x4
#define PFW		0x2
#define PFX		0x1

struct elfphdr_t
{
  elfword_t type;
  elfoff_t offset;
  elfaddr_t vaddr;
  elfaddr_t paddr;
  elfword_t filesz;
  elfword_t memsz;
  elfword_t flags;
  elfword_t align;
};

/* shtype */
#define SHTNULL	0
#define SHTPROGBITS	1
#define SHTSYMTAB	2
#define SHTSTRTAB	3
#define SHTRELA	4
#define SHTHASH	5
#define SHTDYNAMIC	6
#define SHTNOTE	7
#define SHTNOBITS	8
#define SHTREL		9
#define SHTSHLIB	10
#define SHTDYNSYM	11
#define SHTNUM		12
#define SHTLOPROC	0x70000000
#define SHTHIPROC	0x7fffffff
#define SHTLOUSER	0x80000000
#define SHTHIUSER	0xffffffff

/* shflags */
#define SHFWRITE	0x1
#define SHFALLOC	0x2
#define SHFEXECINSTR	0x4
#define SHFMASKPROC	0xf0000000

/* special section indexes */
#define SHNUNDEF	0
#define SHNLORESERVE	0xff00
#define SHNLOPROC	0xff00
#define SHNHIPROC	0xff1f
#define SHNABS		0xfff1
#define SHNCOMMON	0xfff2
#define SHNHIRESERVE	0xffff

struct elfshdr_t
{
  elfword_t name;
  elfword_t type;
  elfword_t flags;
  elfaddr_t addr;
  elfoff_t offset;
  elfword_t size;
  elfword_t link;
  elfword_t info;
  elfword_t addralign;
  elfword_t entsize;
};

#define	EIMAG0		0	/* eident[] indexes */
#define	EIMAG1		1
#define	EIMAG2		2
#define	EIMAG3		3
#define	EICLASS	4
#define	EIDATA		5
#define	EIVERSION	6
#define	EIPAD		7

#define	ELFMAG0		0x7f	/* EIMAG */
#define	ELFMAG1		'E'
#define	ELFMAG2		'L'
#define	ELFMAG3		'F'
#define	ELFMAG		"\177ELF"
#define	SELFMAG		4

#define	ELFCLASSNONE	0	/* EICLASS */
#define	ELFCLASS32	1
#define	ELFCLASS64	2
#define	ELFCLASSNUM	3

#define ELFDATANONE	0	/* eident[EIDATA] */
#define ELFDATA2LSB	1
#define ELFDATA2MSB	2

#define EVNONE		0	/* eversion, EIVERSION */
#define EVCURRENT	1
#define EVNUM		2

/* Notes used in ETCORE */
#define NTPRSTATUS	1
#define NTPRFPREG	2
#define NTPRPSINFO	3
#define NTTASKSTRUCT	4

/* Note header in a PTNOTE section */
struct elfnote_t
{
  elfword_t namesz;		/* Name size */
  elfword_t descsz;		/* Content size */
  elfword_t type;		/* Content type */
};

#define ELFSTARTMMAP 0x80000000

#endif /* LINUXELFH */
