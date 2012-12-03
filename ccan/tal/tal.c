/* Licensed under BSD-MIT - see LICENSE file for details */
#include <ccan/tal/tal.h>
#include <ccan/compiler/compiler.h>
#include <ccan/list/list.h>
#include <ccan/take/take.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

//#define TAL_DEBUG 1

/* 32-bit type field, first byte 0 in either endianness. */
enum prop_type {
	CHILDREN = 0x00c1d500,
	DESTRUCTOR = 0x00de5700,
	NAME = 0x00111100,
};

struct tal_hdr {
	struct list_node list;
	struct prop_hdr *prop;
	struct children *parent_child;
};

struct prop_hdr {
	enum prop_type type;
	struct prop_hdr *next;
};

struct children {
	struct prop_hdr hdr; /* CHILDREN */
	struct tal_hdr *parent;
	struct list_head children; /* Head of siblings. */
};

struct destructor {
	struct prop_hdr hdr; /* DESTRUCTOR */
	void (*destroy)(void *me);
};

struct name {
	struct prop_hdr hdr; /* NAME */
	char name[];
};

static struct {
	struct tal_hdr hdr;
	struct children c;
} null_parent = { { { &null_parent.hdr.list, &null_parent.hdr.list },
		    &null_parent.c.hdr, NULL },
		  { { CHILDREN, NULL },
		    &null_parent.hdr,
		    { { &null_parent.c.children.n,
			&null_parent.c.children.n } }
		  }
};


static void *(*allocfn)(size_t size) = malloc;
static void *(*resizefn)(void *, size_t size) = realloc;
static void (*freefn)(void *) = free;
static void (*errorfn)(const char *msg) = (void *)abort;

static inline void COLD call_error(const char *msg)
{
	errorfn(msg);
}

static bool get_destroying_bit(struct children *parent_child)
{
	return (size_t)parent_child & 1;
}

static void set_destroying_bit(struct children **parent_child)
{
	*parent_child = (void *)((size_t)*parent_child | 1);
}

static struct children *ignore_destroying_bit(struct children *parent_child)
{
	return (void *)((size_t)parent_child & ~(size_t)1);
}

static bool initialized = false;

/* This means valgrind can see leaks. */
static void tal_cleanup(void)
{
	struct tal_hdr *i;

	while ((i = list_top(&null_parent.c.children, struct tal_hdr, list)))
		list_del(&i->list);

	/* Cleanup any taken pointers. */
	take_cleanup();
}

/* For allocation failures inside ccan/take */
static void take_alloc_failed(const void *p)
{
	tal_free(p);
}

/* We carefully start all real properties with a zero byte. */
static bool is_literal(const struct prop_hdr *prop)
{
	return ((char *)prop)[0] != 0;
}

#ifndef NDEBUG
static const void *bounds_start, *bounds_end;

static void update_bounds(const void *new, size_t size)
{
	if (unlikely(!bounds_start)) {
		bounds_start = new;
		bounds_end = (char *)new + size;
	} else if (new < bounds_start)
		bounds_start = new;
	else if ((char *)new + size > (char *)bounds_end)
		bounds_end = (char *)new + size;
}

static bool in_bounds(const void *p)
{
	return !p
		|| (p >= (void *)&null_parent && p <= (void *)(&null_parent + 1))
		|| (p >= bounds_start && p <= bounds_end);
}
#else
static void update_bounds(const void *new, size_t size)
{
}

static bool in_bounds(const void *p)
{
	return true;
}
#endif

static void check_bounds(const void *p)
{
	if (!in_bounds(p))
		call_error("Not a valid header");
}

static struct tal_hdr *to_tal_hdr(const void *ctx)
{
	struct tal_hdr *t;

	t = (struct tal_hdr *)((char *)ctx - sizeof(struct tal_hdr));
	check_bounds(t);
	check_bounds(ignore_destroying_bit(t->parent_child));
	check_bounds(t->list.next);
	check_bounds(t->list.prev);
	if (t->prop && !is_literal(t->prop))
		check_bounds(t->prop);
	return t;
}

static struct tal_hdr *to_tal_hdr_or_null(const void *ctx)
{
	if (!ctx)
		return &null_parent.hdr;
	return to_tal_hdr(ctx);
}

