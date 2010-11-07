#ifndef CCAN_JMAP_TYPE_H
#define CCAN_JMAP_TYPE_H
#include <ccan/jmap/jmap.h>

/**
 * JMAP_DEFINE_UINTIDX_TYPE - create a set of jmap ops for integer->ptr map
 * @type: a type whose pointers will be values in the map.
 * @name: a name for all the functions to define (of form jmap_<name>_*)
 *
 * It's easiest if NULL values aren't placed in the map: jmap_@name_get will
 * return NULL if an index isn't valid.
 *
 * The following wrapper functions are defined; each one is the same as
 * the jmap.h generic equivalent except where noted:
 *
 *	// Creating, errors and freeing.
 *	struct jmap_@name *jmap_@name_new(void);
 *	void jmap_@name_free(const struct jmap_@name *map);
 *	const char *jmap_@name_error(struct jmap_@name *map);
 *
 *	// Add, set, delete, test and get.
 *	bool jmap_@name_add(const struct jmap_@name *map,
 *			    unsigned long idx, const type *value);
 *	bool jmap_@name_set(const struct jmap_@name *map,
 *			   unsigned long idx, const type *value);
 *	bool jmap_@name_del(struct jmap_@name *map, unsigned long idx);
 *	bool jmap_@name_test(const struct jmap_@name *map, unsigned long idx);
 *	type *jmap_@name_get(const struct jmap_@name *map, unsigned long idx);
 *
 *	// Counting and iteration.
 *	unsigned long jmap_@name_popcount(const struct jmap_@name *map,
 *					  unsigned long start,
 *					  unsigned long end_incl);
 *	unsigned long jmap_@name_nth(const struct jmap_@name *map,
 *				     unsigned long n, unsigned long invalid);
 *	unsigned long jmap_@name_first(const struct jmap_@name *map,
 *				       unsigned long invalid);
 *	unsigned long jmap_@name_next(const struct jmap_@name *map,
 *				      unsigned long prev,
 *				      unsigned long invalid);
 *	unsigned long jmap_@name_last(const struct jmap_@name *map,
 *				      unsigned long invalid);
 *	unsigned long jmap_@name_prev(const struct jmap_@name *map,
 *				      unsigned long prev,
 *				      unsigned long invalid);
 *
 *	// Get pointers to values to use.
 *	type **jmap_@name_getval(const struct jmap_@name *map,
 *				 unsigned long idx);
 *	void jmap_@name_putval(struct jmap_@name *map, type ***p);
 *	type **jmap_@name_nthval(struct jmap_@name *map,
 *				 unsigned long n, unsigned long *idx);
 *	type **jmap_@name_firstval(const struct jmap_@name *map,
 *				   unsigned long *idx);
 *	type **jmap_@name_nextval(const struct jmap_@name *map,
 *				 unsigned long *idx);
 *	type **jmap_@name_lastval(const struct jmap_@name *map,
 *				  unsigned long *idx);
 *	type **jmap_@name_prevval(const struct jmap_@name *map,
 *				  unsigned long *idx);
 */
