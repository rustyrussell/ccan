/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef CCAN_JMAP_H
#define CCAN_JMAP_H
#include <ccan/compiler/compiler.h>
#include <ccan/tcon/tcon.h>
#include <stddef.h>
#include <Judy.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#ifdef CCAN_JMAP_DEBUG
#include <stdio.h>
#endif

/**
 * struct map - private definition of a jmap.
 *
 * It's exposed here so you can put it in your structures and so we can
 * supply inline functions.
 */
struct jmap {
	Pvoid_t judy;
	JError_t err;
	const char *errstr;
	/* Used if !NDEBUG */
	int num_accesses;
	/* Used if CCAN_JMAP_DEBUG */
	unsigned long *acc_value;
	unsigned long acc_index;
	const char *funcname;
};

/**
 * JMAP_MEMBERS - declare members for a type-specific jmap.
 * @itype: index type for this map, or void * for any pointer.
 * @ctype: contents type for this map, or void * for any pointer.
 *
 * Example:
 *	struct jmap_long_to_charp {
 *		JMAP_MEMBERS(long, char *);
 *	};
 */
#define JMAP_MEMBERS(itype, ctype)		\
	TCON_WRAP(struct jmap, itype icanary; ctype ccanary) jmap_

/**
 * jmap_new - create a new, empty jmap.
 *
 * See Also:
 *	JMAP_MEMBERS()
 *
 * Example:
 *	struct jmap_long_to_charp {
 *		JMAP_MEMBERS(long, char *);
 *	};
 *
 *	struct jmap_long_to_charp *map = jmap_new(struct jmap_long_to_charp);
 *	if (!map)
 *		errx(1, "Failed to allocate jmap");
 */
#define jmap_new(type) ((type *)jmap_new_(sizeof(type)))

/**
 * jmap_raw_ - unwrap the typed map (no type checking)
 * @map: the typed jmap
 */
#define jmap_raw_(map)	tcon_unwrap(&(map)->jmap_)

/**
 * jmap_free - destroy a jmap.
 * @map: the map returned from jmap_new.
 *
 * Example:
 *	jmap_free(map);
 */
#define jmap_free(map) jmap_free_(jmap_raw_(map))

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
 *	const char *errstr;
 *
 *	errstr = jmap_error(map);
 *	if (errstr)
 *		errx(1, "Woah, error on newly created map?! %s", errstr);
 */
#define jmap_error(map) jmap_error_(jmap_raw_(map))

/**
 * jmap_rawi - unwrap the typed map and check the index type
 * @map: the typed jmap
 * @expr: the expression to check the index type against (not evaluated)
 *
 * This macro usually causes the compiler to emit a warning if the
 * variable is of an unexpected type.  It is used internally where we
 * need to access the raw underlying jmap.
 */
#define jmap_rawi(map, expr) \
	tcon_unwrap(tcon_check(&(map)->jmap_, icanary, (expr)))

/**
 * jmap_rawc - unwrap the typed map and check the contents type
 * @map: the typed jmap
 * @expr: the expression to check the content type against (not evaluated)
 *
 * This macro usually causes the compiler to emit a warning if the
 * variable is of an unexpected type.  It is used internally where we
 * need to access the raw underlying jmap.
 */
#define jmap_rawc(map, expr) \
	tcon_unwrap(tcon_check(&(map)->jmap_, ccanary, (expr)))

/**
 * jmap_rawci - unwrap the typed map and check the index and contents types
 * @map: the typed jmap
 * @iexpr: the expression to check the index type against (not evaluated)
 * @cexpr: the expression to check the contents type against (not evaluated)
 *
 * This macro usually causes the compiler to emit a warning if the
 * variable is of an unexpected type.  It is used internally where we
 * need to access the raw underlying jmap.
 */
#define jmap_rawci(map, iexpr, cexpr)				\
	tcon_unwrap(tcon_check(tcon_check(&(map)->jmap_,\
					  ccanary, (cexpr)), icanary, (iexpr)))

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
 *	if (!jmap_add(map, 0, "hello"))
 *		err(1, "jmap_add failed!");
 */
#define jmap_add(map, index, value)				\
	jmap_add_(jmap_rawci((map), (index), (value)),		\
		  (unsigned long)(index), (unsigned long)value)

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
 *	if (!jmap_set(map, 0, "goodbye"))
 *		err(1, "jmap_set: index 0 not found");
 */
#define jmap_set(map, index, value)				\
	jmap_set_(jmap_rawci((map), (index), (value)),		\
		  (unsigned long)(index), (unsigned long)value)

