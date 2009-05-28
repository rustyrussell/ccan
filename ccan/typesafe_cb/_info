#include <stdio.h>
#include <string.h>
#include "config.h"

/**
 * typesafe_cb - macros for safe callbacks.
 *
 * The basis of the typesafe_cb header is cast_if_type(): a
 * conditional cast macro.   If an expression exactly matches a given
 * type, it is cast to the target type, otherwise it is left alone.
 *
 * This allows us to create functions which take a small number of
 * specific types, rather than being forced to use a void *.  In
 * particular, it is useful for creating typesafe callbacks as the
 * helpers typesafe_cb(), typesafe_cb_preargs() and
 * typesafe_cb_postargs() demonstrate.
 * 
 * The standard way of passing arguments to callback functions in C is
 * to use a void pointer, which the callback then casts back to the
 * expected type.  This unfortunately subverts the type checking the
 * compiler would perform if it were a direct call.  Here's an example:
 *
 *	static void my_callback(void *_obj)
 *	{
 *		struct obj *obj = _obj;
 *		...
 *	}
 *	...
 *		register_callback(my_callback, &my_obj);
 *
 * If we wanted to use the natural type for my_callback (ie. "void
 * my_callback(struct obj *obj)"), we could make register_callback()
 * take a void * as its first argument, but this would subvert all
 * type checking.  We really want register_callback() to accept only
 * the exactly correct function type to match the argument, or a
 * function which takes a void *.
 *
 * This is where typesafe_cb() comes in: it uses cast_if_type() to
 * cast the callback function if it matches the argument type:
 *
 *	void _register_callback(void (*cb)(void *arg), void *arg);
 *	#define register_callback(cb, arg)				\
 *		_register_callback(typesafe_cb(void, (cb), (arg)), (arg))
 *
 * On compilers which don't support the extensions required
 * cast_if_type() and friend become an unconditional cast, so your
 * code will compile but you won't get type checking.
 *
 * Licence: LGPL (2 or any later version)
 */
int main(int argc, char *argv[])
{
	if (argc != 2)
		return 1;

	if (strcmp(argv[1], "depends") == 0) {
		return 0;
	}

	return 1;
}