static void *from_tal_hdr(struct tal_hdr *hdr)
{
	return hdr + 1;
}

#ifdef TAL_DEBUG
static void *from_tal_hdr_or_null(struct tal_hdr *hdr)
{
	if (hdr == &null_parent.hdr)
		return NULL;
	return from_tal_hdr(hdr);
}

static struct tal_hdr *debug_tal(struct tal_hdr *tal)
{
	tal_check(from_tal_hdr_or_null(tal), "TAL_DEBUG ");
	return tal;
}
#else
static struct tal_hdr *debug_tal(struct tal_hdr *tal)
{
	return tal;
}
#endif

static void *allocate(size_t size)
{
	void *ret;

	/* Don't hand silly sizes to malloc. */
	if (size >> (CHAR_BIT*sizeof(size) - 1)) {
		call_error("allocation size overflow");
		return NULL;
	}

	ret = allocfn(size);
	if (!ret)
		call_error("allocation failed");
	else
		update_bounds(ret, size);
	return ret;
}

static struct prop_hdr **find_property_ptr(const struct tal_hdr *t,
					   enum prop_type type)
{
        struct prop_hdr **p;

        for (p = (struct prop_hdr **)&t->prop; *p; p = &(*p)->next) {
                if (is_literal(*p)) {
                        if (type == NAME)
                                return p;
                        break;
                }
                if ((*p)->type == type)
                        return p;
        }
        return NULL;
}

static void *find_property(const struct tal_hdr *parent, enum prop_type type)
{
        struct prop_hdr **p = find_property_ptr(parent, type);

        if (p)
                return *p;
        return NULL;
}

static void init_property(struct prop_hdr *hdr,
			  struct tal_hdr *parent,
			  enum prop_type type)
{
	hdr->type = type;
	hdr->next = parent->prop;
	parent->prop = hdr;
}

static struct destructor *add_destructor_property(struct tal_hdr *t,
						  void (*destroy)(void *))
{
	struct destructor *prop = allocate(sizeof(*prop));
	if (prop) {
		init_property(&prop->hdr, t, DESTRUCTOR);
		prop->destroy = destroy;
	}
	return prop;
}

static bool del_destructor_property(struct tal_hdr *t,
				    void (*destroy)(void *))
{
        struct prop_hdr **p;

        for (p = (struct prop_hdr **)&t->prop; *p; p = &(*p)->next) {
		struct destructor *d;

                if (is_literal(*p))
			break;
                if ((*p)->type != DESTRUCTOR)
			continue;
		d = (struct destructor *)*p;
		if (d->destroy == destroy) {
			*p = (*p)->next;
			freefn(d);
			return true;
		}
        }
        return false;
}

static struct name *add_name_property(struct tal_hdr *t, const char *name)
{
	struct name *prop;

	prop = allocate(sizeof(*prop) + strlen(name) + 1);
	if (prop) {
		init_property(&prop->hdr, t, NAME);
		strcpy(prop->name, name);
	}
	return prop;
}

static struct children *add_child_property(struct tal_hdr *parent,
					   struct tal_hdr *child)
{
	struct children *prop = allocate(sizeof(*prop));
	if (prop) {
		init_property(&prop->hdr, parent, CHILDREN);
		prop->parent = parent;
		list_head_init(&prop->children);
	}
	return prop;
}

static bool add_child(struct tal_hdr *parent, struct tal_hdr *child)
{
	struct children *children = find_property(parent, CHILDREN);

        if (!children) {
		if (unlikely(!initialized)) {
			atexit(tal_cleanup);
			take_allocfail(take_alloc_failed);
			initialized = true;
		}
		children = add_child_property(parent, child);
		if (!children)
			return false;
	}
	list_add(&children->children, &child->list);
	child->parent_child = children;
	return true;
}

static void del_tree(struct tal_hdr *t)
{
	struct prop_hdr **prop, *p, *next;

        /* Already being destroyed?  Don't loop. */
        if (unlikely(get_destroying_bit(t->parent_child)))
                return;

        set_destroying_bit(&t->parent_child);

        /* Carefully call destructors, removing as we go. */
        while ((prop = find_property_ptr(t, DESTRUCTOR))) {
		struct destructor *d = (struct destructor *)*prop;
                d->destroy(from_tal_hdr(t));
                *prop = d->hdr.next;
		freefn(d);
        }

	/* Now free children and groups. */
	prop = find_property_ptr(t, CHILDREN);
	if (prop) {
		struct tal_hdr *i;
		struct children *c = (struct children *)*prop;

		while ((i = list_top(&c->children, struct tal_hdr, list))) {
			list_del(&i->list);
			del_tree(i);
		}
	}

        /* Finally free our properties. */
        for (p = t->prop; p && !is_literal(p); p = next) {
                next = p->next;
		freefn(p);
        }
        freefn(t);
}

