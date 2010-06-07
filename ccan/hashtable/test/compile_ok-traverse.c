#include <ccan/hashtable/hashtable.h>
#include <ccan/hashtable/hashtable.c>

struct foo {
	int i;
};

struct bar {
	int i;
};

static bool fn_foo_bar(struct foo *foo, struct bar *bar)
{
	return true;
}

static bool fn_const_foo_bar(const struct foo *foo, struct bar *bar)
{
	return true;
}

static bool fn_foo_const_bar(struct foo *foo, const struct bar *bar)
{
	return true;
}

static bool fn_const_foo_const_bar(const struct foo *foo,
				   const struct bar *bar)
{
	return true;
}

static bool fn_void_void(void *foo, void *bar)
{
	return true;
}

int main(void)
{
	struct hashtable *ht = NULL;
	struct bar *bar = NULL;

	hashtable_traverse(ht, struct foo, fn_foo_bar, bar);
	hashtable_traverse(ht, struct foo, fn_const_foo_bar, bar);
	hashtable_traverse(ht, struct foo, fn_foo_const_bar, bar);
	hashtable_traverse(ht, struct foo, fn_const_foo_const_bar, bar);
	hashtable_traverse(ht, struct foo, fn_void_void, bar);
	return 0;
}
