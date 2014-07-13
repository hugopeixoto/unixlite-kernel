#ifndef _I386_STRING_H_
#define _I386_STRING_H_

extern "C" char * strcpy(char * dst, const char *src);
extern "C" char * strncpy(char *dst, const char *src, size_t n);
extern "C" char * strcat(char *dst, const char * src);
extern "C" int strlen(const char *s);
extern "C" char * strchr(const char *s, int c);
extern "C" char * strrchr(const char *s, int c);
extern "C" char * strncat(char *dst, const char * src, size_t n);
extern "C" int strcmp(const char *src, const char * dst);
extern "C" int strncmp(const char *src, const char * dst, size_t n);
extern "C" void * memmove(void * dst, const void * src, size_t n);
extern "C" void memset(void *p, int v, size_t n);
extern "C" void memcpy(void *dst, const void *src, size_t n);
extern inline void bzero(void *p, int n)
{
        memset(p, 0, n);
}

template <int SIZE> class str_tl {
	char room[SIZE];
public:
	char * getraw() { return room; }
	char * get() { room[SIZE-1] = 0;  return room; }
	void set(const char * name) { strncpy(room, name, SIZE); }
};

class scanstr_t {
	enum { SIZE = 256 };
	char smallroom[SIZE];
	char * bigroom;
	char * eroom;
	char * cur;
	int nlefttok_;

	void skiptok()
	{
		while ((cur < eroom) && *cur)
			cur++;
	}
	void skipsep()
	{
		while ((cur < eroom) && !*cur)
			cur++;
	}
public: scanstr_t(const char * str, const char * sep);
	~scanstr_t();
	int more() { return cur < eroom;  }
	char * curtok() { return cur; }
	void next() { skiptok(); skipsep(); nlefttok_--; }
	int nlefttok() { return nlefttok_; }
};

#define foreachtok(tok, str, sep) \
for (scanstr_t scan(str, sep); tok = scan.curtok(), scan.more(); scan.next()) 

class scanstrprev_t {
	enum { SIZE = 256 };
	char smallroom[SIZE];
	char * bigroom;
	char * eroom;
	char * cur;
	int nlefttok_;

	int istok(char * p) { return (p > eroom) && *p; }
	void movetotokhead()
	{
		while (istok(cur) && istok(cur-1))
			cur--;
	}
	void skipsep()
	{
		while ((cur > eroom) && !*cur)
			cur--;
	}

public: scanstrprev_t(const char * str, const char * sep);
	~scanstrprev_t();
	int more() { return cur > eroom; }
	char * curtok() { return cur; }
	void prev() { cur--; skipsep(); movetotokhead(); nlefttok_--; }
	int nlefttok() { return nlefttok_; }
};

#endif