#define JMAP_DEFINE_UINTIDX_TYPE(type, name)				\
struct jmap_##name;							\
static inline struct jmap_##name *jmap_##name##_new(void)		\
{									\
	return (struct jmap_##name *)jmap_new();			\
}									\
static inline void jmap_##name##_free(const struct jmap_##name *map)	\
{									\
	jmap_free((const struct jmap *)map);				\
}									\
static inline const char *jmap_##name##_error(struct jmap_##name *map)	\
{									\
	return jmap_error((struct jmap *)map);				\
}									\
static inline bool jmap_##name##_add(struct jmap_##name *map,		\
				     unsigned long idx, const type *value) \
{									\
	return jmap_add((struct jmap *)map, idx, (unsigned long)value);	\
}									\
static inline bool jmap_##name##_set(const struct jmap_##name *map,	\
				     unsigned long idx, const type *value) \
{									\
	return jmap_set((const struct jmap *)map, idx, (unsigned long)value); \
}									\
static inline bool jmap_##name##_del(struct jmap_##name *map,		\
				     unsigned long idx)			\
{									\
	return jmap_del((struct jmap *)map, idx);			\
}									\
static inline bool jmap_##name##_test(const struct jmap_##name *map,	\
				      unsigned long idx)		\
{									\
	return jmap_test((const struct jmap *)map, (unsigned long)idx);	\
}									\
static inline type *jmap_##name##_get(const struct jmap_##name *map,	\
				      unsigned long idx)		\
{									\
	return (type *)jmap_get((const struct jmap *)map, idx, 0);	\
}									\
static inline unsigned long						\
jmap_##name##_popcount(const struct jmap_##name *map,			\
		       unsigned long start, unsigned long end_incl)	\
{									\
	return jmap_popcount((const struct jmap *)map, start, end_incl); \
}									\
static inline unsigned long jmap_##name##_nth(const struct jmap_##name *map, \
					      unsigned long n,		\
					      unsigned long invalid)	\
{									\
	return jmap_nth((const struct jmap *)map, n, invalid);		\
}									\
static inline unsigned long						\
jmap_##name##_first(const struct jmap_##name *map,			\
		    unsigned long invalid)				\
{									\
	return jmap_first((const struct jmap *)map, invalid);		\
}									\
static inline unsigned long						\
jmap_##name##_next(const struct jmap_##name *map,			\
		   unsigned long prev, unsigned long invalid)		\
{									\
	return jmap_next((const struct jmap *)map, prev, invalid);	\
}									\
static inline unsigned long						\
jmap_##name##_last(const struct jmap_##name *map,			\
		   unsigned long invalid)				\
{									\
	return jmap_last((const struct jmap *)map, invalid);		\
}									\
static inline unsigned long						\
jmap_##name##_prev(const struct jmap_##name *map,			\
		   unsigned long prev, unsigned long invalid)		\
{									\
	return jmap_prev((const struct jmap *)map, prev, invalid);	\
}									\
static inline type **jmap_##name##_getval(const struct jmap_##name *map, \
					  unsigned long idx)		\
{									\
	return (type **)jmap_getval((struct jmap *)map, idx);		\
}									\
static inline void jmap_##name##_putval(struct jmap_##name *map,	\
					  type ***p)			\
{									\
	return jmap_putval((struct jmap *)map, (unsigned long **)p);	\
}									\
static inline type **jmap_##name##_nthval(struct jmap_##name *map,	\
					  unsigned long n,		\
					  unsigned long *idx)		\
{									\
	return (type **)jmap_nthval((struct jmap *)map, n, idx);	\
}									\
static inline type **jmap_##name##_firstval(const struct jmap_##name *map, \
					    unsigned long *idx)		\
{									\
	return (type **)jmap_firstval((const struct jmap *)map, idx); \
}									\
static inline type **jmap_##name##_nextval(const struct jmap_##name *map, \
					   unsigned long *idx)		\
{									\
	return (type **)jmap_nextval((const struct jmap *)map, idx);	\
}									\
static inline type **jmap_##name##_lastval(const struct jmap_##name *map, \
					    unsigned long *idx)		\
{									\
	return (type **)jmap_lastval((const struct jmap *)map, idx);	\
}									\
static inline type **jmap_##name##_prevval(const struct jmap_##name *map, \
					   unsigned long *idx)		\
{									\
	return (type **)jmap_prevval((const struct jmap *)map, idx);	\
}

/**
 * JMAP_DEFINE_PTRIDX_TYPE - create a map of jmap ops for ptr->ptr map
 * @itype: a type whose pointers will idx into the map.
 * @type: a type whose pointers will be values in the map.
 * @name: a name for all the functions to define (of form jmap_<name>_*)
 *
 * This macro defines a map of inline functions for typesafe and
 * convenient usage of a pointer-idxed Judy map of pointers.  It is
 * assumed that a NULL pointer is never an idx in the map, as
 * various functions return NULL for "invalid idx".  Similarly,
 * jmap_@name_get will return NULL if an idx isn't valid, so NULL indices
 * are not recommended (though you can tell using jmap_@name_test).
 *
 * Since the ordering is by idx pointer value, it's generally quite useless.
 * Thus we don't define order-specific functions, except first/next for
 * traversal.
 *
 * The following wrapper functions are defined; each one is the same as
 * the jmap.h generic equivalent:
 *
 *	struct jmap_@name *jmap_@name_new(void);
 *	void jmap_@name_free(const struct jmap_@name *map);
 *	const char *jmap_@name_error(struct jmap_@name *map);
 *
 *	bool jmap_@name_add(const struct jmap_@name *map,
 *			    const itype *idx, const type *value);
 *	bool jmap_@name_set(const struct jmap_@name *map,
 *			   const itype *idx, const type *value);
 *	bool jmap_@name_del(struct jmap_@name *map, const itype *idx);
 *	bool jmap_@name_test(const struct jmap_@name *map, const itype *idx);
 *
 *	type *jmap_@name_get(const struct jmap_@name *map, const itype *idx);
 *	itype *jmap_@name_count(const struct jmap_@name *map);
 *	itype *jmap_@name_first(const struct jmap_@name *map);
 *	itype *jmap_@name_next(const struct jmap_@name *map,
 *			       const itype *prev);
 *
 *	type **jmap_@name_getval(const struct jmap_@name *map,
 *				 const itype *idx);
 *	void jmap_@name_putval(struct jmap_@name *map, type ***p);
 *	type **jmap_@name_firstval(const struct jmap_@name *map,
 *				   const itype **idx);
 *	type **jmap_@name_nextval(const struct jmap_@name *map,
 *				  const itype **idx);
 */
