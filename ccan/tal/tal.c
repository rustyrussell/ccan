/* Licensed under BSD-MIT - see LICENSE file for details */
#include <ccan/tal/tal.h>
#include <ccan/compiler/compiler.h>
#include <ccan/hash/hash.h>
#include <ccan/list/list.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

//#define TAL_DEBUG 1

/* How large should grouips get? */
#define GROUP_NODE_AVERAGE 32

/* 32-bit type field, first byte 0 in either endianness. */
enum prop_type {
	CHILDREN = 0x00c1d500,
	GROUP = 0x00600d00,
	DESTRUCTOR = 0x00de5700,
	NAME = 0x00111100,
};

struct tal_hdr {
	struct tal_hdr *next;
	struct prop_hdr *prop;
};

struct prop_hdr {
	enum prop_type type;
	struct prop_hdr *next;
};

/* Unlike other properties, this is owned by parent, not child! */
struct group {
	struct prop_hdr hdr; /* GROUP */
	struct list_head list; /* Head for child->group, node for others. */
	/* We point to parent's children property, as it doesn't move! */
	struct children *parent_child;
	struct tal_hdr *first_child;
};

struct children {
	struct prop_hdr hdr; /* CHILDREN */
	struct tal_hdr *parent;
	/* We always have one group.  Others may be added. */
	struct group group;
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
} null_parent = { { NULL, &null_parent.c.hdr },
		  { { CHILDREN, NULL },
		    &null_parent.hdr,
		    { { GROUP, NULL },
		      { { &null_parent.c.group.list.n,
			  &null_parent.c.group.list.n } },
		      &null_parent.c, NULL } }
};


static void *(*allocfn)(size_t size) = malloc;
static void *(*resizefn)(void *, size_t size) = realloc;
static void (*freefn)(void *) = free;
static void (*errorfn)(const char *msg) = (void *)abort;

static inline void COLD call_error(const char *msg)
{
	errorfn(msg);
}

static bool get_destroying_bit(struct tal_hdr *next)
{
	return (size_t)next & 1;
}

static void set_destroying_bit(struct tal_hdr **next)
{
	*next = (void *)((size_t)next | 1);
}

static struct tal_hdr *ignore_destroying_bit(struct tal_hdr *next)
{
	return (void *)((size_t)next & ~(size_t)1);
}

static struct group *next_group(struct group *group)
{
	return list_entry(group->list.n.next, struct group, list.n);
}

static bool atexit_set = false;
/* This means valgrind can see leaks. */
static void unlink_null(void)
{
	struct group *i, *next;

	for (i = next_group(&null_parent.c.group);
	     i != &null_parent.c.group;
	     i = next) {
		next = next_group(i);
		freefn(i);
	}
	null_parent.c.group.first_child = NULL;
}

#ifndef NDEBUG
static const void *bounds_start, *bounds_end;

static void update_bounds(const void *new)
{
	if (unlikely(!bounds_start))
		bounds_start = bounds_end = new;
	else if (new < bounds_start)
		bounds_start = new;
	else if (new > bounds_end)
		bounds_end = new;
}

static bool in_bounds(const void *p)
{
	return !p || (p >= bounds_start && p <= bounds_end);
}
#else
static void update_bounds(const void *new)
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
	check_bounds(ignore_destroying_bit(t->next));
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
		update_bounds(ret);
	return ret;
}

