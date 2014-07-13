#include <lib/root.h>
#include <lib/errno.h>
#include <lib/gcc.h>
#include <kern/sched.h>
#include "pgrp.h"

/*
 * This is done BSD-style, with no consideration of the saved gid, except
 * that if you set the effective gid, it sets the saved gid too.  This 
 * makes it possible for a setgid program to completely drop its privileges,
 * which is often a useful assertion to make when you are doing a security
 * audit over a program.
 *
 * The general idea is that a program which uses just setregid() will be
 * 100% compatible with BSD.  A program which uses just setgid() will be
 * 100% compatible with POSIX w/ Saved ID's. 
 */
asmlinkage int syssetregid(gid_t rgid, gid_t egid)
{
	int old_rgid = curr->gid;

	if (rgid != (gid_t) -1) {
		if ((curr->egid==rgid) ||
		    (old_rgid == rgid) || 
		    suser())
			curr->gid = rgid;
		else
			return(EPERM);
	}
	if (egid != (gid_t) -1) {
		if ((old_rgid == egid) ||
		    (curr->egid == egid) ||
		    suser()) {
			curr->egid = egid;
			curr->sgid = egid;
		} else {
			curr->gid = old_rgid;
			return(EPERM);
		}
	}
	return 0;
}

/*
 * setgid() is implemeneted like SysV w/ SAVED_IDS 
 */
asmlinkage int syssetgid(gid_t gid)
{
	if (suser())
		curr->gid = curr->egid = curr->sgid = gid;
	else if ((gid == curr->gid) || (gid == curr->sgid))
		curr->egid = gid;
	else
		return EPERM;
	return 0;
}

/*
 * Unprivileged users may change the real user id to the effective uid
 * or vice versa.  (BSD-style)
 *
 * When you set the effective uid, it sets the saved uid too.  This 
 * makes it possible for a setuid program to completely drop its privileges,
 * which is often a useful assertion to make when you are doing a security
 * audit over a program.
 *
 * The general idea is that a program which uses just setreuid() will be
 * 100% compatible with BSD.  A program which uses just setuid() will be
 * 100% compatible with POSIX w/ Saved ID's. 
 */
asmlinkage int syssetreuid(uid_t ruid, uid_t euid)
{
	int old_ruid = curr->uid;
	
	if (ruid != (uid_t) -1) {
		if ((curr->euid==ruid) ||
		    (old_ruid == ruid) ||
		    suser())
			curr->uid = ruid;
		else
			return(EPERM);
	}
	if (euid != (uid_t) -1) {
		if ((old_ruid == euid) ||
		    (curr->euid == euid) ||
		    suser()) {
			curr->euid = euid;
			curr->suid = euid;
		} else {
			curr->uid = old_ruid;
			return(EPERM);
		}
	}
	return 0;
}

/*
 * setuid() is implemeneted like SysV w/ SAVED_IDS 
 * 
 * Note that SAVED_ID's is deficient in that a setuid root program
 * like sendmail, for example, cannot set its uid to be a normal 
 * user and then switch back, because if you're root, setuid() sets
 * the saved uid too.  If you don't like this, blame the bright people
 * in the POSIX commmittee and/or USG.  Note that the BSD-style setreuid()
 * will allow a root program to temporarily drop privileges and be able to
 * regain them by swapping the real and effective uid.  
 */
asmlinkage int syssetuid(uid_t uid)
{
	if (suser())
		curr->uid = curr->euid = curr->suid = uid;
	else if ((uid == curr->uid) || (uid == curr->suid))
		curr->euid = uid;
	else
		return EPERM;
	return(0);
}

asmlinkage int sysgetpid(void)
{
	return curr->pid;
}

asmlinkage int sysgetppid(void)
{
	return curr->parent->pid;
}

asmlinkage int sysgetuid(void)
{
	return curr->uid;
}

asmlinkage int sysgeteuid(void)
{
	return curr->euid;
}

asmlinkage int sysgetgid(void)
{
	return curr->gid;
}

asmlinkage int sysgetegid(void)
{
	return curr->egid;
}

asmlinkage int sysgetpgrp(void)
{
	return curr->pgrp->pgid;
}
