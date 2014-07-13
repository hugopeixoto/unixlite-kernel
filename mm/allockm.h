#ifndef _MMALLOCKM_H
#define _MMALLOCKM_H

#include <lib/queue.h>
#include <lib/math.h>
#include "allocpage.h"

struct pool_t;

struct kmobj_t {
	kmobj_t * next;
	long longs[0];

	void markmagic(int objsize);
	void checkmagic(int objsize);
};

struct bucket_t {
	CHAIN(,bucket_t);
	pool_t * pool;
	int objsize;
	kmobj_t * freeobj;
	short ncurfree;
	short nmaxfree;
	kmobj_t objs[0];

	void init(pool_t * pool_);
	void * alloc();
	void free(void * p);
	void check(kmobj_t * o);
};
QUEUE(,bucket_t);
typedef Q(,bucket_t) bucketq_t;

/* object pool */
struct pool_t {
	CHAIN(,pool_t);
	char * name;
	int objsize;
	bucketq_t richq; /* circluar list of buckets that has free buf */
	bucketq_t poorq; /* circluar list of buckets that has no free buf*/

	void * operator new (size_t size) { return allocbm(size); }
	pool_t(char * name, int objsize);
	void * alloc(int flags);
	void free(void * p);
	int alloced();
};
QUEUE(,pool_t);


#define ALLOCKMMAX (PAGESIZE - sizeof(bucket_t))
extern void allockminit();
extern void * allockm(int flags, int size);
extern void freekm(void * ptr);

#endif
