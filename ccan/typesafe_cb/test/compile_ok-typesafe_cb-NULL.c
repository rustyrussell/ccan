#include <ccan/typesafe_cb/typesafe_cb.h>
#include <stdlib.h>

/* NULL args for callback function should be OK for _exact and _def. */

static void _register_callback(void (*cb)(const void *arg), const void *arg)
{
}

#define register_callback_def(cb, arg)				\
	_register_callback(typesafe_cb_def(void, (cb), (arg)), (arg))

#define register_callback_exact(cb, arg)				\
	_register_callback(typesafe_cb_exact(void, (cb), (arg)), (arg))

int main(int argc, char *argv[])
{
	register_callback_def(NULL, "hello world");
	register_callback_exact(NULL, "hello world");
	return 0;
}
