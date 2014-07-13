#ifndef _LIBQUEUE_H
#define _LIBQUEUE_H

#ifdef __vnode
#error "__vnode is reserved, don't use it"
#endif
#ifdef __nextnode
#error "__nextnode is reserved, don't use it"
#endif
#ifdef __prevnode
#error "__prevnode is reserved, don't use it"
#endif

#define foreach(node, queue) 						\
for (typeof(node) __vnode = (node = (queue).__head(), (queue).vnode()); \
     node != __vnode;							\
     node = (queue).getnext(node))

#define foreachprev(node, queue) 					\
for (typeof(node) __vnode = (node = (queue).__tail(), (queue).vnode()); \
     node != __vnode;							\
     node = (queue).getprev(node))

#define foreachsafe(node, queue) 					\
for (typeof(node) __vnode = (node = (queue).__head(), (queue).vnode()), \
     __nextnode = (queue).getnext(node);					\
     node != __vnode;							\
     node = __nextnode, __nextnode = (queue).getnext(__nextnode))	\

#define foreachprevsafe(node, queue) 					\
for (typeof(node) __vnode = (node = (queue).__tail(), (queue).vnode()), \
     __prevnode = (queue).getprev(node); 				\
     node != __vnode;							\
     node = __prevnode, __prevnode = (queue).getprev(__getprev))	\

#if 0
#define fromlrutomru(node,queue) foreach(node,queue)
#define deqlru deqhead
#define enqmru enqtail
#endif

#define	types <int NEXT, int PREV, class node_t>
#define args <NEXT, PREV, node_t>
/* @node_t - 	class node_t {
			...
			node_t * next;
			node_t * prev;
			...
		}
   @NEXT - 	offsetof(node_t, next)
   @PREV -	offsetof(node_t, prev) */

template types struct queue_tl {
	typedef queue_tl args queue_t;
	node_t * __next, * __prev;

	node_t * __head() { return __next; }
	node_t * __tail() { return __prev; }
	static node_t ** nextpp(node_t * node)
	{
		return (node_t**) ((unsigned long)node + NEXT);
	}
	static node_t ** prevpp(node_t * node)
	{
		return (node_t**) ((unsigned long)node + PREV); 
	}
	static node_t * getnext(node_t * node) { return *nextpp(node); }
	static node_t * getprev(node_t * node) { return *prevpp(node); }
	static void setnext(node_t * node, node_t * next)
	{
		*nextpp(node) = next;
	}
	static void setprev(node_t * node, node_t * prev)
	{
		*prevpp(node) = prev;
	}
	static void link(node_t * a, node_t * b)
	{
		setnext(a, b);
		setprev(b, a);
	}
	static void unlink(node_t * node)
	{
		assert(getnext(node) && getprev(node));
		setprev(getnext(node), getprev(node));
		setnext(getprev(node), getnext(node));
		setnext(node, NULL), setprev(node, NULL);
	}

	node_t * vnode() { return (node_t*)((unsigned long)this - NEXT); }
	queue_tl()
	{
		assert(NEXT + sizeof(node_t*) == PREV);
		__next = __prev = vnode(); 
	}
	void zapall()
	{
		node_t * node;
		foreachsafe (node, *this) {
			unlink(node);
			delete node;
		}
	}
	/* ~queue_tl() { if (DTOR) zapall(); } */
	int count()
	{
		int sum = 0;
		node_t * node;
		foreach (node, *this)
			sum++;
		return sum;
	}
	int empty() { return __next == vnode(); }
	node_t * head() { return empty() ? NULL : __next; }
	node_t * tail() { return empty() ? NULL : __prev; }
	node_t * at(int idx)
	{
		node_t * node;
		foreach (node, *this)
			if (!idx--)
				return node;
		return NULL;
	}
	int index(node_t * target)
	{
		int idx = 0;
		node_t * node;
		foreach (node, *this) {	
			if (node == target)
				return idx; 
			idx++;
		}
		return -1;
	}
	void enqhead(node_t * node)
	{
		assert(!getnext(node) && !getprev(node));
		link(node, __next);
		link(vnode(), node);
	}
	void enqtail(node_t * node)
	{
		assert(!getnext(node) && !getprev(node));
		link(__prev, node);
		link(node, vnode());
	}
	node_t * deqhead()
	{
		if (empty())
			return NULL;
		node_t * node = __next;
		unlink(node);
		return node;
	}
	node_t * deqtail()
	{
		if (empty())
			return NULL;
		node_t * node = __prev;
		unlink(node);
		return node;
	}
	void enqhead(queue_t * q) /* vnode q->next q->prev next */
	{
		if (q->empty())
			return;
		link(q->__prev, __next);
		link(vnode(), q->__next);
		construct(q);
	}
	void enqtail(queue_t * q) /* vnode prev q->next q->prev */ 
	{
		if (q->empty())
			return;
		link(__prev, q->__next);
		link(q->__prev, vnode());
		construct(q);
	}
	void insertlt(node_t * newnode)
	{
		node_t * node;
		foreach (node, *this) {
			if (node_t::lt(newnode, node)) {
				link(node->prev, newnode);
				link(newnode, node);
				return;
			}
		}
		enqtail(newnode);
	}
};

