#ifndef CCAN_COMPILER_H
#define CCAN_COMPILER_H
#include "config.h"

#if HAVE_ATTRIBUTE_COLD
/**
 * COLD_ATTRIBUTE - a function is unlikely to be called.
 *
 * Used to mark an unlikely code path and optimize appropriately.
 * It is usually used on logging or error routines.
 *
 * Example:
 * static void COLD_ATTRIBUTE moan(const char *reason)
 * {
 *	fprintf(stderr, "Error: %s (%s)\n", reason, strerror(errno));
 * }
 */
#define COLD_ATTRIBUTE __attribute__((cold))
#else
#define COLD_ATTRIBUTE
#endif

#if HAVE_ATTRIBUTE_PRINTF
/**
 * PRINTF_ATTRIBUTE - a function takes printf-style arguments
 * @nfmt: the 1-based number of the function's format argument.
 * @narg: the 1-based number of the function's first variable argument.
 *
 * This allows the compiler to check your parameters as it does for printf().
 *
 * Example:
 * void PRINTF_ATTRIBUTE(2,3) my_printf(const char *prefix,
 *					const char *fmt, ...);
 */
#define PRINTF_ATTRIBUTE(nfmt, narg) \
	__attribute__((format(__printf__, nfmt, narg)))
#else
#define PRINTF_ATTRIBUTE(nfmt, narg)
#endif

#if HAVE_ATTRIBUTE_CONST
/**
 * IDEMPOTENT_ATTRIBUTE - a function's return depends only on its argument
 *
 * This allows the compiler to assume that the function will return the exact
 * same value for the exact same arguments.  This implies that the function
 * must not use global variables, or dereference pointer arguments.
 */
#define IDEMPOTENT_ATTRIBUTE __attribute__((const))
#else
#define IDEMPOTENT_ATTRIBUTE
#endif

#if HAVE_ATTRIBUTE_UNUSED
/**
 * UNNEEDED_ATTRIBUTE - a parameter/variable/function may not be needed
 *
 * This suppresses warnings about unused variables or parameters, but tells
 * the compiler that if it is unused it need not emit it into the source code.
 *
 * Example:
 * // With some preprocessor options, this is unnecessary.
 * static UNNEEDED_ATTRIBUTE int counter;
 *
 * // With some preprocessor options, this is unnecessary.
 * static UNNEEDED_ATTRIBUTE void add_to_counter(int add)
 * {
 *	counter += add;
 * }
 */
#define UNNEEDED_ATTRIBUTE __attribute__((unused))

#if HAVE_ATTRIBUTE_USED
/**
 * NEEDED_ATTRIBUTE - a parameter/variable/function is needed
 *
 * This suppresses warnings about unused variables or parameters, but tells
 * the compiler that it must exist even if it (seems) unused.
 *
 * Example:
 *	// Even if this is unused, these are vital for debugging.
 *	static UNNEEDED_ATTRIBUTE int counter;
 *	static UNNEEDED_ATTRIBUTE void dump_counter(void)
 *	{
 *		printf("Counter is %i\n", counter);
 *	}
 */
#define NEEDED_ATTRIBUTE __attribute__((used))
#else
/* Before used, unused functions and vars were always emitted. */
#define NEEDED_ATTRIBUTE __attribute__((unused))
#endif
#else
#define UNNEEDED_ATTRIBUTE
#define NEEDED_ATTRIBUTE
#endif

#if HAVE_BUILTIN_CONSTANT_P
/**
 * IS_COMPILE_CONSTANT - does the compiler know the value of this expression?
 * @expr: the expression to evaluate
 *
 * When an expression manipulation is complicated, it is usually better to
 * implement it in a function.  However, if the expression being manipulated is
 * known at compile time, it is better to have the compiler see the entire
 * expression so it can simply substitute the result.
 *
 * This can be done using the IS_COMPILE_CONSTANT() macro.
 *
 * Example:
 *	enum greek { ALPHA, BETA, GAMMA, DELTA, EPSILON };
 *
 *	// Out-of-line version.
 *	const char *greek_name(enum greek greek);
 *
 *	// Inline version.
 *	static inline char *_greek_name(enum greek greek)
 *	{
 *		switch (greek) {
 *		case ALPHA: return "alpha";
 *		case BETA: return "beta";
 *		case GAMMA: return "gamma";
 *		case DELTA: return "delta";
 *		case EPSILON: return "epsilon";
 *		default: return "**INVALID**";
 *		}
 *	}
 *
 *	// Use inline if compiler knows answer.  Otherwise call function
 *	// to avoid copies of the same code everywhere.
 *	#define greek_name(g)						\
 *		 (IS_COMPILE_CONSTANT(greek) ? _greek_name(g) : greek_name(g))
 */
#define IS_COMPILE_CONSTANT(expr) __builtin_constant_p(expr)
#else
/* If we don't know, assume it's not. */
#define IS_COMPILE_CONSTANT(expr) 0
#endif
#endif /* CCAN_COMPILER_H */
