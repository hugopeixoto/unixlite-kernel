#include <lib/root.h>
#include <lib/errno.h>
#include <lib/string.h>
#include <lib/gcc.h>
#include <mm/mm.h>
#include <fs/inode.h>
#include <fs/buf.h>
#include <boot/elf.h> 
#include <asm/frame.h>
#include <mm/allocpage.h>

static void flush()
{	
	if (lastusedmath == curr)
		lastusedmath = NULL;
	curr->mm->unmapuspace();
	curr->sigset.clear();
	curr->sigvec->clearonexec();
	curr->fdvec->closeonexec();
	if (lastusedmath == curr)
		lastusedmath = NULL;
	curr->flags &= ~TFUSEDMATH;
}

#define MAXPH 6
static int loadelf(inode_t * exe, buf_t * head, vaddr_t * entry)
{
	elfphdr_t ph[MAXPH], *p;
	int e;
	elfhdr_t * eh = (elfhdr_t*) head->data;

	*entry = 0;
	if (eh->phnum > 6) 
		return ENOEXEC;
	if ((e = exe->read(eh->phoff, ph, sizeof(elfphdr_t)*eh->phnum)) < 0)
		return e;
#if 0
	for (p = ph; p < ph + eh->phnum; p++) {
		if (p->type != PTLOAD)
			continue;
		printf("%d:%x--%x ", p->type, p->vaddr, p->offset);
	}
	printf("\n");
#endif
	for (p = ph; p < ph + eh->phnum; p++) {
		if (p->type != PTLOAD)
			continue;
		int prot = 0;
		if (p->flags & PFR)
			prot |= PROTR;
		if (p->flags & PFW)
			prot |= PROTW;
		if (p->flags & PFX)
			prot |= PROTX;
		if (e = curr->mm->mmap(p->vaddr, p->vaddr + p->filesz,
			prot, MAPFIXED|MAPPRIVATE, exe, p->offset))
			return e;
		if (p->flags & PFX)
			continue;
		vaddr_t bss = p->vaddr + p->filesz;
		vaddr_t ebss = p->vaddr + p->memsz;
		curr->mm->clearfrag(bss, pageup(bss));
		/* an empty bss will be created if (bss == ebss) */
		if (e = curr->mm->anonmap(pageup(bss), pageup(ebss),
			prot, MAPFIXED|MAPPRIVATE))
			return e;
	}
	*entry = eh->entry;
	return 0;
}

static int matchelf(buf_t * head)
{
	elfhdr_t * e = (elfhdr_t *) head->data;
	return  (e->ident[EIMAG0] == ELFMAG0) && 
		(e->ident[EIMAG1] == ELFMAG1) && 
		(e->ident[EIMAG2] == ELFMAG2) && 
		(e->ident[EIMAG3] == ELFMAG3) && 
		(e->ident[EICLASS]== ELFCLASS32) && 
		(e->ident[EIDATA] == ELFDATA2LSB) && 
		(e->type == ETEXEC) && 
		(e->machine == EM386 || e->machine == EM486) && 
		(e->phnum <= 3) && 
		(e->phentsize == sizeof(elfphdr_t));
}

static int matchscript(buf_t * head)
{
	return head->data[0] == '#' && head->data[1] == '!';
}

/* interpbasename interpargument pathname argv[1] ... */
static int parsescript(buf_t * head, ustack_t * ustack, inode_t ** interp)
{
	char buf[256];
	char * tmp, * tok;
	int e;

	*interp = NULL;
	memcpy(buf, head->data + 2, 256);
	buf[255] = 0;
	if  (!(tmp = strchr(buf, '\n')))
		return ENOEXEC;
	*tmp = 0;

	scanstrprev_t scan(buf, " \t");
	if (!scan.nlefttok())
		return ENOEXEC;
	for (; scan.more(); scan.prev()) {
		tok = scan.curtok();
		if (scan.nlefttok() == 1) {
			if (e = namei(ISTAT, tok, interp))
				return e;
			if (tmp = strrchr(tok, '/'))
				tok = tmp + 1;
		}
		if (e = ustack->pushkstr(tok))
			return e;
		ustack->argc++;
	}
	return 0;
}

static int count(char ** vec)
{
	int sum = 0;
	while (*vec++) sum++;
	return sum;
}

static int check(inode_t * inode, buf_t ** head, uid_t * euid, gid_t * egid)
{
	*head = NULL;
	if (!inode->isreg() || inode->deny(IX))
		return EACCES;
	*euid = (inode->mode & SISUID) ? inode->uid : curr->euid;
	*egid = (inode->mode & SISGID) ? inode->gid : curr->egid;

	bno_t phys;
	int e;
	if (e = inode->readmap(0, &phys))
		return e;
	if (phys == BNOTALLOC)
		return ENOEXEC;
	if (e = readb(inode->dev, phys, head))
		return e;
	return 0;
}

asmlinkage int sysexecve(char * pathname, char * argv[], char * envp[])
{
	regs_t * regs = (regs_t*)&pathname;
	int e = 0;
	ustack_t ustack;
	inode_t * inode = NULL;
	buf_t * head = NULL;
	uid_t euid;
	gid_t egid;

#if 0
	printf("exec %s ", pathname);
	for (int i = 0; argv[i]; i++)
		printf("%s ", argv[i]);
	printf("\n");
#endif
	if (lowfreepage(8))
		return ENOMEM;
	ustack.init();
	ustack.push((ATEGID + 2)* sizeof(long) * 2);
	if (e = namei(ISTAT, pathname, &inode))
		goto error;
	if (e = check(inode, &head, &euid, &egid))
		goto error;
	/* exepath arg0 arg1 ...
	   interppath interpbase interparg exepath arg1 ... */
	if (matchscript(head)) {
		if (e = ustack.pushuvec(envp))
			goto error;
		ustack.envc = count(envp);
		if (e = ustack.pushuvec(argv+1))
			goto error;
		ustack.argc = count(argv+1);
		if (e = ustack.pushustr(pathname))
			goto error;
		ustack.argc++;
		inode_t * interp;
		if (e = parsescript(head, &ustack, &interp))
			goto error;
		head->lose();
		head = NULL;
		inode->lose();
		inode = interp;
		if (e = check(inode, &head, &euid, &egid))
			goto error;
	} else {
		if (e = ustack.pushuvec(envp))
			goto error;
		ustack.envc = count(envp);
		if (e = ustack.pushuvec(argv))
			goto error;
		ustack.argc = count(argv);
	}
	if (!matchelf(head)) {
		e = ENOEXEC;
		goto error;
	}
	curr->execname.set(pathname); /* must be down prior to flush */
	curr->euid = euid;
	curr->egid = egid;

	flush();
	vaddr_t entry, initsp;
	if (e = loadelf(inode, head, &entry)) {
		printf("execve error occured after flush old exec\n");
		goto error;
	}
	if (e = curr->mm->initustack(&ustack, &initsp)) {
		printf("execve error occured after flush old exec\n");
		goto error;
	}
	regs->eip = entry;
	regs->esp = initsp;
	inode->lose();
	head->lose();
	return 0;

error:	if (head) head->lose();
	if (inode) inode->lose();
	ustack.destroy();
	return e;
}
