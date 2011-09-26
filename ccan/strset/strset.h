#ifndef CCAN_STRSET_H
#define CCAN_STRSET_H
#include "config.h"
#include <ccan/typesafe_cb/typesafe_cb.h>
#include <stdlib.h>
#include <stdbool.h>

/**
 * struct strset - representation of a string set
 *
 * It's exposed here to allow you to embed it and so we can inline the
 * trivial functions.
 */
struct strset {
	union {
		struct node *n;
		const char *s;
	} u;
};

/**
 * strset_init - initialize a string set (empty)
 *
 * For completeness; if you've arranged for it to be NULL already you don't
 * need this.
 *
 * Example:
 *	struct strset set;
 *
 *	strset_init(&set);
 */
static inline void strset_init(struct strset *set)
{
	set->u.n = NULL;
}

/**
 * strset_empty - is this string set empty?
 * @set: the set.
 *
 * Example:
 *	if (!strset_empty(&set))
 *		abort();
 */
static inline bool strset_empty(const struct strset *set)
{
	return set->u.n == NULL;
}

/**
 * strset_test - is this a member of this string set?
 * @set: the set.
 * @member: the string to search for.
 *
 * Returns the member, or NULL if it isn't in the set.
 *
 * Example:
 *	if (strset_test(&set, "hello"))
 *		printf("hello is in the set\n");
 */
char *strset_test(const struct strset *set, const char *member);

/**
 * strset_set - place a member in the string set.
 * @set: the set.
 * @member: the string to place in the set.
 *
 * This returns false if we run out of memory, or (more normally) if that
 * string already appears in the set.
 *
 * Note that the pointer is placed in the set, the string is not copied.  If
 * you want a copy in the set, use strdup().
 *
 * Example:
 *	if (!strset_set(&set, "goodbye"))
 *		printf("goodbye was already in the set\n");
 */
bool strset_set(struct strset *set, const char *member);

/**
 * strset_clear - remove a member from the string set.
 * @set: the set.
 * @member: the string to remove from the set.
 *
 * This returns the string which was passed to strset_set(), or NULL.
 * This means that if you allocated a string (eg. using strdup()), you can
 * free it here.
 *
 * Example:
 *	if (!strset_clear(&set, "goodbye"))
 *		printf("goodbye was not in the set?\n");
 */
char *strset_clear(struct strset *set, const char *member);

/**
 * strset_destroy - remove every member from the set.
 * @set: the set.
 *
 * The set will be empty after this.
 *
 * Example:
 *	strset_destroy(&set);
 */
void strset_destroy(struct strset *set);

/**
 * strset_iterate - ordered iteration over a set
 * @set: the set.
 * @handle: the function to call.
 * @arg: the argument for the function (types should match).
 *
 * You should not alter the set within the @handle function!  If it returns
 * true, the iteration will stop.
 *
 * Example:
 *	static bool dump_some(const char *member, int *num)
 *	{
 *		// Only dump out num nodes.
 *		if (*(num--) == 0)
 *			return true;
 *		printf("%s\n", member);
 *		return false;
 *	}
 *
 *	static void dump_set(const struct strset *set)
 *	{
 *		int max = 100;
 *		strset_iterate(set, dump_some, &max);
 *		if (max < 0)
 *			printf("... (truncated to 100 entries)\n");
 *	}
 */
#define strset_iterate(set, handle, arg)				\
	strset_iterate_((set), typesafe_cb_preargs(bool, void *,	\
						   (handle), (arg),	\
						   const char *),	\
			(arg))
void strset_iterate_(const struct strset *set,
		     bool (*handle)(const char *, void *), void *data);


/**
 * strset_prefix - return a subset matching a prefix
 * @set: the set.
 * @prefix: the prefix.
 *
 * This returns a pointer into @set, so don't alter @set while using
 * the return value.  You can use strset_iterate(), strset_test() or
 * strset_empty() on the returned pointer.
 *
 * Example:
 *	static void dump_prefix(const struct strset *set, const char *prefix)
 *	{
 *		int max = 100;
 *		printf("Nodes with prefix %s:\n", prefix);
 *		strset_iterate(strset_prefix(set, prefix), dump_some, &max);
 *		if (max < 0)
 *			printf("... (truncated to 100 entries)\n");
 *	}
 */
const struct strset *strset_prefix(const struct strset *set,
				   const char *prefix);

#endif /* CCAN_STRSET_H */
