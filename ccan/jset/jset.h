/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef CCAN_JSET_H
#define CCAN_JSET_H
#include "config.h"
#include <ccan/compiler/compiler.h>
#include <ccan/tcon/tcon.h>
#include <Judy.h>
#include <stdbool.h>
#include <assert.h>

/**
 * struct jset - private definition of a jset.
 *
 * It's exposed here so you can put it in your structures and so we can
 * supply inline functions.
 */
struct jset {
	void *judy;
	JError_t err;
	const char *errstr;
};

/**
 * JSET_MEMBERS - declare members for a type-specific jset.
 * @type: type for this set to contain, or void * for any pointer.
 *
 * Example:
 *	struct jset_long {
 *		JSET_MEMBERS(long);
 *	};
 */
#define JSET_MEMBERS(type)			\
	TCON_WRAP(struct jset, type canary) jset_

/**
 * jset_new - create a new, empty jset.
 * @type: the tcon-containing type to allocate.
 *
 * Example:
 *	struct jset_long {
 *		JSET_MEMBERS(long);
 *	} *set = jset_new(struct jset_long);
 *
 *	if (!set)
 *		errx(1, "Failed to allocate set");
 */
#define jset_new(type) ((type *)jset_new_(sizeof(type)))


/**
 * jset_raw_ - unwrap the typed set (without type checking)
 * @set: the typed jset
 */
#define jset_raw_(set)	(tcon_unwrap(&(set)->jset_))


/**
 * jset_free - destroy a jset.
 * @set: the set returned from jset_new.
 *
 * Example:
 *	jset_free(set);
 */
#define jset_free(set) jset_free_(jset_raw_(set))

/**
 * jset_error - test for an error in the a previous jset_ operation.
 * @set: the set to test.
 *
 * Under normal circumstances, return NULL to indicate no error has occurred.
 * Otherwise, it will return a string containing the error.  This string
 * can only be freed by jset_free() on the set.
 *
 * Other than out-of-memory, errors are caused by memory corruption or
 * interface misuse.
 *
 * Example:
 *	const char *errstr;
 *
 *	errstr = jset_error(set);
 *	if (errstr)
 *		errx(1, "Woah, error on newly created set?! %s", errstr);
 */
#define jset_error(set) jset_error_(jset_raw_(set))


/**
 * jset_raw - unwrap the typed set and check the type
 * @set: the typed jset
 * @expr: the expression to check the type against (not evaluated)
 *
 * This macro usually causes the compiler to emit a warning if the
 * variable is of an unexpected type.  It is used internally where we
 * need to access the raw underlying jset.
 */
#define jset_raw(set, expr) \
	(tcon_unwrap(tcon_check(&(set)->jset_, canary, (expr))))


/**
 * jset_test - test a bit in the bitset.
 * @set: bitset from jset_new
 * @index: the index to test
 *
 * Returns true if jset_set() has been done on this index, false otherwise.
 *
 * Example:
 *	assert(!jset_test(set, 0));
 */
#define jset_test(set, index)						\
	jset_test_(jset_raw((set), (index)), (unsigned long)(index))

/**
 * jset_set - set a bit in the bitset.
 * @set: bitset from jset_new
 * @index: the index to set
 *
 * Returns false if it was already set (ie. nothing changed)
 *
 * Example:
 *	if (jset_set(set, 0))
 *		err(1, "Bit 0 was already set?!");
 */
#define jset_set(set, index)						\
	jset_set_(jset_raw((set), (index)), (unsigned long)(index))

/**
 * jset_clear - clear a bit in the bitset.
 * @set: bitset from jset_new
 * @index: the index to set
 *
 * Returns the old bit value (ie. false if nothing changed).
 *
 * Example:
 *	if (jset_clear(set, 0))
 *		err(1, "Bit 0 was already clear?!");
 */
#define jset_clear(set, index)						\
	jset_clear_(jset_raw((set), (index)), (unsigned long)(index))

