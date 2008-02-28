#include "typesafe_cb/typesafe_cb.h"

void _set_some_value(void *val);

void _set_some_value(void *val)
{
}

#define set_some_value(expr)						\
	_set_some_value(cast_if_type((expr), unsigned long, void *))

int main(int argc, char *argv[])
{
#ifdef FAIL
	int x = 0;
	set_some_value(x);
#else
	void *p = 0;
	set_some_value(p);
#endif
	return 0;
}