void *tal_alloc_(const tal_t *ctx, size_t size, bool clear, const char *label)
{
        struct tal_hdr *child, *parent = debug_tal(to_tal_hdr_or_null(ctx));

        child = allocate(sizeof(struct tal_hdr) + size);
	if (!child)
		return NULL;
	if (clear)
		memset(from_tal_hdr(child), 0, size);
        child->prop = (void *)label;
        if (!add_child(parent, child)) {
		freefn(child);
		return NULL;
	}
	debug_tal(parent);
	return from_tal_hdr(debug_tal(child));
}

void *tal_free(const tal_t *ctx)
{
        if (ctx) {
		struct tal_hdr *t;
		int saved_errno = errno;
		t = debug_tal(to_tal_hdr(ctx));
		list_del(&t->list);
		del_tree(t);
		errno = saved_errno;
	}
	return NULL;
}

void *tal_steal_(const tal_t *new_parent, const tal_t *ctx)
{
        if (ctx) {
		struct tal_hdr *newpar, *t, *old_parent;

                newpar = debug_tal(to_tal_hdr_or_null(new_parent));
                t = debug_tal(to_tal_hdr(ctx));

                /* Unlink it from old parent. */
		list_del(&t->list);
		old_parent = ignore_destroying_bit(t->parent_child)->parent;

                if (unlikely(!add_child(newpar, t))) {
			/* We can always add to old parent, becuase it has a
			 * children property already. */
			if (!add_child(old_parent, t))
				abort();
			return NULL;
		}
		debug_tal(newpar);
        }
        return (void *)ctx;
}

bool tal_add_destructor_(tal_t *ctx, void (*destroy)(void *me))
{
        return add_destructor_property(debug_tal(to_tal_hdr(ctx)), destroy);
}

bool tal_del_destructor_(tal_t *ctx, void (*destroy)(void *me))
{
        return del_destructor_property(debug_tal(to_tal_hdr(ctx)), destroy);
}

bool tal_set_name_(tal_t *ctx, const char *name, bool literal)
{
        struct tal_hdr *t = debug_tal(to_tal_hdr(ctx));
        struct prop_hdr **prop = find_property_ptr(t, NAME);

        /* Get rid of any old name */
        if (prop) {
                struct name *name = (struct name *)*prop;
                if (is_literal(&name->hdr))
                        *prop = NULL;
                else {
                        *prop = name->hdr.next;
			freefn(name);
                }
        }

        if (literal && name[0]) {
                struct prop_hdr **p;

                /* Append literal. */
                for (p = &t->prop; *p && !is_literal(*p); p = &(*p)->next);
                *p = (struct prop_hdr *)name;
                return true;
        }
        if (!add_name_property(t, name))
		return false;
	debug_tal(t);
	return true;
}

const char *tal_name(const tal_t *t)
{
        struct name *n;

	n = find_property(debug_tal(to_tal_hdr(t)), NAME);
	if (!n)
		return NULL;

	if (is_literal(&n->hdr))
		return (const char *)n;
	return n->name;
}

/* Start one past first child: make stopping natural in circ. list. */
static struct tal_hdr *first_child(struct tal_hdr *parent)
{
	struct children *child;

	child = find_property(parent, CHILDREN);
        if (!child)
                return NULL;

	return list_top(&child->children, struct tal_hdr, list);
}

tal_t *tal_first(const tal_t *root)
{
        struct tal_hdr *c, *t = debug_tal(to_tal_hdr_or_null(root));

	c = first_child(t);
	if (!c)
		return NULL;
	return from_tal_hdr(c);
}

