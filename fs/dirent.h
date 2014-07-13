#ifndef _FSDIRENT_H
#define _FSDIRENT_H

#include "minix.h"
#include "buf.h"
struct inode_t;

class scandir_t {
	enum { READA = 4 };
	inode_t * dir;
	buf_t * b;	/* current buffer */
	off_t off;
	int create;

	void advance();
public: scandir_t(inode_t * dir_, int create_ = 0, off_t off_ = 0);
	~scandir_t();
	int more() { return off < dir->size; }
	void next();
	minixde_t * curde() 
	{
		assert(b); 
		return (minixde_t*)(b->data + (off & (MINIXBSIZE - 1))); 
	}
	buf_t * curb() { return b; }
	off_t curoff() { return off; }
};

struct dirent_t {
	long ino;
	long off;
	short reclen;
	char name[0];

	dirent_t * next() { return (dirent_t*)((char*)this + reclen); }
};

class destream_t {
	char * room, * eroom;
	dirent_t * cursor;

public:	destream_t(dirent_t * de, int nbyte)
	{
		room = (char*) de;
		eroom = room + nbyte;
		cursor = de;
	}
	int write(ino_t ino_, off_t off_, char * name);
	int written() { return (char*)cursor - room; }
};

#endif
