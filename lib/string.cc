#include <lib/root.h>
#include <init/ctor.h>
#include <mm/allocpage.h>
#include "string.h"

extern "C" char * strcpy(char * dst, const char *src)
{
        char *ret = dst;
        while (*dst++ = *src++)
                ;
        return ret;
}

extern "C" char * strncpy(char *dst, const char *src, size_t n)
{
        char *ret = dst;
        for (; n--; )
                if (!(*dst++ = *src++))
                        return ret;
        return ret;
}

extern "C" char * strcat(char *dst, const char * src)
{
        char *ret = dst;
        while (*dst++)
                ;
        dst--;
        while (*dst++ = *src++)
                ;
        return ret; 
}

int strlen(const char *s)
{
        const char *t = s;
        while (*t++)
                ;
        return t - 1 - s;
}

extern "C" char * strchr(const char *s, int c)
{
        for (; *s; s++)
                if (*s == c)
                        return (char*)s;
        return NULL;
}

extern "C" char * strrchr(const char *s, int c)
{
        char * r = (char*)s;
        while (*r++)
                ;
        r -= 2;
        for (; r >= s; r--)
                if (*r == c)
                        return r;
        return NULL;
}

extern "C" char * strncat(char *dst, const char * src, size_t n)
{
        allege(0);
        return NULL;
}

extern "C" int strcmp(const char *src, const char * dst)
{
        while (*src && *dst && *src == *dst)
                src++, dst++;
        return *src - *dst;
}

extern "C" int strncmp(const char *src, const char * dst, size_t n)
{
        allege(0);
        return 0;
}

extern "C" void * memmove(void * dst, const void * src, size_t n)
{
        void * ret = dst;
	char *d = (char*) dst;
	char *s = (char*) src;

        if (d <= s) {
                while (n--)
                        *d++ = *s++;
                return ret;
        }
        d += n;
        s += n;
        while (n--)
                *--d = *--s; 
        return ret;
}

extern "C" void memset(void *ptr, int v, size_t n)
{
	char *p = (char*) ptr;
        while (n--)
                *p++ = v;
}

extern "C" void memcpy(void *dst, const void *src, size_t n)
{
	char *d = (char*) dst;
	char *s = (char*) src;

        while (n--)
                *d++ = *s++;
}

static int toknum(char * str, const char * sep)
{
	int state = 0, sum = 0;
	for (; *str; str++) {
		char * issep = strchr(sep, *str);
		if (issep)
			*str = 0;
		if (state == 0) {
			if (!issep)
				sum++, state = 1;
		} else {
			if (issep)
				state = 0;
		}
	}
	return sum;
}

scanstr_t::scanstr_t(const char * str, const char * sep)
{
	int len = strlen(str);
	if (len < SIZE) {
		bigroom = NULL;
		cur = smallroom;
		eroom = smallroom + len;
	} else if (len < PAGESIZE) {
		bigroom = (char*) allocpage(AKERN);
		cur = bigroom;
		eroom = bigroom + len;
	} else {
		warn("TOO LONG:%s", str);
		return;
	}
	strcpy(cur, str);
	nlefttok_ = toknum(cur, sep);
	skipsep();
}

scanstr_t::~scanstr_t()
{
	if (bigroom)
		freepage(bigroom);
}

scanstrprev_t::scanstrprev_t(const char * str, const char * sep)
{
	int len = strlen(str);
	char * c;
	if (len < SIZE) {
		bigroom = NULL;
		eroom = smallroom - 1;
		cur = smallroom + len;
		c = smallroom;
	} else if (len < PAGESIZE) {
		bigroom = (char*) allocpage(AKERN);
		eroom = bigroom - 1;
		cur = bigroom + len;
		c = bigroom;
	} else {
		warn("TOO LONG:%s", str);
		return;
	}
	strcpy(c, str);
	nlefttok_ = toknum(c, sep);
	skipsep();	
	movetotokhead();
}

scanstrprev_t::~scanstrprev_t()
{
	if (bigroom)
		freepage(bigroom);
}

#if 0
__ctor(PRIKERN, SUBANY, jf)
{
	char * tok; 

	char * test = "::/bin:/sbin::/usr/bin:";
	foreachtok(tok, test, ":")
		printf("count==%d, tok==%s\n", scan.nlefttok(), tok);
	scanstrprev_t s(test, ":");
	while (s.more()) {
		tok = s.curtok();
		printf("count==%d, tok==%s\n", s.nlefttok(), tok);
		s.prev();
	}
}
#endif