/**
 * jset_count - get population of bitset.
 * @set: bitset from jset_new
 *
 * Example:
 *	// We expect 1000 entries.
 *	assert(jset_count(set) == 1000);
 */
#define jset_count(set)	\
	jset_popcount_(jset_raw_(set), 0, -1UL)

/**
 * jset_popcount - get population of (some part of) bitset.
 * @set: bitset from jset_new
 * @start: first index to count
 * @end_incl: last index to count (use -1 for end).
 *
 * Example:
 *	assert(jset_popcount(set, 0, 1000) <= jset_popcount(set, 0, 2000));
 */
#define jset_popcount(set, start, end_incl)				\
	jset_popcount_(jset_raw((set), (start) ? (start) : (end_incl)), \
		       (unsigned long)(start), (unsigned long)(end_incl))

/**
 * jset_nth - return the index of the nth bit which is set.
 * @set: bitset from jset_new
 * @n: which bit we are interested in (0-based)
 * @invalid: what to return if n >= set population
 *
 * This normally returns the nth bit in the set, and often there is a
 * convenient known-invalid value (ie. something which is never in the
 * set).  Otherwise, and a wrapper function like this can be used:
 *
 *	static bool jset_nth_index(struct jset *set,
 *				   unsigned long n, unsigned long *idx)
 *	{
 *		// Zero might be valid, if it's first in set.
 *		if (n == 0 && jset_test(set, 0)) {
 *			*idx = 0;
 *			return true;
 *		}
 *		*idx = jset_nth(set, n, 0);
 *		return (*idx != 0);
 *	}
 *
 * Example:
 *	unsigned long i, val;
 *
 *	// We know 0 isn't in set.
 *	assert(!jset_test(set, 0));
 *	for (i = 0; (val = jset_nth(set, i, 0)) != 0; i++) {
 *		assert(jset_popcount(set, 0, val) == i);
 *		printf("Value %lu = %lu\n", i, val);
 *	}
 */
#define jset_nth(set, n, invalid)					\
	tcon_cast(&(set)->jset_, canary,				\
		  jset_nth_(jset_raw((set), (invalid)),			\
			    (n), (unsigned long)(invalid)))

/**
 * jset_first - return the first bit which is set (must not contain 0).
 * @set: bitset from jset_new
 *
 * This is equivalent to jset_nth(set, 0, 0).  ie. useful only if 0
 * isn't in your set.
 *
 * Example:
 *	assert(!jset_test(set, 0));
 *	printf("Set contents (increasing order):");
 *	for (i = jset_first(set); i; i = jset_next(set, i))
 *		printf(" %lu", i);
 *	printf("\n");
 */
#define jset_first(set)						\
	tcon_cast(&(set)->jset_, canary, jset_first_(jset_raw_(set)))

/**
 * jset_next - return the next bit which is set (must not contain 0).
 * @set: bitset from jset_new
 * @prev: previous index
 *
 * This is usually used to find an adjacent index which is set, after
 * jset_first.
 */
#define jset_next(set, prev)						\
	tcon_cast(&(set)->jset_, canary,				\
		  jset_next_(jset_raw_(set), (unsigned long)(prev)))

/**
 * jset_last - return the last bit which is set (must not contain 0).
 * @set: bitset from jset_new
 *
 * Example:
 *	assert(!jset_test(set, 0));
 *	printf("Set contents (decreasing order):");
 *	for (i = jset_last(set); i; i = jset_prev(set, i))
 *		printf(" %lu", i);
 *	printf("\n");
 */
#define jset_last(set)						\
	tcon_cast(&(set)->jset_, canary, jset_last_(jset_raw_(set)))

/**
 * jset_prev - return the previous bit which is set (must not contain 0).
 * @set: bitset from jset_new
 * @prev: previous index
 *
 * This is usually used to find an adjacent bit which is set, after
 * jset_last.
 */
#define jset_prev(set, prev)						\
	tcon_cast(&(set)->jset_, canary,				\
		  jset_prev_(jset_raw_(set), (unsigned long)(prev)))

