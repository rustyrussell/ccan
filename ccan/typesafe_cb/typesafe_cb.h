#ifndef CCAN_CAST_IF_TYPE_H
#define CCAN_CAST_IF_TYPE_H
#include "config.h"

#if HAVE_TYPEOF && HAVE_BUILTIN_CHOOSE_EXPR && HAVE_BUILTIN_TYPES_COMPATIBLE_P
/**
 * cast_if_type - only cast an expression if test matches a given type
 * @desttype: the type to cast to
 * @expr: the expression to cast
 * @test: the expression to test
 * @oktype: the type we allow
 *
 * This macro is used to create functions which allow multiple types.
 * The result of this macro is used somewhere that a @desttype type is
 * expected: if @test is exactly of type @oktype, then @expr will be
 * cast to @desttype type, otherwise left alone.
 *
 * This macro can be used in static initializers.
 *
 * This is merely useful for warnings: if the compiler does not
 * support the primitives required for cast_if_type(), it becomes an
 * unconditional cast, and the @test and @oktype argument is not used.  In
 * particular, this means that @oktype can be a type which uses
 * the "typeof": it will not be evaluated if typeof is not supported.
 *
 * Example:
 *	// We can take either an unsigned long or a void *.
 *	void _set_some_value(void *val);
 *	#define set_some_value(e)			\
 *		_set_some_value(cast_if_type(void *, (e), (e), unsigned long))
 */
#define cast_if_type(desttype, expr, test, oktype)			\
	__builtin_choose_expr(__builtin_types_compatible_p(typeof(test), oktype), \
			(desttype)(expr), (expr))
#else
#define cast_if_type(desttype, expr, test, oktype) ((desttype)(expr))
#endif

/**
 * cast_if_any - only cast an expression if it is one of the three given types
 * @desttype: the type to cast to
 * @expr: the expression to cast
 * @test: the expression to test
 * @ok1: the first type we allow
 * @ok2: the second type we allow
 * @ok3: the third type we allow
 *
 * This is a convenient wrapper for multiple cast_if_type() calls.  You can
 * chain them inside each other (ie. use cast_if_any() for expr) if you need
 * more than 3 arguments.
 *
 * Example:
 *	// We can take either a long, unsigned long, void * or a const void *.
 *	void _set_some_value(void *val);
 *	#define set_some_value(expr)					\
 *		_set_some_value(cast_if_any(void *, (expr), (expr),	\
 *					    long, unsigned long, const void *))
 */
#define cast_if_any(desttype, expr, test, ok1, ok2, ok3)		\
	cast_if_type(desttype,						\
		     cast_if_type(desttype,				\
				  cast_if_type(desttype, (expr), (test), ok1), \
				  (test),				\
				  ok2),					\
		     (test),						\
		     ok3)

/**
 * typesafe_cb - cast a callback function if it matches the arg
 * @rtype: the return type of the callback function
 * @fn: the callback function to cast
 * @arg: the (pointer) argument to hand to the callback function.
 *
 * If a callback function takes a single argument, this macro does
 * appropriate casts to a function which takes a single void * argument if the
 * callback provided matches the @arg (or a const or volatile version).
 *
 * It is assumed that @arg is of pointer type: usually @arg is passed
 * or assigned to a void * elsewhere anyway.
 *
 * This will not work with a NULL @fn argument: see typesafe_cb_def or
 * typesafe_cb_exact.
 *
 * Example:
 *	void _register_callback(void (*fn)(void *arg), void *arg);
 *	#define register_callback(fn, arg) \
 *		_register_callback(typesafe_cb(void, (fn), (arg)), (arg))
 */
#define typesafe_cb(rtype, fn, arg)			\
	cast_if_type(rtype (*)(void *), (fn), (fn)(arg), rtype)

/**
 * typesafe_cb_def - cast a callback fn if it matches arg (of defined type)
 * @rtype: the return type of the callback function
 * @fn: the callback function to cast
 * @arg: the (pointer) argument to hand to the callback function.
 *
 * This is typesafe_cb(), except the type must be defined (eg. if it's
 * struct foo *, the definition of struct foo must be visible).  For many
 * applications, this is reasonable.
 *
 * This variant can accept @fn equal to NULL.
 *
 * Example:
 *	void _register_callback(void (*fn)(void *arg), void *arg);
 *	#define register_callback(fn, arg) \
 *		_register_callback(typesafe_cb_def(void, (fn), (arg)), (arg))
 */
#define typesafe_cb_def(rtype, fn, arg)			\
	cast_if_any(rtype (*)(void *), (fn), (fn)+0,	\
		    rtype (*)(typeof(*arg)*),		\
		    rtype (*)(const typeof(*arg)*),	\
		    rtype (*)(volatile typeof(*arg)*))

