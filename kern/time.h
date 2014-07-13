#ifndef _KERNTIME_H
#define _KERNTIME_H

/* today's machine is much more faster than ten years ago */
#define CLKFREQ 100 
#define _1s CLKFREQ
#define _100ms (_1s/10) 
#define _10ms (_1s/100)

extern time_t boottime; /* measured in second */
extern tick_t passedtick; /* the number of ticks passed away since system boot */
extern inline time_t now()
{
	return boottime + (passedtick / CLKFREQ);
}

/* convert second into tick */
extern inline tick_t timetotick(time_t time)
{
	return time * CLKFREQ;
}
extern inline tick_t time1totick(time_t time1)
{
	return time1 * (CLKFREQ / 10); /* (time1 / 10) * CLKFREQ */
}
/* convert mini-second into tick */
extern inline tick_t time3totick(time_t time3)
{
	return time3 / (1000 / CLKFREQ); /* (time3 / 1000) * CLKFREQ */
}
extern inline time_t ticktotime(tick_t ntick)
{
	return ntick / CLKFREQ;
}
extern inline time_t ticktotime3(tick_t ntick)
{
	return ntick * (1000 / CLKFREQ); /* ntick / CLKFREQ * 1000 */
}
extern inline time_t ticktotime1(tick_t ntick)
{
	return ntick / (CLKFREQ / 10); /* ntick / CLKFREQ * 10 */
}

struct tms_t {
	tick_t utick;
	tick_t stick;
	tick_t cutick;
	tick_t cstick;
};

struct timeval_t {
	long	sec;		/* seconds */
	long	usec;		/* microseconds */
};

struct timezone_t {
	int	minuteswest;	/* minutes west of Greenwich */
	int	dsttime;	/* type of dst correction */
};

/* Used by other time functions.  */
struct tm_t {
  int sec;			/* Seconds.	[0-60] (1 leap second) */
  int min;			/* Minutes.	[0-59] */
  int hour;			/* Hours.	[0-23] */
  int mday;			/* Day.		[1-31] */
  int mon;			/* Month.	[0-11] */
  int year;			/* Year	- 1900.  */
  int wday;			/* Day of week.	[0-6] */
  int yday;			/* Days in year.[0-365]	*/
  int isdst;			/* DST.		[-1/0/1]*/
#if 0
  long int gmtoff;		/* Seconds east of UTC.  */
  __const char *zone;	/* Timezone abbreviation.  */
#endif
};

#endif
