#ifndef CCAN_JMAP_H
#define CCAN_JMAP_H
#include <stddef.h>
#include <Judy.h>
#include <stdbool.h>
#include <string.h>
#include <ccan/compiler/compiler.h>
#include <assert.h>
#ifdef DEBUG
#include <stdio.h>
#endif

/**
 * jmap_new - create a new, empty jmap.
 *
 * See Also:
 *	JMAP_DEFINE_TYPE()
 *
 * Example:
 *	struct jmap *map = jmap_new();
 *	if (!map)
 *		errx(1, "Failed to allocate jmap");
 */
struct jmap *jmap_new(void);

/**
 * jmap_free - destroy a jmap.
 * @map: the map returned from jmap_new.
 *
 * Example:
 *	jmap_free(map);
 */
void jmap_free(const struct jmap *map);

/* This is exposed in the header so we can inline.  Treat it as private! */
struct jmap {
	Pvoid_t judy;
	JError_t err;
	const char *errstr;
	/* Used if !NDEBUG */
	int num_accesses;
	/* Used if DEBUG */
	unsigned long *acc_value;
	unsigned long acc_index;
	const char *funcname;
};
const char *COLD jmap_error_(struct jmap *map);

/* Debugging checks. */
static inline void jmap_debug_add_access(const struct jmap *map,
					 unsigned long index,
					 unsigned long *val,
					 const char *funcname)
{
#ifdef DEBUG
	if (!map->acc_value) {
		((struct jmap *)map)->acc_value = val;
		((struct jmap *)map)->acc_index = index;
		((struct jmap *)map)->funcname = funcname;
	}
#endif
	if (val)
		assert(++((struct jmap *)map)->num_accesses);
}

static inline void jmap_debug_del_access(struct jmap *map, unsigned long **val)
{
	assert(--map->num_accesses >= 0);
#ifdef DEBUG
	if (map->acc_value == *val)
		map->acc_value = NULL;
#endif
	/* Set it to some invalid value.  Not NULL, they might rely on that! */
	assert(memset(val, 0x42, sizeof(*val)));
}

static inline void jmap_debug_access(struct jmap *map)
{
#ifdef DEBUG
	if (map->num_accesses && map->acc_value)
		fprintf(stderr,
			"jmap: still got index %lu, val %lu (%p) from %s\n",
			map->acc_index, *map->acc_value, map->acc_value,
			map->funcname);
#endif
	assert(!map->num_accesses);
}

/**
 * jmap_error - test for an error in the a previous jmap_ operation.
 * @map: the map to test.
 *
 * Under normal circumstances, return NULL to indicate no error has occurred.
 * Otherwise, it will return a string containing the error.  This string
 * can only be freed by jmap_free() on the map.
 *
 * Other than out-of-memory, errors are caused by memory corruption or
 * interface misuse.
 *
 * Example:
 *	struct jmap *map = jmap_new();
 *	const char *errstr;
 *
 *	if (!map)
 *		err(1, "allocating jmap");
 *	errstr = jmap_error(map);
 *	if (errstr)
 *		errx(1, "Woah, error on newly created map?! %s", errstr);
 */
static inline const char *jmap_error(struct jmap *map)
{
	if (JU_ERRNO(&map->err) <= JU_ERRNO_NFMAX)
		return NULL;
	return jmap_error_(map);
}

/**
 * jmap_add - add or replace a value for a given index in the map.
 * @map: map from jmap_new
 * @index: the index to map
 * @value: the value to associate with the index
 *
 * Adds index into the map; replaces value if it's already there.
 * Returns false on error (out of memory).
 *
 * Example:
 *	if (!jmap_add(map, 0, 1))
 *		err(1, "jmap_add failed!");
 */
static inline bool jmap_add(struct jmap *map,
			    unsigned long index, unsigned long value)
{
	unsigned long *val;
	jmap_debug_access(map);
	val = (unsigned long *)JudyLIns(&map->judy, index, &map->err);
	if (val == PJERR)
		return false;
	*val = value;
	return true;
}

