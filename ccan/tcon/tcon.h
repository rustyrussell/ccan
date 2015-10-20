/* CC0 (Public domain) - see LICENSE file for details */
#ifndef CCAN_TCON_H
#define CCAN_TCON_H
#include "config.h"

/**
 * TCON - declare a _tcon type containing canary variables.
 * @decls: the semi-colon separated list of type canaries.
 *
 * This declares a _tcon member for a structure.  It should be the
 * last element in your structure; with sufficient compiler support it
 * will not use any actual storage.  tcon_check() will compare
 * expressions with one of these "type canaries" to cause warnings if
 * the container is misused.
 *
 * A type of "void *" will allow tcon_check() to pass on any (pointer) type.
 *
 * Example:
 *	// Simply typesafe linked list.
 *	struct list_head {
 *		struct list_head *prev, *next;
 *	};
 *
 *	struct string_list {
 *		struct list_head raw;
 *		TCON(char *canary);
 *	};
 *
 *	// More complex: mapping from one type to another.
 *	struct map {
 *		void *contents;
 *	};
 *
 *	struct int_to_string_map {
 *		struct map raw;
 *		TCON(char *charp_canary; int int_canary);
 *	};
 */
#if HAVE_FLEXIBLE_ARRAY_MEMBER
#define TCON(decls) struct { decls; } _tcon[]
#else
#define TCON(decls) struct { decls; } _tcon[1]
#endif

/**
 * TCON_WRAP - declare a wrapper type containing a base type and type canaries
 * @basetype: the base type to wrap
 * @decls: the semi-colon separated list of type canaries.
 *
 * This expands to a new type which includes the given base type, and
 * also type canaries, similar to those created with TCON.
 *
 * The embedded base type value can be accessed using tcon_unwrap().
 *
 * Differences from using TCON()
 * - The wrapper type will take either the size of the base type, or
 *   the size of a single pointer, whichever is greater (regardless of
 *   compiler)
 * - A TCON_WRAP type may be included in another structure, and need
 *   not be the last element.
 *
 * A type of "void *" will allow tcon_check() to pass on any (pointer) type.
 *
 * Example:
 *	// Simply typesafe linked list.
 *	struct list_head {
 *		struct list_head *prev, *next;
 *	};
 *
 *	typedef TCON_WRAP(struct list_head, char *canary) string_list_t;
 *
 *	// More complex: mapping from one type to another.
 *	struct map {
 *		void *contents;
 *	};
 *
 *	typedef TCON_WRAP(struct map, char *charp_canary; int int_canary)
 *		int_to_string_map_t;
 */
#define TCON_WRAP(basetype, decls) \
	union {			   \
		basetype _base;	   \
		struct {	   \
			decls;	   \
		} *_tcon;	   \
	}

/**
 * TCON_WRAP_INIT - an initializer for a variable declared with TCON_WRAP
 * @...: Initializer for the base type (treated as variadic so commas
 *       can be included)
 *
 * Converts converts an initializer suitable for a base type into one
 * suitable for that type wrapped with TCON_WRAP.
 *
 * Example:
 *	TCON_WRAP(int, char *canary) canaried_int = TCON_WRAP_INIT(17);
 */
#define TCON_WRAP_INIT(...)			\
	{ ._base = __VA_ARGS__, }

/**
 * tcon_unwrap - Access the base type of a TCON_WRAP
 * @ptr: pointer to an object declared with TCON_WRAP
 *
 * tcon_unwrap() returns a pointer to the base type of the TCON_WRAP()
 * object pointer to by @ptr.
 *
 * Example:
 *	TCON_WRAP(int, char *canary) canaried_int;
 *
 *	*tcon_unwrap(&canaried_int) = 17;
 */
#define tcon_unwrap(ptr) (&((ptr)->_base))

/**
 * tcon_check - typecheck a typed container
 * @x: the structure containing the TCON.
 * @canary: which canary to check against.
 * @expr: the expression whose type must match the TCON (not evaluated)
 *
 * This macro is used to check that the expression is the type
 * expected for this structure (note the "useless" sizeof() argument
 * which contains this comparison with the type canary).
 *
 * It evaluates to @x so you can chain it.
 *
 * Example:
 *	#define tlist_add(h, n, member) \
 *		list_add(&tcon_check((h), canary, (n))->raw, &(n)->member)
 */
#define tcon_check(x, canary, expr)				\
	(sizeof((x)->_tcon[0].canary == (expr)) ? (x) : (x))

/**
 * tcon_check_ptr - typecheck a typed container
 * @x: the structure containing the TCON.
 * @canary: which canary to check against.
 * @expr: the expression whose type must match &TCON (not evaluated)
 *
 * This macro is used to check that the expression is a pointer to the type
 * expected for this structure (note the "useless" sizeof() argument
 * which contains this comparison with the type canary), or NULL.
 *
 * It evaluates to @x so you can chain it.
 */
#define tcon_check_ptr(x, canary, expr)				\
	(sizeof(&(x)->_tcon[0].canary == (expr)) ? (x) : (x))


/**
 * tcon_type - the type within a container (or void *)
 * @x: the structure containing the TCON.
 * @canary: which canary to check against.
 */
#if HAVE_TYPEOF
#define tcon_type(x, canary) __typeof__((x)->_tcon[0].canary)
#else
#define tcon_type(x, canary) void *
#endif

/**
 * tcon_ptr_type - pointer to the type within a container (or void *)
 * @x: the structure containing the TCON.
 * @canary: which canary to check against.
 */
#if HAVE_TYPEOF
#define tcon_ptr_type(x, canary) __typeof__(&(x)->_tcon[0].canary)
#else
#define tcon_ptr_type(x, canary) void *
#endif

/**
 * tcon_cast - cast to a canary type for this container (or void *)
 * @x: a structure containing the TCON.
 * @canary: which canary to cast to.
 * @expr: the value to cast
 *
 * This is used to cast to the correct type for this container.  If the
 * platform doesn't HAVE_TYPEOF, then it casts to void * (which will
 * cause a warning if the user doesn't expect a pointer type).
 */
#define tcon_cast(x, canary, expr) ((tcon_type((x), canary))(expr))
#define tcon_cast_ptr(x, canary, expr) ((tcon_ptr_type((x), canary))(expr))

#endif /* CCAN_TCON_H */