#define JMAP_DEFINE_PTRIDX_TYPE(itype, type, name)			\
struct jmap_##name;							\
static inline struct jmap_##name *jmap_##name##_new(void)		\
{									\
	return (struct jmap_##name *)jmap_new();			\
}									\
static inline void jmap_##name##_free(const struct jmap_##name *map)	\
{									\
	jmap_free((const struct jmap *)map);				\
}									\
static inline const char *jmap_##name##_error(struct jmap_##name *map)	\
{									\
	return jmap_error((struct jmap *)map);				\
}									\
static inline bool jmap_##name##_add(struct jmap_##name *map,		\
				     const itype *idx, const type *value) \
{									\
	return jmap_add((struct jmap *)map, (unsigned long)idx,		\
			(unsigned long)value);				\
}									\
static inline bool jmap_##name##_set(const struct jmap_##name *map,	\
				     const itype *idx, const type *value) \
{									\
	return jmap_set((const struct jmap *)map, (unsigned long)idx,	\
			(unsigned long)value);				\
}									\
static inline bool jmap_##name##_del(struct jmap_##name *map,		\
				     const itype *idx)			\
{									\
	return jmap_del((struct jmap *)map, (unsigned long)idx);	\
}									\
static inline bool jmap_##name##_test(const struct jmap_##name *map,	\
				      const itype *idx)			\
{									\
	return jmap_test((const struct jmap *)map, (unsigned long)idx);	\
}									\
static inline type *jmap_##name##_get(const struct jmap_##name *map,	\
				      const itype *idx)			\
{									\
	return (type *)jmap_get((const struct jmap *)map,		\
				(unsigned long)idx, 0);			\
}									\
static inline unsigned long						\
jmap_##name##_count(const struct jmap_##name *map)			\
{									\
	return jmap_popcount((const struct jmap *)map, 0, -1);		\
}									\
static inline itype *jmap_##name##_first(const struct jmap_##name *map)	\
{									\
	return (itype *)jmap_first((const struct jmap *)map, 0);	\
}									\
static inline itype *jmap_##name##_next(const struct jmap_##name *map,	\
					const itype *prev)		\
{									\
	return (itype *)jmap_next((const struct jmap *)map,		\
				  (unsigned long)prev, 0);		\
}									\
static inline type **jmap_##name##_getval(const struct jmap_##name *map, \
					  const itype *idx)		\
{									\
	return (type **)jmap_getval((struct jmap *)map,			\
				    (unsigned long)idx);		\
}									\
static inline void jmap_##name##_putval(struct jmap_##name *map,	\
					type ***p)			\
{									\
	return jmap_putval((struct jmap *)map, (unsigned long **)p);	\
}									\
static inline type **jmap_##name##_firstval(const struct jmap_##name *map, \
					    itype **idx)		\
{									\
	unsigned long i;						\
	type **ret;							\
	ret = (type **)jmap_firstval((const struct jmap *)map, &i);	\
	*idx = (void *)i;						\
	return ret;							\
}									\
static inline type **jmap_##name##_nextval(const struct jmap_##name *map, \
					   itype **idx)		\
{									\
	return (type **)jmap_nextval((const struct jmap *)map,		\
				     (unsigned long *)idx);		\
}
#endif /* CCAN_JMAP_TYPE_H */