/**
 * jmap_del - remove an index from the map.
 * @map: map from jmap_new
 * @index: the index to map
 *
 * Example:
 *	if (!jmap_del(map, 0))
 *		err(1, "jmap_del failed!");
 */
#define jmap_del(map, index)						\
	jmap_del_(jmap_rawi((map), (index)), (unsigned long)(index))

/**
 * jmap_test - test if a given index is defined.
 * @map: map from jmap_new
 * @index: the index to find
 *
 * Example:
 *	jmap_add(map, 1, "hello");
 *	assert(jmap_test(map, 1));
 */
#define jmap_test(map, index)				\
	jmap_test_(jmap_rawi((map), (index)), (unsigned long)(index))

/**
 * jmap_get - get a value for a given index.
 * @map: map from jmap_new
 * @index: the index to find
 *
 * Returns 0 if !jmap_test(map, index).
 *
 * Example:
 *	const char *str = "hello";
 *	jmap_add(map, 2, str);
 *	assert(jmap_get(map, 0) == str);
 *
 * See Also:
 *	jmap_getval()
 */
#define jmap_get(map, index)						\
	tcon_cast(&(map)->jmap_, ccanary,				\
		  jmap_get_(jmap_rawi((map), (index)), (unsigned long)(index)))

/**
 * jmap_count - get population of the map.
 * @map: map from jmap_new
 *
 * Example:
 *	assert(jmap_count(map) < 1000);
 */
#define jmap_count(map)				\
	jmap_popcount_(jmap_raw_(map), 0, -1UL)

/**
 * jmap_popcount - get population of (some part of) the map.
 * @map: map from jmap_new
 * @start: first index to count
 * @end_incl: last index to count (use -1 for end).
 *
 * Example:
 *	assert(jmap_popcount(map, 0, 1000) <= jmap_popcount(map, 0, 2000));
 */
#define jmap_popcount(map, start, end_incl)				\
	jmap_popcount_(jmap_rawi((map), (start) ? (start) : (end_incl)), \
		       (unsigned long)(start), (unsigned long)(end_incl))

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
#define jmap_nth(map, n, invalid)					\
	tcon_cast(&(map)->jmap_, icanary,				\
		  jmap_nth_(jmap_rawi((map), (invalid)),		\
			    (n), (unsigned long)(invalid)))

/**
 * jmap_first - return the first index in the map (must not contain 0).
 * @map: map from jmap_new
 *
 * This is equivalent to jmap_nth(map, 0, 0).
 *
 * Example:
 *	assert(!jmap_test(map, 0));
 *	printf("Map indices (increasing order):");
 *	for (i = jmap_first(map); i; i = jmap_next(map, i))
 *		printf(" %lu", i);
 *	printf("\n");
 *
 * See Also:
 *	jmap_firstval()
 */
#define jmap_first(map)						\
	tcon_cast(&(map)->jmap_, icanary, jmap_first_(jmap_raw_(map)))

/**
 * jmap_next - return the next index in the map.
 * @map: map from jmap_new
 * @prev: previous index
 *
 * This is usually used to find an adjacent index after jmap_first.
 * See Also:
 *	jmap_nextval()
 */
#define jmap_next(map, prev)						\
	tcon_cast(&(map)->jmap_, icanary, jmap_next_(jmap_rawi((map), (prev)),	\
						     (unsigned long)(prev)))

/**
 * jmap_last - return the last index in the map.
 * @map: map from jmap_new
 *
 * Returns 0 if map is empty.
 *
 * Example:
 *	assert(!jmap_test(map, 0));
 *	printf("Map indices (increasing order):");
 *	for (i = jmap_last(map); i; i = jmap_prev(map, i))
 *		printf(" %lu", i);
 *	printf("\n");
 * See Also:
 *	jmap_lastval()
 */
#define jmap_last(map)						\
	tcon_cast(&(map)->jmap_, icanary, jmap_last_(jmap_raw_(map)))

/**
 * jmap_prev - return the previous index in the map (must not contain 0)
 * @map: map from jmap_new
 * @prev: previous index
 *
 * This is usually used to find an prior adjacent index after jmap_last.
 * Returns 0 if no previous indices in map.
 *
 * See Also:
 *	jmap_prevval()
 */
#define jmap_prev(map, prev)						\
	tcon_cast(&(map)->jmap_, icanary,				\
		  jmap_prev_(jmap_rawi((map), (prev)), (prev)))

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
 *	char **p;
 *	jmap_add(map, 0, "hello");
 *	p = jmap_getval(map, 0);
 *	if (!p)
 *		errx(1, "Could not find 0 in map!");
 *	if (strcmp(*p, "hello") != 0)
 *		errx(1, "Value in map was not correct?!");
 *	*p = (char *)"goodbye";
 *	jmap_putval(map, &p);
 *	// Accessing p now would probably crash.
 *
 * See Also:
 *	jmap_putval(), jmap_firstval()
 */
