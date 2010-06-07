#include <ccan/hashtable/hashtable.h>
#include <ccan/hashtable/hashtable.c>
#include <stdlib.h>

struct foo {
	int i;
};

struct bar {
	int i;
};

static bool fn_bar_bar(
#ifdef FAIL
	struct bar *
#else
	struct foo *
#endif
		       foo,
		       struct bar *bar)
{
	return true;
}

int main(void)
{
	struct hashtable *ht = NULL;
	struct bar *bar = NULL;

	hashtable_traverse(ht, struct foo, fn_bar_bar, bar);
	return 0;
}
