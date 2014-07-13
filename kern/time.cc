#include <lib/root.h>
#include <lib/gcc.h>
#include <lib/errno.h>
#include <init/ctor.h>
#include <asm/system.h>
#include <asm/io.h>
#include "sched.h"
#include "time.h"

time_t boottime;
tick_t passedtick;

/* day of month in leap year */
static int dayofl[] = {0, 31,29,31,30, 31,30,31,31, 30,31,30,31};
/* day of month in non-leap year */
static int dayofn[] = {0, 31,28,31,30, 31,30,31,31, 30,31,30,31};
#define LIFE 100
static int dayofyear[1+LIFE];

static int leap(int y)
{
	return ((y%4==0) && (y%100!=0)) || (y%400==0);
}

/* assume n > 0 */
static void add(int * p, int n)
{	
	for (int * target = p + n - 1; target > p; target--)
		for (int * source = p; source < target; source++)
			*target += *source;
}

static void show(int * p, int n)
{
	while (n--)
		printf("%d ", *p++);
	printf("\n");
}

static void calcdayofyear()
{
	add(dayofl,13);
	assert(dayofl[12] == 366);
	add(dayofn,13);
	assert(dayofn[12] == 365);
	for (int i = 1; i < LIFE+1; i++)
		dayofyear[i] = leap(1970 + i) ? dayofl[12] : dayofn[12];
	add(dayofyear,LIFE+1);
} 

/* hour/minute/second count from 0, month/dayofmonth count from 1 */
static int dayfrom1970(tm_t * t)
{
	return dayofyear[t->year - 1970] + 
		(leap(t->year) ? dayofl[t->mon - 1] : dayofn[t->mon - 1]) +
		t->mday - 1;
}

static time_t timefrom1970(tm_t * t)
{
	return dayfrom1970(t) * 24 * 3600 +
		(t->hour) * 3600 + (t->min) * 60 + (t->sec);
}

static int readcmos(int addr)
{
	outbp(0x80|addr, 0x70);
	int i = inbp(0x71);
	return (i & 15) + (i >> 4) * 10; 
}

#include <asm/io.h>
#define LATCH (1193180/CLKFREQ)
__ctor(PRIKERN,SUBANY,inittime)
{
	tm_t t;

	t.sec = readcmos(0);
	t.min = readcmos(2);
	t.hour = readcmos(4);
	t.mday = readcmos(7);
	t.mon = readcmos(8);
	if (!between(1, t.mon, 13)) {
		warn("cmos error:incorrect month:%d\n", t.mon);
		t.mon = 1;
	}
	t.year = readcmos(9);
	t.year += (t.year > 70) ? 1900 : 2000;
	if (!between(1970, t.year , 1970 + LIFE)) {
		warn("cmos error:incorrect year:%d\n", t.year);
		t.year = 1970;
	}
	printf("current time is %d/%d/%d %d:%d:%d\n", 
	t.year, t.mon, t.mday, t.hour, t.min, t.sec);

	calcdayofyear();
	boottime = timefrom1970(&t);

	outbp(0x36, 0x43); /* binary, mode 3, LSB/MSB, ch 0 */
	outbp(LATCH & 0xff, 0x40); /* LSB */
	outbp(LATCH >> 8, 0x40); /* MSB */
}

int readlatch()
{
	int count = inbp(0x40);
	count += inbp(0x40) << 8;
	return count;
}

ulong passedusec()
{
	return passedtick * 10000 + (LATCH - readlatch()) * 10000 / LATCH;
}

asmlinkage int systime(time_t * t)
{
	if (t) {
		int e;
		if (e = verw(t, sizeof(time_t)))
			return e;
		*t = now();
	}
	return now();
}

asmlinkage int sysstime(time_t * t)
{
	if (!suser())
		return EPERM;
	int e = verr(t , sizeof(time_t*));
	if (e)
		return e;
	boottime = *t - passedtick / CLKFREQ;
	return 0;
}

asmlinkage int systimes(tms_t * t)
{
	if (t) {
		int e = verw(t, sizeof(*t));
		if (e)
			return e;
		t->utick = curr->utick;
		t->stick = curr->stick;
		t->cutick = curr->cutick;
		t->cstick = curr->cstick;
	}
	return passedtick;
}

asmlinkage int sysgettimeofday(timeval_t * tv, timezone_t * tz)
{
	int e;

	if (tv) {
		if (e = verw(tv, sizeof(*tv)))
			return e;
		tv->sec = now(); 
		tv->usec = (passedtick % CLKFREQ) * 10000; /* 10ms == 10000us */
	}
	if (tz) {
		if (e = verw(tz, sizeof(*tz)))
			return e;
		memset(tz, 0, sizeof(*tz));
		tz->minuteswest = -480; /* CST */
	}
	return 0;
}

asmlinkage int syssettimeofday()
{
	printf("SET TIME NOT YET\n");
	return 0;
}