#define jmap_getval(map, index)						\
	tcon_cast_ptr(&(map)->jmap_, ccanary,				\
		      jmap_getval_(jmap_rawi((map), (index)),		\
				   (unsigned long)(index)))

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
#define jmap_putval(map, p)						\
	jmap_putval_(jmap_rawc((map), **(p)), (p))

/**
 * jmap_nthval - access the value of the nth value in the map.
 * @map: map from jmap_new
 * @n: which index we are interested in (0-based)
 * @index: set to the nth index in the map.
 *
 * This returns a pointer to the value at the nth index in the map,
 * or NULL if there are n is greater than the population of the map.
 * You must use jmap_putval() on the pointer once you are done with it.
 *
 * Example:
 *	char **val;
 *
 *	// We know 0 isn't in map.
 *	assert(!jmap_test(map, 0));
 *	for (i = 0; (val = jmap_nthval(map, i, &index)) != NULL; i++) {
 *		assert(jmap_popcount(map, 0, index) == i);
 *		printf("Index %lu = %lu, value = %s\n", i, index, *val);
 *		jmap_putval(map, &val);
 *	}
 *
 * See Also:
 *	jmap_nth();
 */
#define jmap_nthval(map, n, index)					\
	tcon_cast_ptr(&(map)->jmap_, ccanary,				\
		      jmap_nthval_(jmap_rawi((map), *(index)), (n), (index)))

/**
 * jmap_firstval - access the first value in the map.
 * @map: map from jmap_new
 * @index: set to the first index in the map.
 *
 * Returns NULL if the map is empty; otherwise this returns a pointer to
 * the first value, which you must call jmap_putval() on!
 *
 * Example:
 *	// Add one to every value (ie. make it point into second char of string)
 *	for (val = jmap_firstval(map, &i); val; val = jmap_nextval(map, &i)) {
 *		(*val)++;
 *		jmap_putval(map, &val);
 *	}
 *	printf("\n");
 *
 * See Also:
 *	jmap_first, jmap_nextval()
 */
#define jmap_firstval(map, index)					\
	tcon_cast_ptr(&(map)->jmap_, ccanary,				\
		      jmap_firstval_(jmap_rawi((map), *(index)),	\
				     (unsigned long *)(index)))

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
#define jmap_nextval(map, index)				\
	tcon_cast_ptr(&(map)->jmap_, ccanary,			\
		      jmap_nextval_(jmap_rawi((map), *(index)),	\
				    (unsigned long *)(index)))


/**
 * jmap_lastval - access the last value in the map.
 * @map: map from jmap_new
 * @index: set to the last index in the map.
 *
 * See Also:
 *	jmap_last(), jmap_putval()
 */
#define jmap_lastval(map, index)				\
	tcon_cast_ptr(&(map)->jmap_, ccanary,			\
		      jmap_lastval_(jmap_rawi((map), *(index)),	\
				    (unsigned long *)(index)))


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
#define jmap_prevval(map, index)				\
	tcon_cast_ptr(&(map)->jmap_, ccanary,			\
		      jmap_prevval_(jmap_rawi((map), *(index)),	\
				    (unsigned long *)(index)))



/* Debugging checks. */
static inline void jmap_debug_add_access(const struct jmap *map,
					 unsigned long index,
					 unsigned long *val,
					 const char *funcname)
{
#ifdef CCAN_JMAP_DEBUG
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
#ifdef CCAN_JMAP_DEBUG
	if (map->acc_value == *val)
		map->acc_value = NULL;
#endif
	/* Set it to some invalid value.  Not NULL, they might rely on that! */
	assert(memset(val, 0x42, sizeof(void *)));
}

static inline void jmap_debug_access(struct jmap *map)
{
#ifdef CCAN_JMAP_DEBUG
	if (map->num_accesses && map->acc_value)
		fprintf(stderr,
			"jmap: still got index %lu, val %lu (%p) from %s\n",
			map->acc_index, *map->acc_value, map->acc_value,
			map->funcname);
#endif
	assert(!map->num_accesses);
}