tal_t *tal_next(const tal_t *root, const tal_t *prev)
{
        struct tal_hdr *c, *t = debug_tal(to_tal_hdr(prev)), *top;

        /* Children? */
	c = first_child(t);
	if (c)
		return from_tal_hdr(c);

        top = to_tal_hdr_or_null(root);
        do {
		struct tal_hdr *next;
		struct list_node *end;

		end = &ignore_destroying_bit(t->parent_child)->children.n;

		next = list_entry(t->list.next, struct tal_hdr, list);
		if (&next->list != end)
			return from_tal_hdr(next);

                /* OK, go back to parent. */
                t = ignore_destroying_bit(t->parent_child)->parent;
        } while (t != top);

        return NULL;
}

tal_t *tal_parent(const tal_t *ctx)
{
        struct tal_hdr *t;

	if (!ctx)
		return NULL;

	t = debug_tal(to_tal_hdr(ctx));
	if (ignore_destroying_bit(t->parent_child)->parent == &null_parent.hdr)
		return NULL;
        return from_tal_hdr(ignore_destroying_bit(t->parent_child)->parent);
}

bool tal_resize_(tal_t **ctxp, size_t size)
{
        struct tal_hdr *old_t, *t;
        struct children *child;

        old_t = debug_tal(to_tal_hdr(*ctxp));

	/* Don't hand silly sizes to realloc. */
	if (size >> (CHAR_BIT*sizeof(size) - 1)) {
		call_error("Reallocation size overflow");
		return false;
	}

        t = resizefn(old_t, size + sizeof(struct tal_hdr));
	if (!t) {
		call_error("Reallocation failure");
		return false;
	}

	/* If it didn't move, we're done! */
        if (t == old_t)
                return true;
	update_bounds(t, size + sizeof(struct tal_hdr));

	/* Fix up linked list pointers. */
	if (list_entry(t->list.next, struct tal_hdr, list) != old_t)
		t->list.next->prev = t->list.prev->next = &t->list;

	/* Fix up child property's parent pointer. */
	child = find_property(t, CHILDREN);
	if (child) {
		assert(child->parent == old_t);
		child->parent = t;
	}
	*ctxp = from_tal_hdr(debug_tal(t));
	return true;
}

char *tal_strdup(const tal_t *ctx, const char *p)
{
	/* We have to let through NULL for take(). */
	return tal_dup(ctx, char, p, p ? strlen(p) + 1: 1, 0);
}

char *tal_strndup(const tal_t *ctx, const char *p, size_t n)
{
	size_t len;
	char *ret;

	/* We have to let through NULL for take(). */
	if (likely(p)) {
		len = strlen(p);
		if (len > n)
			len = n;
	} else
		len = n;

	ret = tal_dup(ctx, char, p, len, 1);
	if (ret)
		ret[len] = '\0';
	return ret;
}

void *tal_dup_(const tal_t *ctx, const void *p, size_t n, size_t extra,
	       const char *label)
{
	void *ret;

	/* Beware overflow! */
	if (n + extra < n || n + extra + sizeof(struct tal_hdr) < n) {
		call_error("dup size overflow");
		if (taken(p))
			tal_free(p);
		return NULL;
	}

	if (taken(p)) {
		if (unlikely(!p))
			return NULL;
		if (unlikely(!tal_resize_((void **)&p, n + extra)))
			return tal_free(p);
		if (unlikely(!tal_steal(ctx, p)))
			return tal_free(p);
		return (void *)p;
	}
	ret = tal_alloc_(ctx, n + extra, false, label);
	if (ret)
		memcpy(ret, p, n);
	return ret;
}

char *tal_asprintf(const tal_t *ctx, const char *fmt, ...)
{
	va_list ap;
	char *ret;

	va_start(ap, fmt);
	ret = tal_vasprintf(ctx, fmt, ap);
	va_end(ap);

	return ret;
}

char *tal_vasprintf(const tal_t *ctx, const char *fmt, va_list ap)
{
	size_t max;
	char *buf;
	int ret;

	if (!fmt && taken(fmt))
		return NULL;

	/* A decent guess to start. */
	max = strlen(fmt) * 2;
	buf = tal_arr(ctx, char, max);
	while (buf) {
		va_list ap2;

		va_copy(ap2, ap);
		ret = vsnprintf(buf, max, fmt, ap2);
		va_end(ap2);

		if (ret < max)
			break;
		if (!tal_resize(&buf, max *= 2))
			buf = tal_free(buf);
	}
	if (taken(fmt))
		tal_free(fmt);
	return buf;
}