/**
 * jset_first_clear - return the first bit which is unset
 * @set: bitset from jset_new
 *
 * This allows for iterating the inverse of the bitmap; only returns 0 if the
 * set is full.
 */
#define jset_first_clear(set)						\
	tcon_cast(&(set)->jset_, canary, jset_next_clear_(jset_raw_(set), 0))

#define jset_next_clear(set, prev)					\
	tcon_cast(&(set)->jset_, canary, jset_next_clear_(jset_raw_(set), \
						  (unsigned long)(prev)))

#define jset_last_clear(set)					\
	tcon_cast(&(set)->jset_, canary, jset_last_clear_(jset_raw_(set)))

#define jset_prev_clear(set, prev)					\
	tcon_cast(&(set)->jset_, canary, jset_prev_clear_(jset_raw_(set), \
						  (unsigned long)(prev)))

/* Raw functions */
struct jset *jset_new_(size_t size);
void jset_free_(const struct jset *set);
const char *COLD jset_error_str_(struct jset *set);
static inline const char *jset_error_(struct jset *set)
{
	if (JU_ERRNO(&set->err) <= JU_ERRNO_NFMAX)
		return NULL;
	return jset_error_str_(set);
}
static inline bool jset_test_(const struct jset *set, unsigned long index)
{
	return Judy1Test(set->judy, index, (JError_t *)&set->err);
}
static inline bool jset_set_(struct jset *set, unsigned long index)
{
	return Judy1Set(&set->judy, index, &set->err);
}
static inline bool jset_clear_(struct jset *set, unsigned long index)
{
	return Judy1Unset(&set->judy, index, &set->err);
}
static inline unsigned long jset_popcount_(const struct jset *set,
					   unsigned long start,
					   unsigned long end_incl)
{
	return Judy1Count(set->judy, start, end_incl, (JError_t *)&set->err);
}
static inline unsigned long jset_nth_(const struct jset *set,
				      unsigned long n, unsigned long invalid)
{
	unsigned long index;
	if (!Judy1ByCount(set->judy, n+1, &index, (JError_t *)&set->err))
		index = invalid;
	return index;
}
static inline unsigned long jset_first_(const struct jset *set)
{
	unsigned long index = 0;
	if (!Judy1First(set->judy, &index, (JError_t *)&set->err))
		index = 0;
	else
		assert(index != 0);
	return index;
}
static inline unsigned long jset_next_(const struct jset *set,
				       unsigned long prev)
{
	if (!Judy1Next(set->judy, &prev, (JError_t *)&set->err))
		prev = 0;
	else
		assert(prev != 0);
	return prev;
}
static inline unsigned long jset_last_(const struct jset *set)
{
	unsigned long index = -1;
	if (!Judy1Last(set->judy, &index, (JError_t *)&set->err))
		index = 0;
	else
		assert(index != 0);
	return index;
}
static inline unsigned long jset_prev_(const struct jset *set,
				       unsigned long prev)
{
	if (!Judy1Prev(set->judy, &prev, (JError_t *)&set->err))
		prev = 0;
	else
		assert(prev != 0);
	return prev;
}
static inline unsigned long jset_next_clear_(const struct jset *set,
					     unsigned long prev)
{
	if (!Judy1NextEmpty(set->judy, &prev, (JError_t *)&set->err))
		prev = 0;
	else
		assert(prev != 0);
	return prev;
}
static inline unsigned long jset_last_clear_(const struct jset *set)
{
	unsigned long index = -1;
	if (!Judy1LastEmpty(set->judy, &index, (JError_t *)&set->err))
		index = 0;
	return index;
}
static inline unsigned long jset_prev_clear_(const struct jset *set,
					     unsigned long prev)
{
	if (!Judy1PrevEmpty(set->judy, &prev, (JError_t *)&set->err))
		prev = 0;
	return prev;
}
#endif /* CCAN_JSET_H */