/* We carefully start all real properties with a zero byte. */
static bool is_literal(const struct prop_hdr *prop)
{
	return ((char *)prop)[0] != 0;
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

static void init_group_property(struct group *group,
				struct children *parent_child,
				struct tal_hdr *child)
{
	init_property(&group->hdr, child, GROUP);
	group->parent_child = parent_child;
	group->first_child = child;
}

static struct children *add_child_property(struct tal_hdr *parent,
					   struct tal_hdr *child)
{
	struct children *prop = allocate(sizeof(*prop));
	if (prop) {
		init_property(&prop->hdr, parent, CHILDREN);
		prop->parent = parent;

		init_group_property(&prop->group, prop, child);
		list_head_init(&prop->group.list);
		update_bounds(&prop->group);
	}
	return prop;
}

static struct group *add_group_property(struct tal_hdr *child,
					struct children *parent_child)
{
	struct group *prop = allocate(sizeof(*prop));
	if (prop)
		init_group_property(prop, parent_child, child);
	return prop;
}

static bool add_child(struct tal_hdr *parent, struct tal_hdr *child)
{
        struct group *group;
	struct children *children = find_property(parent, CHILDREN);

        if (!children) {
		children = add_child_property(parent, child);
		if (!children)
			return false;
		children->group.list.n.next = children->group.list.n.prev
			= &children->group.list.n;

		/* Child links to itself. */
                child->next = child;
		return true;
	}

	/* Last one (may be children->group itself). */
	group = next_group(&children->group);

	/* Empty group can happen: null_parent, or all children freed. */
	if (unlikely(!group->first_child)) {
		assert(group == &children->group);
		/* This hits on first child appended to null parent. */
		if (unlikely(!atexit_set)) {
			atexit(unlink_null);
			atexit_set = true;
		}
		/* Link group into this child, make it the first one. */
		group->hdr.next = child->prop;
		child->prop = &group->hdr;
		group->first_child = child;

		/* Child links to itself. */
		child->next = child;
		return true;
	}

	if (unlikely(hash_pointer(child, 0) % GROUP_NODE_AVERAGE == 0)) {
		struct group *newgroup;

		newgroup = add_group_property(child, children);
		if (likely(newgroup)) {
			list_add(&children->group.list, &newgroup->list.n);

			/* Child links to itself. */
			child->next = child;
			return true;
		}
		/* Fall through: on allocation failure reuse old group. */
        }

	/* We insert after head, otherwise we'd need to find end. */
	child->next = group->first_child->next;
	group->first_child->next = child;
	return true;
}

static void del_tree(struct tal_hdr *t)
{
	struct prop_hdr **prop, *p, *next;

        /* Already being destroyed?  Don't loop. */
        if (unlikely(get_destroying_bit(t->next)))
                return;

        set_destroying_bit(&t->next);

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
		struct children *c = (struct children *)*prop;
		struct group *group, *next;

		group = &c->group;
		do {
			next = next_group(group);
			if (group->first_child) {
				struct tal_hdr *i, *nextc;

				i = group->first_child;
				do {
					nextc = i->next;
					del_tree(i);
					i = nextc;
				} while (i != group->first_child);
			}
			if (group != &c->group)
				freefn(group);
			group = next;
		} while (group != &c->group);
	}

        /* Finally free our properties (groups are freed by parent). */
        for (p = t->prop; p && !is_literal(p); p = next) {
                next = p->next;
		if (p->type != GROUP)
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

/* Update back ptrs, etc, as required.
 * May return pointer to parent. */
static struct tal_hdr *remove_node(struct tal_hdr *t)
{
        struct prop_hdr **prop;
        struct tal_hdr *prev;

	/* Loop around to find previous node. */
	for (prev = t->next; prev->next != t; prev = prev->next);

	/* Unlink ourselves. */
	prev->next = t->next;

	/* Are we the node with the group property? */
	prop = find_property_ptr(t, GROUP);
	if (prop) {
		struct group *group = (struct group *)*prop;

		/* Are we the only one? */
		if (prev == t) {
			struct children *c = group->parent_child;
			/* Is this the group embedded in the child property? */
			if (group == &c->group) {
				group->first_child = NULL;
			} else {
				/* Empty group, so free it. */
				list_del_from(&c->group.list, &group->list.n);
				*prop = group->hdr.next;
				freefn(group);
			}
			return c->parent;
		} else {
			/* Move property to next node. */
			group->first_child = t->next;

			*prop = group->hdr.next;
			group->hdr.next = t->next->prop;
			t->next->prop = &group->hdr;
		}
	}
	return NULL;
}

void tal_free(const tal_t *ctx)
{
        struct tal_hdr *t;

        if (!ctx)
                return;

        t = debug_tal(to_tal_hdr(ctx));
        remove_node(t);
        del_tree(t);
}

void *tal_steal_(const tal_t *new_parent, const tal_t *ctx)
{
        if (ctx) {
		struct tal_hdr *newpar, *t, *old_next, *old_parent;

                newpar = debug_tal(to_tal_hdr_or_null(new_parent));
                t = debug_tal(to_tal_hdr(ctx));

		/* Save enough data to get us back if we fail! */
		old_next = t->next;

                /* Unlink it from old parent. */
                old_parent = remove_node(t);
                if (unlikely(!add_child(newpar, t))) {
			/* If we were last child, parent returned by
			 * remove_node, otherwise search old siblings
			 * for it. */
			if (!old_parent) {
				struct group *g;
				while (!(g = find_property(old_next, GROUP)))
					old_next = old_next->next;
				old_parent = g->parent_child->parent;
			}
			/* We can always add to old parent, becuase it has one
			 * group already. */
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
	struct group *group;

	child = find_property(parent, CHILDREN);
        if (!child)
                return NULL;

	/* Careful of empty group embedded in child property. */
	if (child->group.first_child)
		return child->group.first_child->next;

	/* There could still be another group! */
	group = next_group(&child->group);
	if (group == &child->group)
		return NULL;

	return group->first_child->next;
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
        struct group *group;

        /* Children? */
	c = first_child(t);
	if (c)
		return from_tal_hdr(c);

        top = to_tal_hdr_or_null(root);
        do {
		struct group *next;

		/* Are we back to first child in group? */
		group = find_property(t, GROUP);
		if (!group)
                        return from_tal_hdr(t->next);

		/* Last group is one inside children property. */
		next = next_group(group);
		if (next != &group->parent_child->group)
			return from_tal_hdr(next->first_child->next);

                /* OK, go back to parent. */
                t = group->parent_child->parent;
        } while (t != top);

        return NULL;
}

tal_t *tal_parent(const tal_t *ctx)
{
        struct group *group;
        struct tal_hdr *t = debug_tal(to_tal_hdr(ctx));

	while (!(group = find_property(t, GROUP)))
		t = t->next;

	if (group->parent_child->parent == &null_parent.hdr)
		return NULL;
        return from_tal_hdr(group->parent_child->parent);
}

void *tal_realloc_(tal_t *ctx, size_t size)
{
        struct tal_hdr *old_t, *t, **prev;
        struct group *group;
        struct children *child;

        old_t = debug_tal(to_tal_hdr(ctx));

        t = resizefn(old_t, size + sizeof(struct tal_hdr));
	if (!t) {
		call_error("Reallocation failure");
		tal_free(old_t);
		return NULL;
	}
        if (t == old_t)
                return ctx;
	update_bounds(t);

	/* Fix up linked list pointer. */
	for (prev = &t->next; *prev != old_t; prev = &(*prev)->next);
	*prev = t;

	/* Fix up group pointer, if any. */
	group = find_property(t, GROUP);
	if (group) {
		assert(group->first_child == old_t);
		group->first_child = t;
	}

	/* Fix up child propertie's parent pointer. */
	child = find_property(t, CHILDREN);
	if (child) {
		assert(child->parent == old_t);
		child->parent = t;
	}

        return from_tal_hdr(debug_tal(t));
}

char *tal_strdup(const tal_t *ctx, const char *p)
{
	return tal_memdup(ctx, p, strlen(p)+1);
}

char *tal_strndup(const tal_t *ctx, const char *p, size_t n)
{
	char *ret;

	if (strlen(p) < n)
		n = strlen(p);
	ret = tal_memdup(ctx, p, n+1);
	if (ret)
		ret[n] = '\0';
	return ret;
}

void *tal_memdup(const tal_t *ctx, const void *p, size_t n)
{
	void *ret;

	if (ctx == TAL_TAKE)
		return (void *)p;

	ret = tal_arr(ctx, char, n);
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
	size_t max = strlen(fmt) * 2;
	char *buf;
	int ret;

	if (ctx == TAL_TAKE)
		buf = tal_arr(tal_parent(fmt), char, max);
	else
		buf = tal_arr(ctx, char, max);

	while (buf) {
		va_list ap2;

		va_copy(ap2, ap);
		ret = vsnprintf(buf, max, fmt, ap2);
		va_end(ap2);

		if (ret < max)
			break;
		buf = tal_resize(buf, max *= 2);
	}
	if (ctx == TAL_TAKE)
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
		struct group *g;
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
			printf(" CHILDREN(%p):parent=%p,group=%p\n",
			       p, c->parent, &c->group);
			g = &c->group;
			printf("  GROUP(%p):list={%p,%p},parent_ch=%p,first=%p",
			       g, g->list.n.next, g->list.n.next,
			       g->parent_child, g->first_child);
			break;
		case GROUP:
			g = (struct group *)p;
			printf(" GROUP(%p):list={%p,%p},,parent_ch=%p,first=%p",
			       p, g->list.n.next, g->list.n.next,
			       g->parent_child, g->first_child);
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
	struct group *group;

	dump_node(level, t);

	children = find_property(t, CHILDREN);
	if (!children)
		return;

	group = &children->group;
	do {
		struct tal_hdr *i;

		i = group->first_child;
		if (i) {
			do {
				tal_dump_(level+1, i);
				i = i->next;
			} while (i != group->first_child);
		}
		group = next_group(group);
	} while (group != &children->group);
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

static bool check_group(struct group *group,
			struct tal_hdr *t, const char *errorstr);

static bool check_node(struct group *group,
		       struct tal_hdr *t, const char *errorstr)
{
	struct prop_hdr *p;
	struct name *name = NULL;
	struct children *children = NULL;
	struct group *gr = NULL;

	if (t != &null_parent.hdr && !in_bounds(t))
		return check_err(t, errorstr, "invalid pointer");

	for (p = t->prop; p; p = p->next) {
		if (is_literal(p)) {
			if (name)
				return check_err(t, errorstr,
						 "has extra literal");
			name = (struct name *)p;
			break;
		}
		if (p != &null_parent.c.hdr && p != &null_parent.c.group.hdr
		    && !in_bounds(p))
			return check_err(t, errorstr,
					 "has bad property pointer");

		switch (p->type) {
		case GROUP:
			if (gr)
				return check_err(t, errorstr,
						 "has two groups");
			gr = (struct group *)p;
			break;
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
	if (group && gr != group)
		return check_err(t, errorstr, "has bad group");

	if (children) {
		if (!list_check(&children->group.list, errorstr))
			return false;
		gr = &children->group;
		do {
			if (gr->first_child) {
				if (!check_group(gr, gr->first_child, errorstr))
					return false;
			} else if (gr != &children->group) {
				/* Empty groups should be deleted! */
				return check_err(t, errorstr,
						 "has empty group");
			}
			gr = next_group(gr);
		} while (gr != &children->group);
	}
	return true;
}

static bool check_group(struct group *group,
			struct tal_hdr *t, const char *errorstr)
{
	struct tal_hdr *i;

	i = t;
	do {
		if (!check_node(group, i, errorstr))
			return false;
		group = NULL;
		i = i->next;
	} while (i != t);
	return true;
}

bool tal_check(const tal_t *ctx, const char *errorstr)
{
	struct tal_hdr *t = to_tal_hdr_or_null(ctx);

	return check_node(NULL, t, errorstr);
}
#else /* NDEBUG */
bool tal_check(const tal_t *ctx, const char *errorstr)
{
	return true;
}
#endif
