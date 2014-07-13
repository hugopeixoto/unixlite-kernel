#include <lib/gcc.h>
#include <lib/unistd.h>
#include <lib/errno.h>
#include "frame.h"
#include "seg.h" /* for KCODESEL */

#define BODY(trapno) \
	PUSHALL; \
	LOADSEG; \
	call do##trap##trapno; \
	cmpl $KCODESEL,CS(%esp); \
	je 1f; \
	call rettouser; \
1:	POPALL; \
	addl $4,%esp; \
	iret

#define ERROR(trapno) \
	ENTRY(trap##trapno); \
	BODY(trapno)

#define NOERR(trapno) \
	ENTRY(trap##trapno); \
	pushl $0; \
	BODY(trapno)

.text
NOERR(0x00)
NOERR(0x01)
NOERR(0x02)
NOERR(0x03)
NOERR(0x04)
NOERR(0x05)
NOERR(0x06)
NOERR(0x07)
ERROR(0x08)
ERROR(0x09)
ERROR(0x0a)
ERROR(0x0b)
ERROR(0x0c)
ERROR(0x0d)
ERROR(0x0e)
ERROR(0x0f)
NOERR(0x10)
ERROR(0x11)
ERROR(0x12)

	.text
	.globl rettouser
ENTRY(syscallentry)
	pushl %eax	/* save original eax */
	PUSHALL
	cmp $__NR_end,%eax
	jb 1f
	movl $ENOSYS,EAX(%esp)
	jmp 2f
1:	call *syscalltab(,%eax,4)
	movl %eax,EAX(%esp)
2:	call rettouser
	POPALL
	addl $4,%esp	/* skip original eax */
	iret

#define SYS(name) .long sys##name
#define XXX(name) .long sysnosys

ENTRY(syscalltab)
	SYS(setup)		/* 0 */
	SYS(exit)
	SYS(fork)
	SYS(read)
	SYS(write)
	SYS(open)		/* 5 */
	SYS(close)
	SYS(waitpid)
	SYS(creat)
	SYS(link)
	SYS(unlink)		/* 10 */
	SYS(execve)
	SYS(chdir)
	SYS(time)
	SYS(mknod)
	SYS(chmod)		/* 15 */
	XXX(lchown)
	XXX(break)
	XXX(oldstat)
	SYS(lseek)
	SYS(getpid)		/* 20 */
	SYS(mount)
	SYS(umount)
	SYS(setuid)
	SYS(getuid)
	SYS(stime)		/* 25 */
	SYS(ptrace)
	SYS(alarm)
	XXX(oldfstat)
	SYS(pause)
	SYS(utime)		/* 30 */
	XXX(stty)
	XXX(gtty)
	SYS(access)
	SYS(nice)
	XXX(ftime)		/* 35 */
	SYS(sync)
	SYS(kill)
	SYS(rename)
	SYS(mkdir)
	SYS(rmdir)		/* 40 */
	SYS(dup)
	SYS(pipe)
	SYS(times)
	XXX(prof)
	SYS(brk)		/* 45 */
	SYS(setgid)
	SYS(getgid)
	SYS(signal)
	SYS(geteuid)
	SYS(getegid)		/* 50 */
	XXX(acct)
	XXX(phys)
	XXX(lock)
	SYS(ioctl)
	SYS(fcntl)		/* 55 */
	XXX(mpx)
	SYS(setpgid)
	XXX(ulimit)
	XXX(oldolduname)
	SYS(umask)		/* 60 */
	SYS(chroot)
	XXX(ustat)
	SYS(dup2)
	SYS(getppid)
	SYS(getpgrp)		/* 65 */
	SYS(setsid)
	SYS(sigaction)
	SYS(sgetmask)
	SYS(ssetmask)
	SYS(setreuid)		/* 70 */
	SYS(setregid)
	SYS(sigsuspend)
	SYS(sigpending)
	XXX(sethostname)
	SYS(setrlimit)		/* 75 */
	SYS(getrlimit)
	SYS(getrusage)
	SYS(gettimeofday)
	SYS(settimeofday)
	XXX(getgroups)		/* 80 */
	XXX(setgroups)
	XXX(select)
	XXX(symlink)
	XXX(oldlstat)
	XXX(readlink)		/* 85 */
	XXX(uselib)
	XXX(swapon)
	XXX(reboot)
	XXX(readdir)		/* obsolete */
	XXX(mmap)		/* 90 */
	XXX(munmap)
	XXX(truncate)
	XXX(ftruncate)
	XXX(fchmod)
	XXX(fchown)		/* 95 */
	XXX(getpriority)
	XXX(setpriority)
	XXX(profil)
	XXX(statfs)
	XXX(fstatfs)		/* 100 */
	XXX(ioperm)
	SYS(socketcall)
	XXX(syslog)
	XXX(setitimer)
	XXX(getitimer)		/* 105 */
	SYS(stat)
	SYS(lstat)
	SYS(fstat)
	XXX(olduname)
	XXX(iopl)		/* 110 */
	XXX(vhangup)
	XXX(idle)
	XXX(vm86old)
	SYS(wait4)
	XXX(swapoff)		/* 115 */
	XXX(sysinfo)
	XXX(ipc)
	XXX(fsync)
	SYS(sigreturn)
	XXX(clone)		/* 120 */
	XXX(setdomainname)
	SYS(uname)
	XXX(modify_ldt)
	XXX(adjtimex)
	XXX(mprotect)		/* 125 */
	SYS(sigprocmask)
	XXX(create_module)
	XXX(init_module)
	XXX(delete_module)
	XXX(get_kernel_syms)	/* 130 */
	XXX(quotactl)
	XXX(getpgid)
	SYS(fchdir)
	XXX(bdflush)
	XXX(sysfs)		/* 135 */	
	SYS(personality)
	XXX(afs_syscall)
	XXX(setfsuid)
	XXX(setfsgid)
	XXX(llseek)		/* 140 */
	SYS(getdents)		/* uclibc need this */
	XXX(142)
	XXX(143)
	XXX(144)
	XXX(145)
	XXX(146)
	XXX(147)
	XXX(148)
	XXX(149)
	XXX(150)
	XXX(151)
	XXX(152)
	XXX(153)
	XXX(154)
	XXX(155)
	XXX(156)
	XXX(157)
	XXX(158)
	XXX(159)
	XXX(160)
	XXX(161)
	XXX(162)
	XXX(163)
	XXX(164)
	XXX(165)
	XXX(166)
	XXX(167)
	XXX(168)
	XXX(169)
	XXX(170)
	XXX(171)
	XXX(172)
	XXX(173)
	SYS(sigaction)     /* rt_sigaction */
	SYS(sigprocmask)   /* rt_sigprocmask */
	XXX(176)
	XXX(177)
	XXX(178)
	XXX(179)
	XXX(180)
	XXX(181)
	SYS(chown)
	SYS(getcwd)
	XXX(184)
	XXX(185)
	XXX(186)
	XXX(187)
	XXX(188)
	XXX(189)
	SYS(fork)	/* gcc, system(3) need vfork */
	XXX(191)
	XXX(192)
	XXX(193)
	XXX(194)
	SYS(stat64)
	XXX(196)
	XXX(197)