/**
 * typesafe_cb_exact - cast a callback fn if it exactly matches arg
 * @rtype: the return type of the callback function
 * @fn: the callback function to cast
 * @arg: the (pointer) argument to hand to the callback function.
 *
 * This is typesafe_cb(), except the @fn can be NULL, or must exactly match
 * the @arg type (no const or volatile).
 *
 * Example:
 *	void _register_callback(void (*fn)(void *arg), void *arg);
 *	#define register_callback(fn, arg) \
 *		_register_callback(typesafe_cb_exact(void, (fn), (arg)), (arg))
 */
#define typesafe_cb_exact(rtype, fn, arg)				\
	cast_if_type(rtype (*)(void *), (fn), (fn)+0, rtype (*)(typeof(arg)))

/**
 * typesafe_cb_const - cast a const callback function if it matches the arg
 * @rtype: the return type of the callback function
 * @fn: the callback function to cast
 * @arg: the (pointer) argument to hand to the callback function.
 *
 * If a callback function takes a single argument, this macro does appropriate
 * casts to a function which takes a single const void * argument if the
 * callback provided matches the @arg.
 *
 * It is assumed that @arg is of pointer type: usually @arg is passed
 * or assigned to a void * elsewhere anyway.
 *
 * Example:
 *	void _register_callback(void (*fn)(const void *arg), const void *arg);
 *	#define register_callback(fn, arg) \
 *		_register_callback(typesafe_cb_const(void, (fn), (arg)), (arg))
 */
#define typesafe_cb_const(rtype, fn, arg)			\
	cast_if_type(rtype (*)(const void *), (fn), (fn)(arg), rtype)

/**
 * typesafe_cb_preargs - cast a callback function if it matches the arg
 * @rtype: the return type of the callback function
 * @fn: the callback function to cast
 * @arg: the (pointer) argument to hand to the callback function.
 *
 * This is a version of typesafe_cb() for callbacks that take other arguments
 * before the @arg.
 *
 * Example:
 *	void _register_callback(void (*fn)(int, void *arg), void *arg);
 *	#define register_callback(fn, arg) \
 *		_register_callback(typesafe_cb_preargs(void, (fn), (arg), int),\
 *				   (arg))
 */
#define typesafe_cb_preargs(rtype, fn, arg, ...)			\
	cast_if_type(rtype (*)(__VA_ARGS__, void *), (fn), (fn)+0,	\
		     rtype (*)(__VA_ARGS__, typeof(arg)))

/**
 * typesafe_cb_postargs - cast a callback function if it matches the arg
 * @rtype: the return type of the callback function
 * @fn: the callback function to cast
 * @arg: the (pointer) argument to hand to the callback function.
 *
 * This is a version of typesafe_cb() for callbacks that take other arguments
 * after the @arg.
 *
 * Example:
 *	void _register_callback(void (*fn)(void *arg, int), void *arg);
 *	#define register_callback(fn, arg) \
 *		_register_callback(typesafe_cb_postargs(void, (fn), (arg), int),\
 *				   (arg))
 */
#define typesafe_cb_postargs(rtype, fn, arg, ...)			\
	cast_if_type(rtype (*)(void *, __VA_ARGS__), (fn), (fn)+0,	\
		     rtype (*)(typeof(arg), __VA_ARGS__))

/**
 * typesafe_cb_cmp - cast a compare function if it matches the arg
 * @rtype: the return type of the callback function
 * @fn: the callback function to cast
 * @arg: the (pointer) argument(s) to hand to the compare function.
 *
 * If a callback function takes two matching-type arguments, this macro does
 * appropriate casts to a function which takes two const void * arguments if
 * the callback provided takes two a const pointers to @arg.
 *
 * It is assumed that @arg is of pointer type: usually @arg is passed
 * or assigned to a void * elsewhere anyway.  Note also that the type
 * arg points to must be defined.
 *
 * Example:
 *	void _my_qsort(void *base, size_t nmemb, size_t size,
 *		       int (*cmp)(const void *, const void *));
 *	#define my_qsort(base, nmemb, cmpfn) \
 *		_my_qsort((base), (nmemb), sizeof(*(base)), \
 *			  typesafe_cb_cmp(int, (cmpfn), (base)), (arg))
 */
#define typesafe_cb_cmp(rtype, cmpfn, arg)				\
	cast_if_type(rtype (*)(const void *, const void *), (cmpfn), (cmpfn)+0, \
		     rtype (*)(const typeof(*arg)*, const typeof(*arg)*))
		     
#endif /* CCAN_CAST_IF_TYPE_H */
