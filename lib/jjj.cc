#include <lib/root.h>
#include <kern/sched.h>

#define b(n) ((ulong)__builtin_return_address(n))
void tracefunc()
{
	if (booting)
		printf("in boottime ");
	else
		printf("in %s(pid=%d) ", curr->execname.get(), curr->pid);
	//printf("return:%lx\n", b(1));
	//printf("return: %lx %lx %lx\n", b(1), b(2), b(3));
}
