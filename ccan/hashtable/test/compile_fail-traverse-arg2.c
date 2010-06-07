#include <ccan/hashtable/hashtable.h>
#include <ccan/hashtable/hashtable.c>
#include <stdlib.h>

struct foo {
	int i;
};

struct bar {
	int i;
};

static bool fn_foo_foo(struct foo *foo, 
#ifdef FAIL
		       struct foo *
#else
		       struct bar *
#endif
		       bar)
{
	return true;
}

int main(void)
{
	struct hashtable *ht = NULL;
	struct bar *bar = NULL;

	hashtable_traverse(ht, struct foo, fn_foo_foo, bar);
	return 0;
}
