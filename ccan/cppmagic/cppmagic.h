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

/**
 * CPPMAGIC_ISZERO - is argument '0'
 *
 * CPPMAGIC_ISZERO(@a)
 *	expands to '1' if @a is '0', otherwise expands to '0'.
 */
#define _CPPMAGIC_ISPROBE(...)		CPPMAGIC_2ND(__VA_ARGS__, 0)
#define _CPPMAGIC_PROBE()		$, 1
#define _CPPMAGIC_ISZERO_0		_CPPMAGIC_PROBE()
#define CPPMAGIC_ISZERO(a_)		\
	_CPPMAGIC_ISPROBE(CPPMAGIC_GLUE2(_CPPMAGIC_ISZERO_, a_))

/**
 * CPPMAGIC_NONZERO - is argument not '0'
 *
 * CPPMAGIC_NONZERO(@a)
 *	expands to '0' if @a is '0', otherwise expands to '1'.
 */
#define CPPMAGIC_NONZERO(a_)		CPPMAGIC_ISZERO(CPPMAGIC_ISZERO(a_))

/**
 * CPPMAGIC_NONEMPTY - does the macro have any arguments?
 *
 * CPPMAGIC_NONEMPTY()
 * 	expands to '0'
 * CPPMAGIC_NONEMPTY(@a)
 * CPPMAGIC_NONEMPTY(@a, ...)
 * 	expand to '1'
 */
#define _CPPMAGIC_EOA()			0
#define CPPMAGIC_NONEMPTY(...)		\
	CPPMAGIC_NONZERO(CPPMAGIC_1ST(_CPPMAGIC_EOA __VA_ARGS__)())

/**
 * CPPMAGIC_ISEMPTY - does the macro have no arguments?
 *
 * CPPMAGIC_ISEMPTY()
 * 	expands to '1'
 * CPPMAGIC_ISEMPTY(@a)
 * CPPMAGIC_ISEMPTY(@a, ...)
 * 	expand to '0'
 */
#define CPPMAGIC_ISEMPTY(...)		\
	CPPMAGIC_ISZERO(CPPMAGIC_NONEMPTY(__VA_ARGS__))

/*
 * CPPMAGIC_IFELSE - preprocessor conditional
 *
 * CPPMAGIC_IFELSE(@cond)(@if)(@else)
 *	expands to @else if @cond is '0', otherwise expands to @if
 */
#define _CPPMAGIC_IF_0(...)		_CPPMAGIC_IF_0_ELSE
#define _CPPMAGIC_IF_1(...)		__VA_ARGS__ _CPPMAGIC_IF_1_ELSE
#define _CPPMAGIC_IF_0_ELSE(...)	__VA_ARGS__
#define _CPPMAGIC_IF_1_ELSE(...)
#define _CPPMAGIC_IFELSE(cond_)		CPPMAGIC_GLUE2(_CPPMAGIC_IF_, cond_)
#define CPPMAGIC_IFELSE(cond_)		\
	_CPPMAGIC_IFELSE(CPPMAGIC_NONZERO(cond_))

#endif /* CCAN_CPPMAGIC_H */