/**
 * jmap_set - change a value for an existing index in the map.
 * @map: map from jmap_new
 * @index: the index to map
 * @value: the value to associate with the index
 *
 * This sets the value of an index if it already exists, and return true,
 * otherwise returns false and does nothing.
 *
 * Example:
 *	if (!jmap_set(map, 0, 2))
 *		err(1, "jmap_set: index 0 not found");
 */
static inline bool jmap_set(const struct jmap *map,
			    unsigned long index, unsigned long value)
{
	unsigned long *val;
	val = (unsigned long *)JudyLGet(map->judy, index,
					(JError_t *)&map->err);
	if (val && val != PJERR) {
		*val = value;
		return true;
	}
	return false;
}

/**
 * jmap_del - remove an index from the map.
 * @map: map from jmap_new
 * @index: the index to map
 *
 * Example:
 *	if (!jmap_del(map, 0))
 *		err(1, "jmap_del failed!");
 */
static inline bool jmap_del(struct jmap *map, unsigned long index)
{
	jmap_debug_access(map);
	return JudyLDel(&map->judy, index, &map->err) == 1;
}

/**
 * jmap_test - test if a given index is defined.
 * @map: map from jmap_new
 * @index: the index to find
 *
 * Example:
 *	jmap_add(map, 0, 1);
 *	assert(jmap_test(map, 0));
 */
static inline bool jmap_test(const struct jmap *map, unsigned long index)
{
	return JudyLGet(map->judy, index, (JError_t *)&map->err) != NULL;
}

/**
 * jmap_get - get a value for a given index.
 * @map: map from jmap_new
 * @index: the index to find
 * @invalid: the value to return if the index isn't found.
 *
 * Example:
 *	jmap_add(map, 0, 1);
 *	assert(jmap_get(map, 0, -1) == 1);
 *
 * See Also:
 *	jmap_getval()
 */
static inline unsigned long jmap_get(const struct jmap *map,
				     unsigned long index,
				     unsigned long invalid)
{
	unsigned long *val;
	val = (unsigned long *)JudyLGet(map->judy, index,
					(JError_t *)&map->err);
	if (!val || val == PJERR)
		return invalid;
	return *val;
}

/**
 * jmap_popcount - get population of (some part of) the map.
 * @map: map from jmap_new
 * @start: first index to count
 * @end_incl: last index to count (use -1 for end).
 *
 * Example:
 *	assert(jmap_popcount(map, 0, 1000) <= jmap_popcount(map, 0, 2000));
 */
static inline unsigned long jmap_popcount(const struct jmap *map,
					  unsigned long start,
					  unsigned long end_incl)
{
	return JudyLCount(map->judy, start, end_incl, (JError_t *)&map->err);
}

/**
 * jmap_nth - return the index of the nth value in the map.
 * @map: map from jmap_new
 * @n: which index we are interested in (0-based)
 * @invalid: what to return if n >= map population
 *
 * This normally returns the nth index in the map, and often there is a
 * convenient known-invalid value (ie. something which is never in the
 * map).  Otherwise you can use jmap_nthval().
 *
 * Example:
 *	unsigned long i, index;
 *
 *	// We know 0 isn't in map.
 *	assert(!jmap_test(map, 0));
 *	for (i = 0; (index = jmap_nth(map, i, 0)) != 0; i++) {
 *		assert(jmap_popcount(map, 0, index) == i);
 *		printf("Index %lu = %lu\n", i, index);
 *	}
 *
 * See Also:
 *	jmap_nthval();
 */
static inline unsigned long jmap_nth(const struct jmap *map,
				     unsigned long n, unsigned long invalid)
{
	unsigned long index;
	if (!JudyLByCount(map->judy, n+1, &index, (JError_t *)&map->err))
		index = invalid;
	return index;
}

/**
 * jmap_first - return the first index in the map.
 * @map: map from jmap_new
 * @invalid: return value if jmap is empty.
 *
 * This is equivalent to jmap_nth(map, 0, invalid).
 *
 * Example:
 *	assert(!jmap_test(map, 0));
 *	printf("Map indices (increasing order):");
 *	for (i = jmap_first(map, 0); i; i = jmap_next(map, i, 0))
 *		printf(" %lu", i);
 *	printf("\n");
 *
 * See Also:
 *	jmap_firstval()
 */
