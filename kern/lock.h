#ifndef _KERNLOCK_H
#define _KERNLOCK_H

#ifndef _KERNSCHED_H
#error "please don't include sync.h directly"
#endif

typedef Q(run,task_t) runtaskq_t;
class waitq_t : public runtaskq_t {
	void waitsleep(int state);
	int waitsleep(int state, int ntick);

public: void sleep()
	{
		waitsleep(TSLEEP); 
	}
	void wait() 
	{
		if (!curr->hassig()) 
			waitsleep(TWAIT); 
	}
	void stop()
	{
		waitsleep(TSTOP); 
	}
	void zombie() 
	{
		waitsleep(TZOMBIE); 
	}
	int sleep(int ntick) 
	{
		return waitsleep(TSLEEP, ntick); 
	}
	int wait(int ntick)
	{ 
		if (!curr->hassig())
			return waitsleep(TWAIT, ntick); 
		return 0;
	}
	void signal();
	void broadcast();
	~waitq_t() { broadcast(); }
};
#define WAIT(waitq) do { (waitq).wait(); if (curr->hassig()) return EINTR; } while (0)

class lock_t {
	volatile int locked_;
	waitq_t waitq;
	task_t * lockowner;

public: lock_t() { locked_ = 0; lockowner = NULL; }
	int locked() { return locked_; }
	void lock();
	void unlock();
};

class iolock_t { 
	volatile int locked_;
	waitq_t waitq;

public: iolock_t() { locked_ = 0; } 
	int locked() { return locked_; }
	void lock();
	void unlock();
	void wait();
};

#define IOLOCK \
	iolock_t iolock; \
	int locked() { return iolock.locked(); } \
	void lock() { iolock.lock(); } \
	void unlock() { iolock.unlock(); } \
	void wait() { iolock.wait(); }

class rwlock_t {
	enum state_t {
		FREE,
		READING,
		WRITING,
	}; 
	volatile state_t state;
	int nreader;
	waitq_t waitq;
	task_t * lockowner;

public: rwlock_t() { state = FREE; nreader = 0; lockowner = NULL; }
	int locked() { return state != FREE; }
	void rlock();
	void wlock();
	void unlock();
};

class sema_t {
	volatile int curcnt;
	int maxcnt;
	waitq_t	waitq;

public: sema_t(int n) { curcnt = maxcnt = n; }
	int wouldblock(int n = 1) /* down operation would block */
	{
		return (curcnt - n) < 0;
	}
	void down(int n = 1);
	void up(int n = 1);
};

#endif
