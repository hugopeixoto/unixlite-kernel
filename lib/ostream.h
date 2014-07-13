#ifndef _LIBOSTREAM_H
#define _LIBOSTREAM_H

class ostream_t {
	char * room, * eroom;
	char * cursor;

public:	ostream_t(char * buf, int size)
	{
		cursor = room = buf;
		eroom = buf + size;
	}
	void write(char * fmt, ...);
	int written() { return cursor - room; }
};
#endif
