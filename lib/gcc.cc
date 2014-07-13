#include <lib/root.h>
#include <mm/allockm.h>
#include "gcc.h"

int globalvar;
void * operator new (size_t size)
{
	return allockm(AKERN, size);
}

void operator delete (void * ptr)
{
	freekm(ptr);
}

void * operator new[] (size_t size)
{
	panic("don't overload new[]\n");
	return NULL;
}

void operator delete[] (void * ptr)
{
	panic("don't overload delete[]\n");
}

asmlinkage void __pure_virtual()
{
	panic("pure virtual function called\n"); 
}

asmlinkage void __cxa_pure_virtual()
{
	panic("pure virtual function called\n"); 
}

asmlinkage void __cxa_atexit()
{
	panic("__at_exit called\n"); 
}

asmlinkage void __dso_handle()
{
	panic("__dso_handle called\n");
}

void checkoffsetof()
{
}
