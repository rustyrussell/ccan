#include "typesafe_cb/typesafe_cb.h"
#include <stdlib.h>

static void _register_callback(void (*cb)(void *arg), void *arg)
{
}

#define register_callback(cb, arg)				\
	_register_callback(typesafe_cb(void, (cb), (arg)), (arg))

static void my_callback(char *p)
{
}

int main(int argc, char *argv[])
{
#ifdef FAIL
	int *p;
#if !HAVE_TYPEOF||!HAVE_BUILTIN_CHOOSE_EXPR||!HAVE_BUILTIN_TYPES_COMPATIBLE_P
#error "Unfortunately we don't fail if cast_if_type is a noop."
#endif
#else
	char *p;
#endif
	p = NULL;

	/* This should work always. */
	register_callback(my_callback, "hello world");

	/* This will fail with FAIL defined */
	register_callback(my_callback, p);
	return 0;
}
