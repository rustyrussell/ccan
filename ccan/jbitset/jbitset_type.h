/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef CCAN_JBITSET_TYPE_H
#define CCAN_JBITSET_TYPE_H
#include <ccan/jbitset/jbitset.h>

/**
 * JBIT_DEFINE_TYPE - create a set of jbit ops for a given pointer type
 * @type: a type whose pointers will go into the bitset.
 * @name: a name for all the functions to define (of form jbit_<name>_*)
 *
 * This macro defines a set of inline functions for typesafe and convenient
 * usage of a Judy bitset for pointers.  It is assumed that a NULL pointer
 * is never set in the bitset.
 *
 * Example:
 *	JBIT_DEFINE_TYPE(char, char);
 *	JBIT_DEFINE_TYPE(struct foo, foo);
 *
 *	static struct jbitset_char *jc;
 *	struct jbitset_foo *jf;
 *
 *	static void add_to_bitsets(const char *p, const struct foo *f)
 *	{
 *		// Note, this adds the pointer, not the string!
 *		jbit_char_set(jc, p);
 *		jbit_foo_set(jf, f);
 *	}
 */
#define JBIT_DEFINE_TYPE(type, name)					\
struct jbitset_##name;							\
static inline struct jbitset_##name *jbit_##name##_new(void)		\
{									\
	return (struct jbitset_##name *)jbit_new();			\
}									\
static inline void jbit_##name##_free(const struct jbitset_##name *set)	\
{									\
	jbit_free((const struct jbitset *)set);				\
}									\
static inline const char *jbit_##name##_error(struct jbitset_##name *set) \
{									\
	return jbit_error((struct jbitset *)set);			\
}									\
static inline bool jbit_##name##_test(const struct jbitset_##name *set,	\
				      const type *index)		\
{									\
	return jbit_test((const struct jbitset *)set, (size_t)index);	\
}									\
static inline bool jbit_##name##_set(struct jbitset_##name *set,	\
				     const type *index)			\
{									\
	return jbit_set((struct jbitset *)set, (size_t)index);		\
}									\
static inline bool jbit_##name##_clear(struct jbitset_##name *set,	\
				       const type *index)		\
{									\
	return jbit_clear((struct jbitset *)set, (size_t)index);	\
}									\
static inline size_t jbit_##name##_count(struct jbitset_##name *set)	\
{									\
	return jbit_popcount((const struct jbitset *)set, 0, -1);	\
}									\
static inline type *jbit_##name##_nth(const struct jbitset_##name *set,	\
					    size_t n)			\
{									\
	return (type *)jbit_nth((const struct jbitset *)set, n, 0);	\
}									\
static inline type *jbit_##name##_first(const struct jbitset_##name *set) \
{									\
	return (type *)jbit_first((const struct jbitset *)set, 0);	\
}									\
static inline type *jbit_##name##_next(struct jbitset_##name *set,	\
				       const type *prev)		\
{									\
	return (type *)jbit_next((const struct jbitset *)set, (size_t)prev, 0);	\
}									\
static inline type *jbit_##name##_last(struct jbitset_##name *set)	\
{									\
	return (type *)jbit_last((const struct jbitset *)set, 0);	\
}									\
static inline type *jbit_##name##_prev(struct jbitset_##name *set,	\
				       const type *prev)		\
{									\
	return (type *)jbit_prev((const struct jbitset *)set, (size_t)prev, 0);	\
}
#endif /* CCAN_JBITSET_TYPE_H */
