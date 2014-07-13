#include <lib/root.h> 
#include "ctor.h"

#if 1
#define debug(...)
#else
#define debug(...) printf(__VA_ARGS__)
#endif

typedef void (*ctor_t) ();
ctor_t __ctorlist0106[MAXCTOR] __attribute__((section("data")));

void doglobalctors()
{
	ctor_t * ctor = __ctorlist0106, * end = __ctorlist0106 + MAXCTOR;

	if (*ctor != (ctor_t) 0x19790106)
		panic("head magic not found, check collect\n");
	debug("BEGIN DO GLOBAL CTORS\n");
	int i = 0;
	for (ctor++; ctor < end; i++, ctor++) {
		if (*ctor == (ctor_t) 0x19790106) {
			debug("END DO GLOBAL CTORS\n");
			return;
		}
		debug("NO.%d %8lx\n", i, **ctor);
		(**ctor)();
	}
	panic("trailing magic not found, check collect\n");
}