void tal_set_backend(void *(*alloc_fn)(size_t size),
		     void *(*resize_fn)(void *, size_t size),
		     void (*free_fn)(void *),
		     void (*error_fn)(const char *msg))
{
	if (alloc_fn)
		allocfn = alloc_fn;
	if (resize_fn)
		resizefn = resize_fn;
	if (free_fn)
		freefn = free_fn;
	if (error_fn)
		errorfn = error_fn;
}

#ifdef CCAN_TAL_DEBUG
static void dump_node(unsigned int indent, const struct tal_hdr *t)
{
	unsigned int i;
        const struct prop_hdr *p;

	for (i = 0; i < indent; i++)
		printf("  ");
	printf("%p", t);
        for (p = t->prop; p; p = p->next) {
		struct children *c;
		struct destructor *d;
		struct name *n;
                if (is_literal(p)) {
			printf(" \"%s\"", (const char *)p);
			break;
		}
		switch (p->type) {
		case CHILDREN:
			c = (struct children *)p;
			printf(" CHILDREN(%p):parent=%p,children={%p,%p}\n",
			       p, c->parent,
			       c->children.n.prev, c->children.n.next);
			break;
		case DESTRUCTOR:
			d = (struct destructor *)p;
			printf(" DESTRUCTOR(%p):fn=%p", p, d->destroy);
			break;
		case NAME:
			n = (struct name *)p;
			printf(" NAME(%p):%s", p, n->name);
			break;
		default:
			printf(" **UNKNOWN(%p):%i**", p, p->type);
		}
	}
	printf("\n");
}

static void tal_dump_(unsigned int level, const struct tal_hdr *t)
{
        struct children *children;

	dump_node(level, t);

	children = find_property(t, CHILDREN);
	if (children) {
		struct tal_hdr *i;

		list_for_each(&children->children, i, list)
			tal_dump_(level + 1, i);
	}
}

void tal_dump(void)
{
	tal_dump_(0, &null_parent.hdr);
}
#endif /* CCAN_TAL_DEBUG */

#ifndef NDEBUG
static bool check_err(struct tal_hdr *t, const char *errorstr,
		      const char *errmsg)
{
	if (errorstr) {
		/* Try not to malloc: it may be corrupted. */
		char msg[strlen(errorstr) + 20 + strlen(errmsg) + 1];
		sprintf(msg, "%s:%p %s", errorstr, from_tal_hdr(t), errmsg);
		call_error(msg);
	}
	return false;
}

static bool check_node(struct children *parent_child,
		       struct tal_hdr *t, const char *errorstr)
{
	struct prop_hdr *p;
	struct name *name = NULL;
	struct children *children = NULL;

	if (!in_bounds(t))
		return check_err(t, errorstr, "invalid pointer");

	if (ignore_destroying_bit(t->parent_child) != parent_child)
		return check_err(t, errorstr, "incorrect parent");

	for (p = t->prop; p; p = p->next) {
		if (is_literal(p)) {
			if (name)
				return check_err(t, errorstr,
						 "has extra literal");
			name = (struct name *)p;
			break;
		}
		if (!in_bounds(p))
			return check_err(t, errorstr,
					 "has bad property pointer");

		switch (p->type) {
		case CHILDREN:
			if (children)
				return check_err(t, errorstr,
						 "has two child nodes");
			children = (struct children *)p;
			break;
		case DESTRUCTOR:
			break;
		case NAME:
			if (name)
				return check_err(t, errorstr,
						 "has two names");
			name = (struct name *)p;
			break;
		default:
			return check_err(t, errorstr, "has unknown property");
		}
	}
	if (children) {
		struct tal_hdr *i;

		if (!list_check(&children->children, errorstr))
			return false;
		list_for_each(&children->children, i, list) {
			if (!check_node(children, i, errorstr))
				return false;
		}
	}
	return true;
}

bool tal_check(const tal_t *ctx, const char *errorstr)
{
	struct tal_hdr *t = to_tal_hdr_or_null(ctx);

	return check_node(ignore_destroying_bit(t->parent_child), t, errorstr);
}
#else /* NDEBUG */
bool tal_check(const tal_t *ctx, const char *errorstr)
{
	return true;
}
#endif
