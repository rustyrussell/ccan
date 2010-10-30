#ifndef CCAN_JBITSET_H
#define CCAN_JBITSET_H
#include <Judy.h>
#include <stdbool.h>
#include <ccan/compiler/compiler.h>
#include <assert.h>

/**
 * jbit_new - create a new, empty jbitset.
 *
 * See Also:
 *	JBIT_DEFINE_TYPE()
 *
 * Example:
 *	struct jbitset *set = jbit_new();
 *	if (!set)
 *		errx(1, "Failed to allocate jbitset");
 */
struct jbitset *jbit_new(void);

/**
 * jbit_free - destroy a jbitset.
 * @set: the set returned from jbit_new.
 *
 * Example:
 *	jbit_free(set);
 */
void jbit_free(const struct jbitset *set);

/* This is exposed in the header so we can inline.  Treat it as private! */
struct jbitset {
	void *judy;
	JError_t err;
	const char *errstr;
};
const char *COLD_ATTRIBUTE jbit_error_(struct jbitset *set);

/**
 * jbit_error - test for an error in the a previous jbit_ operation.
 * @set: the set to test.
 *
 * Under normal circumstances, return NULL to indicate no error has occurred.
 * Otherwise, it will return a string containing the error.  This string
 * can only be freed by jbit_free() on the set.
 *
 * Other than out-of-memory, errors are caused by memory corruption or
 * interface misuse.
 *
 * Example:
 *	struct jbitset *set = jbit_new();
 *	const char *errstr;
 *
 *	if (!set)
 *		err(1, "allocating jbitset");
 *	errstr = jbit_error(set);
 *	if (errstr)
 *		errx(1, "Woah, error on newly created set?! %s", errstr);
 */
static inline const char *jbit_error(struct jbitset *set)
{
	if (JU_ERRNO(&set->err) <= JU_ERRNO_NFMAX)
		return NULL;
	return jbit_error_(set);
}

/**
 * jbit_test - test a bit in the bitset.
 * @set: bitset from jbit_new
 * @index: the index to test
 *
 * Returns true if jbit_set() has been done on this index, false otherwise.
 *
 * Example:
 *	assert(!jbit_test(set, 0));
 */
static inline bool jbit_test(const struct jbitset *set, size_t index)
{
	return Judy1Test(set->judy, index, (JError_t *)&set->err);
}

/**
 * jbit_set - set a bit in the bitset.
 * @set: bitset from jbit_new
 * @index: the index to set
 *
 * Returns false if it was already set (ie. nothing changed)
 *
 * Example:
 *	if (jbit_set(set, 0))
 *		err(1, "Bit 0 was already set?!");
 */
static inline bool jbit_set(struct jbitset *set, size_t index)
{
	return Judy1Set(&set->judy, index, &set->err);
}

/**
 * jbit_clear - clear a bit in the bitset.
 * @set: bitset from jbit_new
 * @index: the index to set
 *
 * Returns the old bit value (ie. false if nothing changed).
 *
 * Example:
 *	if (jbit_clear(set, 0))
 *		err(1, "Bit 0 was already clear?!");
 */
static inline bool jbit_clear(struct jbitset *set, size_t index)
{
	return Judy1Unset(&set->judy, index, &set->err);
}

/**
 * jbit_popcount - get population of (some part of) bitset.
 * @set: bitset from jbit_new
 * @start: first index to count
 * @end_incl: last index to count (use -1 for end).
 *
 * Example:
 *	assert(jbit_popcount(set, 0, 1000) <= jbit_popcount(set, 0, 2000));
 */
static inline size_t jbit_popcount(const struct jbitset *set,
				   size_t start, size_t end_incl)
{
	return Judy1Count(set->judy, start, end_incl, (JError_t *)&set->err);
}

/**
 * jbit_nth - return the index of the nth bit which is set.
 * @set: bitset from jbit_new
 * @n: which bit we are interested in (0-based)
 * @invalid: what to return if n >= set population
 *
 * This normally returns the nth bit in the set, and often there is a
 * convenient known-invalid value (ie. something which is never in the
 * set).  Otherwise, and a wrapper function like this can be used:
 *
 *	static bool jbit_nth_index(struct jbitset *set, size_t n, size_t *idx)
 *	{
 *		// Zero might be valid, if it's first in set.
 *		if (n == 0 && jbit_test(set, 0)) {
 *			*idx = 0;
 *			return true;
 *		}
 *		*idx = jbit_nth(set, n, 0);
 *		return (*idx != 0);
 *	}
 *
 * Example:
 *	size_t i, val;
 *
 *	// We know 0 isn't in set.
 *	assert(!jbit_test(set, 0));
 *	for (i = 0; (val = jbit_nth(set, i, 0)) != 0; i++) {
 *		assert(jbit_popcount(set, 0, val) == i);
 *		printf("Value %zu = %zu\n", i, val);
 *	}
 */