static inline unsigned long jmap_first(const struct jmap *map,
				       unsigned long invalid)
{
	unsigned long index = 0;
	if (!JudyLFirst(map->judy, &index, (JError_t *)&map->err))
		index = invalid;
	else
		assert(index != invalid);
	return index;
}

/**
 * jmap_next - return the next index in the map.
 * @map: map from jmap_new
 * @prev: previous index
 * @invalid: return value if there prev was final index in map.
 *
 * This is usually used to find an adjacent index after jmap_first.
 * See Also:
 *	jmap_nextval()
 */
static inline unsigned long jmap_next(const struct jmap *map,
				      unsigned long prev,
				      unsigned long invalid)
{
	if (!JudyLNext(map->judy, &prev, (JError_t *)&map->err))
		prev = invalid;
	else
		assert(prev != invalid);
	return prev;
}

/**
 * jmap_last - return the last index in the map.
 * @map: map from jmap_new
 * @invalid: return value if map is empty.
 *
 * Example:
 *	assert(!jmap_test(map, 0));
 *	printf("Map indices (increasing order):");
 *	for (i = jmap_last(map, 0); i; i = jmap_prev(map, i, 0))
 *		printf(" %lu", i);
 *	printf("\n");
 * See Also:
 *	jmap_lastval()
 */
static inline unsigned long jmap_last(const struct jmap *map,
				      unsigned long invalid)
{
	unsigned long index = -1;
	if (!JudyLLast(map->judy, &index, (JError_t *)&map->err))
		index = invalid;
	else
		assert(index != invalid);
	return index;
}

/**
 * jmap_prev - return the previous index in the map.
 * @map: map from jmap_new
 * @prev: previous index
 * @invalid: return value if no previous indices are in the map.
 *
 * This is usually used to find an prior adjacent index after jmap_last.
 * See Also:
 *	jmap_prevval()
 */
static inline unsigned long jmap_prev(const struct jmap *map,
				      unsigned long prev,
				      unsigned long invalid)
{
	if (!JudyLPrev(map->judy, &prev, (JError_t *)&map->err))
		prev = invalid;
	else
		assert(prev != invalid);
	return prev;
}

/**
 * jmap_getval - access a value in-place for a given index.
 * @map: map from jmap_new
 * @index: the index to find
 *
 * Returns a pointer into the map, or NULL if the index isn't in the
 * map.  Like the other val functions (jmap_nthval, jmap_firstval
 * etc), this pointer cannot be used after a jmap_add or jmap_del
 * call, and you must call jmap_putval() once you are finished.
 *
 * Unless you define NDEBUG, jmap_add and kmap_del will check that you
 * have called jmap_putval().
 *
 * Example:
 *	unsigned long *p;
 *	jmap_add(map, 0, 1);
 *	p = jmap_getval(map, 0);
 *	if (!p)
 *		errx(1, "Could not find 0 in map!");
 *	if (*p != 1)
 *		errx(1, "Value in map was not 0?!");
 *	*p = 7;
 *	jmap_putval(map, &p);
 *	// Accessing p now would probably crash.
 *
 * See Also:
 *	jmap_putval(), jmap_firstval()
 */
static inline unsigned long *jmap_getval(struct jmap *map, unsigned long index)
{
	unsigned long *val;
	val = (unsigned long *)JudyLGet(map->judy, index,
					(JError_t *)&map->err);
	jmap_debug_add_access(map, index, val, "jmap_getval");
	return val;
}

/**
 * jmap_putval - revoke access to a value.
 * @map: map from jmap_new
 * @p: the pointer to a pointer to the value
 *
 * @p is a pointer to the (successful) value retuned from one of the
 * jmap_*val functions (listed below).  After this, it will be invalid.
 *
 * Unless NDEBUG is defined, this will actually alter the value of p
 * to point to garbage to help avoid accidental use.
 *
 * See Also:
 *	jmap_getval(), jmap_nthval(), jmap_firstval(), jmap_nextval(),
 *		jmap_lastval(), jmap_prevval().
 */
