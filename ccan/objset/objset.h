/* Licensed under LGPLv2+ - see LICENSE file for details */
#ifndef CCAN_OBJSET_H
#define CCAN_OBJSET_H
#include "config.h"
#include <ccan/htable/htable_type.h>
#include <ccan/hash/hash.h>
#include <ccan/tcon/tcon.h>
#include <stdlib.h>
#include <stdbool.h>

static inline const void *objset_key_(const void *elem)
{
	return elem;
}
static inline size_t objset_hashfn_(const void *elem)
{
	return hash_pointer(elem, 0);
}
static inline bool objset_eqfn_(const void *e1, const void *e2)
{
	return e1 == e2;
}
HTABLE_DEFINE_TYPE(void, objset_key_, objset_hashfn_, objset_eqfn_, objset_h);

/**
 * OBJSET_MEMBERS - declare members for a type-specific unordered objset.
 * @type: type for this set's values, or void * for any pointer.
 *
 * You use this to create your own typed objset for a particular type.
 * You can use an integer type, *but* remember you can't use "0" as a
 * value!
 *
 * Example:
 *	struct objset_int {
 *		OBJSET_MEMBERS(int *);
 *	};
 */
#define OBJSET_MEMBERS(type)			\
	TCON_WRAP(struct objset_h, type canary) objset_

#define objset_raw(set)				\
	tcon_unwrap(&(set)->objset_)

/**
 * objset_init - initialize an empty objset
 * @set: the typed objset to initialize.
 *
 * Example:
 *	struct objset_int set;
 *
 *	objset_init(&set);
 */
#define objset_init(set) objset_h_init(objset_raw(set))

/**
 * objset_empty - is this set empty?
 * @set: the typed objset to check.
 *
 * Example:
 *	if (!objset_empty(&set))
 *		abort();
 */
#define objset_empty(set) objset_empty_(objset_raw(set))

static inline bool objset_empty_(const struct objset_h *set)
{
	struct objset_h_iter i;
	return objset_h_first(set, &i) == NULL;
}

/**
 * objset_add - place a member into the set.
 * @set: the typed objset to add to.
 * @value: the (non-NULL) object to place in the set.
 *
 * This returns false if we run out of memory (errno = ENOMEM), or
 * (more normally) if that pointer already appears in the set (EEXIST).
 *
 * Example:
 *	int *val;
 *
 *	val = malloc(sizeof *val);
 *	*val = 17;
 *	if (!objset_add(&set, val))
 *		printf("Impossible: value was already in the set?\n");
 */
#define objset_add(set, value)						\
	objset_h_add(tcon_unwrap(tcon_check(&(set)->objset_, canary, (value))), (void *)(value))

/**
 * objset_get - get a value from a set
 * @set: the typed objset to search.
 * @value: the value to search for.
 *
 * Returns the value, or NULL if it isn't in the set (and sets errno = ENOENT).
 *
 * Example:
 *	if (objset_get(&set, val));
 *		printf("hello => %i\n", *val);
 */
#define objset_get(set, member)					\
	tcon_cast(&(set)->objset_, canary,			\
		  objset_h_get(objset_raw(set), (member)))

/**
 * objset_del - remove a member from the set.
 * @set: the typed objset to delete from.
 * @value: the value (non-NULL) to remove from the set
 *
 * This returns false NULL if @value was not in the set (and sets
 * errno = ENOENT).
 *
 * Example:
 *	if (!objset_del(&set, val))
 *		printf("val was not in the set?\n");
 */
#define objset_del(set, value)						\
	objset_h_del(tcon_unwrap(tcon_check(&(set)->objset_, canary, value)), \
		     (const void *)value)

/**
 * objset_clear - remove every member from the set.
 * @set: the typed objset to clear.
 *
 * The set will be empty after this.
 *
 * Example:
 *	objset_clear(&set);
 */
#define objset_clear(set) objset_h_clear(objset_raw(set))

/**
 * objset_iter - iterator reference.
 *
 * This is valid for a particular set as long as the contents remain unchaged,
 * otherwise the effect is undefined.
 */
struct objset_iter {
	struct objset_h_iter iter;
};

/**
 * objset_first - get an element in the set
 * @set: the typed objset to iterate through.
 * @i: a struct objset_iter to use as an iterator.
 *
 * Example:
 *	struct objset_iter i;
 *	int *v;
 *
 *	v = objset_first(&set, &i);
 *	if (v)
 *		printf("One value is %i\n", *v);
 */
#define objset_first(set, i)					\
	tcon_cast(&(set)->objset_, canary,			\
		  objset_h_first(objset_raw(set), &(i)->iter))

/**
 * objset_next - get the another element in the set
 * @set: the typed objset to iterate through.
 * @i: a struct objset_iter to use as an iterator.
 *
 * @i must have been set by a successful objset_first() or
 * objset_next() call.
 *
 * Example:
 *	while (v) {
 *		v = objset_next(&set, &i);
 *		if (v)
 *			printf("Another value is %i\n", *v);
 *	}
 */
#define objset_next(set, i)					\
	tcon_cast(&(set)->objset_, canary,			\
		  objset_h_next(objset_raw(set), &(i)->iter))

#endif /* CCAN_OBJSET_H */
