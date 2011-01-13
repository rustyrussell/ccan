#ifndef CCAN_HTABLE_TYPE_H
#define CCAN_HTABLE_TYPE_H
#include <ccan/htable/htable.h>
#include "config.h"

/**
 * HTABLE_DEFINE_TYPE - create a set of htable ops for a type
 * @type: a type whose pointers will be values in the hash.
 * @keyof: a function/macro to extract a key from a @type element.
 * @hashfn: a hash function for a @key
 * @cmpfn: a comparison function for two keyof()s.
 * @name: a name for all the functions to define (of form htable_<name>_*)
 *
 * NULL values may not be placed into the hash table.
 *
 * The following wrapper functions are defined; each one is a
 * simplified version of the htable.h equivalent:
 *
 *	// Creating and freeing.
 *	struct htable_@name *htable_@name_new(void);
 *	void htable_@name_free(const struct htable_@name *ht);
 *
 *	// Add, delete and find.
 *	bool htable_@name_add(struct htable_@name *ht, const type *e);
 *	bool htable_@name_del(struct htable_@name *ht, const type *e);
 *	bool htable_@name_delkey(struct htable_@name *ht, const ktype *k);
 *	type *htable_@name_get(const struct htable_@name *ht, const ktype *k);
 *
 *	// Iteration.
 *	struct htable_@name_iter;
 *	type *htable_@name_first(const struct htable_@name *ht,
 *				 struct htable_@name_iter *i);
 *	type *htable_@name_next(const struct htable_@name *ht,
 *				struct htable_@name_iter *i);
 */
#define HTABLE_DEFINE_TYPE(type, keyof, hashfn, cmpfn, name)		\
struct htable_##name;							\
struct htable_##name##_iter { struct htable_iter i; };			\
static inline size_t htable_##name##_hash(const void *elem, void *priv)	\
{									\
	return hashfn(keyof((const type *)elem));			\
}									\
static inline struct htable_##name *htable_##name##_new(void)		\
{									\
	return (struct htable_##name *)htable_new(htable_##name##_hash,	\
						  NULL);		\
}									\
static inline void htable_##name##_free(const struct htable_##name *ht)	\
{									\
	htable_free((const struct htable *)ht);				\
}									\
static inline bool htable_##name##_add(struct htable_##name *ht,	\
				       const type *elem)		\
{									\
	return htable_add((struct htable *)ht, hashfn(keyof(elem)), elem); \
}									\
static inline bool htable_##name##_del(const struct htable_##name *ht,	\
				       const type *elem)		\
{									\
	return htable_del((struct htable *)ht, hashfn(keyof(elem)), elem); \
}									\
static inline type *htable_##name##_get(const struct htable_##name *ht,	\
					const HTABLE_KTYPE(keyof) k)	\
{									\
	/* Typecheck for cmpfn */					\
	(void)sizeof(cmpfn((const type *)NULL,				\
			   keyof((const type *)NULL)));			\
	return (type *)htable_get((const struct htable *)ht,		\
				  hashfn(k),				\
				  (bool (*)(const void *, void *))(cmpfn), \
				  k);					\
}									\
static inline bool htable_##name##_delkey(struct htable_##name *ht,	\
					  const HTABLE_KTYPE(keyof) k) \
{									\
	type *elem = htable_##name##_get(ht, k);			\
	if (elem)							\
		return htable_##name##_del(ht, elem);			\
	return false;							\
}									\
static inline type *htable_##name##_first(const struct htable_##name *ht, \
					  struct htable_##name##_iter *iter) \
{									\
	return htable_first((const struct htable *)ht, &iter->i);	\
}									\
static inline type *htable_##name##_next(const struct htable_##name *ht, \
					 struct htable_##name##_iter *iter) \
{									\
	return htable_next((const struct htable *)ht, &iter->i);	\
}

#if HAVE_TYPEOF
#define HTABLE_KTYPE(keyof) typeof(keyof(NULL))
#else
#define HTABLE_KTYPE(keyof) void *
#endif
#endif /* CCAN_HTABLE_TYPE_H */
