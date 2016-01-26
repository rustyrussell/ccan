/* MIT (BSD) license - see LICENSE file for details */
#ifndef CCAN_CPPMAGIC_H
#define CCAN_CPPMAGIC_H

/**
 * CPPMAGIC_NOTHING - expands to nothing
 */
#define CPPMAGIC_NOTHING()

/**
 * CPPMAGIC_STRINGIFY - convert arguments to a string literal
 */
#define _CPPMAGIC_STRINGIFY(...)	#__VA_ARGS__
#define CPPMAGIC_STRINGIFY(...)		_CPPMAGIC_STRINGIFY(__VA_ARGS__)

/**
 * CPPMAGIC_GLUE2 - glue arguments together
 *
 * CPPMAGIC_GLUE2(@a_, @b_)
 *	expands to the expansion of @a_ followed immediately
 *	(combining tokens) by the expansion of @b_
 */
#define _CPPMAGIC_GLUE2(a_, b_)		a_##b_
#define CPPMAGIC_GLUE2(a_, b_)		_CPPMAGIC_GLUE2(a_, b_)

/**
 * CPPMAGIC_1ST - return 1st argument
 *
 * CPPMAGIC_1ST(@a_, ...)
 *	expands to the expansion of @a_
 */
#define CPPMAGIC_1ST(a_, ...)		a_

/**
 * CPPMAGIC_2ND - return 2nd argument
 *
 * CPPMAGIC_2ST(@a_, @b_, ...)
 *	expands to the expansion of @b_
 */
#define CPPMAGIC_2ND(a_, b_, ...)	b_

#endif /* CCAN_CPPMAGIC_H */
