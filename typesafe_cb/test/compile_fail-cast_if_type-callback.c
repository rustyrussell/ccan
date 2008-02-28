#include "typesafe_cb/typesafe_cb.h"
#include <stdlib.h>

static void _callback(void (*fn)(void *arg), void *arg)
{
	fn(arg);
}

#define callback(fn, arg)						\
	_callback(cast_if_type((fn), void (*)(typeof(arg)), void (*)(void *)), \
		  arg)

static void my_callback(char *p)
{
}

int main(int argc, char *argv[])
{
	callback(my_callback, "hello world");

#ifdef FAIL
	/* Must be a char * */
	callback(my_callback, my_callback);
#endif
	return 0;
}
