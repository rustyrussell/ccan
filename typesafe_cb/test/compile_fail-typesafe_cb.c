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
#else
	char *p;
#endif
	p = NULL;
	register_callback(my_callback, p);
	return 0;
}