/* Private functions */
struct jmap *jmap_new_(size_t size);
void jmap_free_(const struct jmap *map);
const char *COLD jmap_error_str_(struct jmap *map);
static inline const char *jmap_error_(struct jmap *map)
{
	if (JU_ERRNO(&map->err) <= JU_ERRNO_NFMAX)
		return NULL;
	return jmap_error_str_(map);
}
static inline bool jmap_add_(struct jmap *map,
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
static inline bool jmap_set_(const struct jmap *map,
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
static inline bool jmap_del_(struct jmap *map, unsigned long index)
{
	jmap_debug_access(map);
	return JudyLDel(&map->judy, index, &map->err) == 1;
}
static inline bool jmap_test_(const struct jmap *map, unsigned long index)
{
	return JudyLGet(map->judy, index, (JError_t *)&map->err) != NULL;
}
static inline unsigned long jmap_get_(const struct jmap *map,
				      unsigned long index)
{
	unsigned long *val;
	val = (unsigned long *)JudyLGet(map->judy, index,
					(JError_t *)&map->err);
	if (!val || val == PJERR)
		return 0;
	return *val;
}
static inline unsigned long jmap_popcount_(const struct jmap *map,
					   unsigned long start,
					   unsigned long end_incl)
{
	return JudyLCount(map->judy, start, end_incl, (JError_t *)&map->err);
}
static inline unsigned long jmap_nth_(const struct jmap *map,
				      unsigned long n, unsigned long invalid)
{
	unsigned long index;
	if (!JudyLByCount(map->judy, n+1, &index, (JError_t *)&map->err))
		index = invalid;
	return index;
}
static inline unsigned long jmap_first_(const struct jmap *map)
{
	unsigned long index = 0;
	if (!JudyLFirst(map->judy, &index, (JError_t *)&map->err))
		index = 0;
	else
		assert(index != 0);
	return index;
}
static inline unsigned long jmap_next_(const struct jmap *map,
				       unsigned long prev)
{
	if (!JudyLNext(map->judy, &prev, (JError_t *)&map->err))
		prev = 0;
	else
		assert(prev != 0);
	return prev;
}
static inline unsigned long jmap_last_(const struct jmap *map)
{
	unsigned long index = -1;
	if (!JudyLLast(map->judy, &index, (JError_t *)&map->err))
		index = 0;
	else
		assert(index != 0);
	return index;
}
static inline unsigned long jmap_prev_(const struct jmap *map,
				       unsigned long prev)
{
	if (!JudyLPrev(map->judy, &prev, (JError_t *)&map->err))
		prev = 0;
	else
		assert(prev != 0);
	return prev;
}
static inline void *jmap_getval_(struct jmap *map, unsigned long index)
{
	unsigned long *val;
	val = (unsigned long *)JudyLGet(map->judy, index,
					(JError_t *)&map->err);
	jmap_debug_add_access(map, index, val, "jmap_getval");
	return val;
}
static inline void jmap_putval_(struct jmap *map, void *p)
{
	jmap_debug_del_access(map, p);
}
static inline unsigned long *jmap_nthval_(const struct jmap *map, unsigned long n,
					  unsigned long *index)
{
	unsigned long *val;
	val = (unsigned long *)JudyLByCount(map->judy, n+1, index,
				     (JError_t *)&map->err);
	jmap_debug_add_access(map, *index, val, "jmap_nthval");
	return val;
}
static inline unsigned long *jmap_firstval_(const struct jmap *map,
					    unsigned long *index)
{
	unsigned long *val;
	*index = 0;
	val = (unsigned long *)JudyLFirst(map->judy, index,
					  (JError_t *)&map->err);
	jmap_debug_add_access(map, *index, val, "jmap_firstval");
	return val;
}
static inline unsigned long *jmap_nextval_(const struct jmap *map,
					   unsigned long *index)
{
	unsigned long *val;
	val = (unsigned long *)JudyLNext(map->judy, index,
					 (JError_t *)&map->err);
	jmap_debug_add_access(map, *index, val, "jmap_nextval");
	return val;
}
static inline unsigned long *jmap_lastval_(const struct jmap *map,
					   unsigned long *index)
{
	unsigned long *val;
	*index = -1;
	val = (unsigned long *)JudyLLast(map->judy, index,
					 (JError_t *)&map->err);
	jmap_debug_add_access(map, *index, val, "jmap_lastval");
	return val;
}
static inline unsigned long *jmap_prevval_(const struct jmap *map,
					   unsigned long *index)
{
	unsigned long *val;
	val = (unsigned long *)JudyLPrev(map->judy, index,
					 (JError_t *)&map->err);
	jmap_debug_add_access(map, *index, val, "jmap_prevval");
	return val;
}
#endif /* CCAN_JMAP_H */
