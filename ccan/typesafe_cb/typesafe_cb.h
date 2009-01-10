#ifndef CCAN_CAST_IF_TYPE_H
#define CCAN_CAST_IF_TYPE_H
#include "config.h"

#if HAVE_TYPEOF && HAVE_BUILTIN_CHOOSE_EXPR && HAVE_BUILTIN_TYPES_COMPATIBLE_P
/**
 * cast_if_type - only cast an expression if it is of a given type
 * @expr: the expression to cast
 * @oktype: the type we allow
 * @desttype: the type to cast to
 *
 * This macro is used to create functions which allow multiple types.
 * The result of this macro is used somewhere that a @desttype type is
 * expected: if @expr was of type @oktype, it will be cast to
 * @desttype type.  As a result, if @expr is any type other than
 * @oktype or @desttype, a compiler warning will be issued.
 *
 * This macro can be used in static initializers.
 *
 * This is merely useful for warnings: if the compiler does not
 * support the primitives required for cast_if_type(), it becomes an
 * unconditional cast, and the @oktype argument is not used.  In
 * particular, this means that @oktype can be a type which uses
 * the "typeof": it will not be evaluated if typeof is not supported.
 *
 * Example:
 *	// We can take either an unsigned long or a void *.
 *	void _set_some_value(void *val);
 *	#define set_some_value(expr)			\
 *		_set_some_value(cast_if_type((expr), unsigned long, void *))
 */
#define cast_if_type(expr, oktype, desttype)				\
__builtin_choose_expr(__builtin_types_compatible_p(typeof(1?(expr):0), oktype), \
			(desttype)(expr), (expr))
#else
#define cast_if_type(expr, oktype, desttype) ((desttype)(expr))
#endif

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
 * Example:
 *	void _register_callback(void (*fn)(void *arg), void *arg);
 *	#define register_callback(fn, arg) \
 *		_register_callback(typesafe_cb(void, (fn), (arg)), (arg))
 */
#define typesafe_cb(rtype, fn, arg)					\
	cast_if_type(cast_if_type(cast_if_type((fn),			\
					       rtype (*)(const typeof(*arg)*), \
					       rtype (*)(void *)),	\
				  rtype (*)(volatile typeof(*arg) *),	\
				  rtype (*)(void *)),			\
		     rtype (*)(typeof(arg)),				\
		     rtype (*)(void *))

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
	cast_if_type(cast_if_type(cast_if_type((fn),			\
					       rtype (*)(__VA_ARGS__,	\
							 const typeof(*arg) *),\
					       rtype (*)(__VA_ARGS__,	\
							 void *)),	\
				  rtype (*)(__VA_ARGS__,		\
					    volatile typeof(*arg) *),	\
				  rtype (*)(__VA_ARGS__, void *)),	\
		     rtype (*)(__VA_ARGS__, typeof(arg)),		\
		     rtype (*)(__VA_ARGS__, void *))

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
 *		_register_callback(typesafe_cb_preargs(void, (fn), (arg), int),\
 *				   (arg))
 */
#define typesafe_cb_postargs(rtype, fn, arg, ...)			\
	cast_if_type(cast_if_type(cast_if_type((fn),			\
					       rtype (*)(const typeof(*arg) *, \
							 __VA_ARGS__),	\
					       rtype (*)(void *,	\
							 __VA_ARGS__)), \
				  rtype (*)(volatile typeof(*arg) *,	\
					    __VA_ARGS__),		\
				  rtype (*)(void *, __VA_ARGS__)),	\
		     rtype (*)(typeof(arg), __VA_ARGS__),		\
		     rtype (*)(void *, __VA_ARGS__))

#endif /* CCAN_CAST_IF_TYPE_H */
