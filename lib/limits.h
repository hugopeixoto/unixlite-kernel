#ifndef _LIBLIMITS_H
#define _LIBLIMITS_H

#define NGROUPMAX        32	/* supplemental group IDs are available */
#define ARGMAX       131072	/* # bytes of args + environ for exec() */
#define CHILDMAX        999     /* no limit :-) */
#define OPENMAX         256	/* # open files a process may have */
#define LINKMAX         127	/* # links a file may have */
#define MAXCANON        255	/* size of the canonical input queue */
#define MAXINPUT        255	/* size of the type-ahead buffer */
#define NAMEMAX         255	/* # chars in a file name */
#define PATHMAX        1024	/* # chars in a path name */
#define PIPEBUF        4096	/* # bytes in atomic write to a pipe */

#endif