static inline void jmap_putval(struct jmap *map, unsigned long **p)
{
	jmap_debug_del_access(map, p);
}

/**
 * jmap_nthval - access the value of the nth value in the map.
 * @map: map from jmap_new
 * @n: which index we are interested in (0-based)
 *
 * This returns a pointer to the value at the nth index in the map,
 * or NULL if there are n is greater than the population of the map.
 * You must use jmap_putval() on the pointer once you are done with it.
 *
 * Example:
 *	unsigned long *val;
 *
 *	// We know 0 isn't in map.
 *	assert(!jmap_test(map, 0));
 *	for (i = 0; (val = jmap_nthval(map, i, &index)) != NULL; i++) {
 *		assert(jmap_popcount(map, 0, index) == i);
 *		printf("Index %lu = %lu, value = %lu\n", i, index, *val);
 *		jmap_putval(map, &val);
 *	}
 *
 * See Also:
 *	jmap_nth();
 */
static inline unsigned long *jmap_nthval(const struct jmap *map,
					 unsigned long n, unsigned long *index)
{
	unsigned long *val;
	val = (unsigned long *)JudyLByCount(map->judy, n+1, index,
				     (JError_t *)&map->err);
	jmap_debug_add_access(map, *index, val, "jmap_nthval");
	return val;
}

/**
 * jmap_firstval - access the first value in the map.
 * @map: map from jmap_new
 * @index: set to the first index in the map.
 *
 * Returns NULL if the map is empty; otherwise this returns a pointer to
 * the first value, which you must call jmap_putval() on!
 *
 * Example:
 *	// Add one to every value.
 *	for (val = jmap_firstval(map, &i); val; val = jmap_nextval(map, &i)) {
 *		(*val)++;
 *		jmap_putval(map, &val);
 *	}
 *	printf("\n");
 *
 * See Also:
 *	jmap_first, jmap_nextval()
 */
static inline unsigned long *jmap_firstval(const struct jmap *map,
					   unsigned long *index)
{
	unsigned long *val;
	*index = 0;
	val = (unsigned long *)JudyLFirst(map->judy, index,
					  (JError_t *)&map->err);
	jmap_debug_add_access(map, *index, val, "jmap_firstval");
	return val;
}

/**
 * jmap_nextval - access the next value in the map.
 * @map: map from jmap_new
 * @index: previous index, updated with the new index.
 *
 * This returns a pointer to a value (which you must call jmap_putval on)
 * or NULL.  This usually used to find an adjacent value after jmap_firstval.
 *
 * See Also:
 *	jmap_firstval(), jmap_putval()
 */
static inline unsigned long *jmap_nextval(const struct jmap *map,
					  unsigned long *index)
{
	unsigned long *val;
	val = (unsigned long *)JudyLNext(map->judy, index,
					 (JError_t *)&map->err);
	jmap_debug_add_access(map, *index, val, "jmap_nextval");
	return val;
}

/**
 * jmap_lastval - access the last value in the map.
 * @map: map from jmap_new
 * @index: set to the last index in the map.
 *
 * See Also:
 *	jmap_last(), jmap_putval()
 */
static inline unsigned long *jmap_lastval(const struct jmap *map,
					  unsigned long *index)
{
	unsigned long *val;
	*index = -1;
	val = (unsigned long *)JudyLLast(map->judy, index,
					 (JError_t *)&map->err);
	jmap_debug_add_access(map, *index, val, "jmap_lastval");
	return val;
}

/**
 * jmap_prevval - access the previous value in the map.
 * @map: map from jmap_new
 * @index: previous index, updated with the new index.
 *
 * This returns a pointer to a value (which you must call jmap_putval on)
 * or NULL.  This usually used to find an adjacent value after jmap_lastval.
 *
 * See Also:
 *	jmap_lastval(), jmap_putval()
 */
static inline unsigned long *jmap_prevval(const struct jmap *map,
					  unsigned long *index)
{
	unsigned long *val;
	val = (unsigned long *)JudyLPrev(map->judy, index,
					 (JError_t *)&map->err);
	jmap_debug_add_access(map, *index, val, "jmap_prevval");
	return val;
}
#endif /* CCAN_JMAP_H */