template types class queue2_templ {
	typedef queue_tl args queue_t;
	typedef queue2_templ args queue2_t;

	queue_t queue;
	int count_;

	void increase(int nr = 1) { assert(count_ >= 0); count_ += nr; }
	void decrease(int nr = 1) { count_ -= nr; assert(count_ >= 0); }
public:
	node_t * __head() { return queue.__head(); }
	node_t * __tail() { return queue.__tail(); }
	static node_t ** nextpp(node_t * node)
	{
		return (node_t**) ((unsigned long)node + NEXT);
	}
	static node_t ** prevpp(node_t * node)
	{
		return (node_t**) ((unsigned long)node + PREV); 
	}
	static node_t * getnext(node_t * node) { return *nextpp(node); }
	static node_t * getprev(node_t * node) { return *prevpp(node); }

	node_t * vnode() { return queue.vnode(); }
	queue2_templ() { count_ = 0; }
	void zapall() { queue.zapall(); count_ = 0; }
	int count() { return count_; }
	int empty() { return !count_; }
	node_t * head() { return queue.head(); }
	node_t * tail() { return queue.tail(); }
	node_t * at(int idx) { return queue.at(idx); }
	int index(node_t * node) { return queue.index(node); }
	void enqhead(node_t * node)
	{
		queue.enqhead(node);
		increase();
	}
	void enqtail(node_t * node)
	{
		queue.enqtail(node);
		increase();
	}
	node_t * deqhead()
	{
		decrease();
		return queue.deqhead();
	}
	node_t * deqtail()
	{
		decrease();
		return queue.deqtail();
	}
	void unlink(node_t * node)
	{
		queue.unlink(node);
		increase();
	}
	void enqhead(queue2_t * q2)
	{
		queue.enqhead(&q2->queue);
		increase(q2->count_);
		q2->count_ = 0;
	}
	void enqtail(queue2_t * q2)
	{
		queue.enqtail(&q2->queue);
		increase(q2->count_);
		q2->count_ = 0;
	}
};

#undef types
#undef args 

#undef offsetof
#if (__GNUC__ >= 3 && __GNUC_MINOR__ >= 4) || (__GNUC__ >= 4)
#define offsetof(structure,field) \
(__offsetof__(  (int)(&(static_cast<structure*>(0)->field))  ))
#else
#define offsetof(structure,field) ((int)&((structure*)0)->field)
#endif

#define CHAIN(tag,node_t)							\
	node_t * next##tag, * prev##tag; 					\
	void unlink##tag() {						\
		assert(next##tag && prev##tag);				\
		next##tag->prev##tag = prev##tag;			\
		prev##tag->next##tag = next##tag;			\
		next##tag = prev##tag = NULL;				\
	}								\
	/* prepend "this" object "before" "that" object */		\
	void prepend##tag(node_t * that) {				\
		assert(!next##tag && !prev##tag);			\
	 	assert(that->next##tag && that->prev##tag);		\
		next##tag = that;					\
		prev##tag = that->prev##tag;				\
		next##tag->prev##tag = prev##tag->next##tag = this;	\
	}								\
	/* append "this" object "after" "that" object */		\
	void append##tag(node_t * that) {					\
		assert(!next##tag && !prev##tag);			\
	 	assert(that->next##tag && that->prev##tag);		\
		next##tag = that->next##tag;				\
		prev##tag = that;					\
		next##tag->prev##tag = prev##tag->next##tag = this;	\
	}

#define Q(tag,node_t) _##tag##node_t
#define QUEUE(tag,node_t) typedef queue_tl \
<offsetof(node_t,next##tag), offsetof(node_t, prev##tag), node_t> \
Q(tag,node_t)

#if 0
#define QCTOR(tag,node_t,queue) { \
(Q(tag,node_t)*) (unsigned long)&queue - offsetof(node_t, next##tag), \
(Q(tag,node_t)*) (unsigned long)&queue - offsetof(node_t, prev##tag), }
#endif
#define QCTOR(queue) __ctor(PRIQ, SUBANY, init##queue) { construct(&queue); }

#define CHAIN2(tag,node_t) node_t * next##tag, * prev##tag;
#define QUEUE2(tag,node_t) typedef queue2_templ \
<offsetof(node_t,next##tag), offsetof(node_t, prev##tag), node_t> \
Q2(tag,node_t)
#define Q2(tag,node_t) _##tag##node_t2

#endif