static inline size_t jbit_nth(const struct jbitset *set,
			      size_t n, size_t invalid)
{
	Word_t index;
	if (!Judy1ByCount(set->judy, n+1, &index, (JError_t *)&set->err))
		index = invalid;
	return index;
}

/**
 * jbit_first - return the first bit which is set.
 * @set: bitset from jbit_new
 * @invalid: return value if no bits are set at all.
 *
 * This is equivalent to jbit_nth(set, 0, invalid).
 *
 * Example:
 *	assert(!jbit_test(set, 0));
 *	printf("Set contents (increasing order):");
 *	for (i = jbit_first(set, 0); i; i = jbit_next(set, i, 0))
 *		printf(" %zu", i);
 *	printf("\n");
 */
static inline size_t jbit_first(const struct jbitset *set, size_t invalid)
{
	Word_t index = 0;
	if (!Judy1First(set->judy, &index, (JError_t *)&set->err))
		index = invalid;
	else
		assert(index != invalid);
	return index;
}

/**
 * jbit_next - return the next bit which is set.
 * @set: bitset from jbit_new
 * @prev: previous index
 * @invalid: return value if no bits are set at all.
 *
 * This is usually used to find an adjacent bit which is set, after
 * jbit_first.
 */
static inline size_t jbit_next(const struct jbitset *set, size_t prev,
			       size_t invalid)
{
	if (!Judy1Next(set->judy, (Word_t *)&prev, (JError_t *)&set->err))
		prev = invalid;
	else
		assert(prev != invalid);
	return prev;
}

/**
 * jbit_last - return the last bit which is set.
 * @set: bitset from jbit_new
 * @invalid: return value if no bits are set at all.
 *
 * Example:
 *	assert(!jbit_test(set, 0));
 *	printf("Set contents (decreasing order):");
 *	for (i = jbit_last(set, 0); i; i = jbit_prev(set, i, 0))
 *		printf(" %zu", i);
 *	printf("\n");
 */
static inline size_t jbit_last(const struct jbitset *set, size_t invalid)
{
	Word_t index = -1;
	if (!Judy1Last(set->judy, &index, (JError_t *)&set->err))
		index = invalid;
	else
		assert(index != invalid);
	return index;
}

/**
 * jbit_prev - return the previous bit which is set.
 * @set: bitset from jbit_new
 * @prev: previous index
 * @invalid: return value if no bits are set at all.
 *
 * This is usually used to find an adjacent bit which is set, after
 * jbit_last.
 */
static inline size_t jbit_prev(const struct jbitset *set, size_t prev,
			       size_t invalid)
{
	if (!Judy1Prev(set->judy, (Word_t *)&prev, (JError_t *)&set->err))
		prev = invalid;
	else
		assert(prev != invalid);
	return prev;
}

/**
 * jbit_first_clear - return the first bit which is unset.
 * @set: bitset from jbit_new
 * @invalid: return value if no bits are clear at all.
 *
 * This allows for iterating the inverse of the bitmap.
 */
static inline size_t jbit_first_clear(const struct jbitset *set,
				      size_t invalid)
{
	Word_t index = 0;
	if (!Judy1FirstEmpty(set->judy, &index, (JError_t *)&set->err))
		index = invalid;
	else
		assert(index != invalid);
	return index;
}

static inline size_t jbit_next_clear(const struct jbitset *set, size_t prev,
				     size_t invalid)
{
	if (!Judy1NextEmpty(set->judy, (Word_t *)&prev, (JError_t *)&set->err))
		prev = invalid;
	else
		assert(prev != invalid);
	return prev;
}

static inline size_t jbit_last_clear(const struct jbitset *set, size_t invalid)
{
	Word_t index = -1;
	if (!Judy1LastEmpty(set->judy, &index, (JError_t *)&set->err))
		index = invalid;
	else
		assert(index != invalid);
	return index;
}

static inline size_t jbit_prev_clear(const struct jbitset *set, size_t prev,
				     size_t invalid)
{
	if (!Judy1PrevEmpty(set->judy, (Word_t *)&prev, (JError_t *)&set->err))
		prev = invalid;
	else
		assert(prev != invalid);
	return prev;
}
#endif /* CCAN_JBITSET_H */
