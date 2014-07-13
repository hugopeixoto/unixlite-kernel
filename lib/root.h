/* root.h contain often used defines, and it _must_ be included 
   in every c++ source file first */
#ifndef	_LIBROOT_H
#define	_LIBROOT_H

#include "types.h"

extern inline void * operator new (size_t size, void * place)
{
	return place;
}

/* call the default constructor */
template <class object_t>
extern inline void construct(object_t * ptr)
{
	new (ptr) object_t();
}

template <class object_t>
extern inline void construct(object_t * ptr, int nr)
{
	for (object_t * end = ptr + nr; ptr < end; ptr++)
		new (ptr) object_t();
}

template <class object_t>
extern inline void dispose(object_t ** ptr)
{
	delete *ptr;
	*ptr = 0;
}

#define	NULL 0
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)1)->MEMBER - 1)

#define DEBUG 1
extern int printf(const char * fmt, ...)__attribute__((format(printf,1,2)));
extern void tracefunc();
#define trace(...) \
do {	printf(__VA_ARGS__); \
	printf(" at %s:%d:%s ", __FILE__, __LINE__, __func__); \
 	tracefunc(); } while (0)
#define warn(...) do { printf("warning:"); trace(__VA_ARGS__); } while (0)
#define panic(...) do { printf("panic:"); trace(__VA_ARGS__); for(;;);} while (0)

#define allege(expr) \
if (!(expr)) \
	do { 	printf("assert \"%s\" fail ", #expr); \
		trace(" "); for(;;); } while(0)
#if DEBUG
#define assert(expr) allege(expr)
#define printd(...) printf(__VA_ARGS__)
#else
#define	assert(expr)
#define printd(...) 
#endif

#define debug(flag, ...) if (flag) { printf("%s:%d:", __FILE__, __LINE__); printf(__VA_ARGS__); }

extern int globalvar;

#include "math.h"


#endif
