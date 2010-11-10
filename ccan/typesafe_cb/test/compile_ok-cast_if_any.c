#include <ccan/typesafe_cb/typesafe_cb.h>
#include <stdlib.h>

struct foo {
	int x;
};

struct bar {
	int x;
};

struct baz {
	int x;
};

struct any {
	int x;
};

static void take_any(struct any *any)
{
}

int main(int argc, char *argv[])
{
#if HAVE_TYPEOF
	/* Otherwise we get unused warnings for these. */
	struct foo *foo = NULL;
	struct bar *bar = NULL;
	struct baz *baz = NULL;
#endif
	struct other *arg = NULL;

	take_any(cast_if_any(struct any *, arg, foo,
			     struct foo *, struct bar *, struct baz *));
	take_any(cast_if_any(struct any *, arg, bar,
			     struct foo *, struct bar *, struct baz *));
	take_any(cast_if_any(struct any *, arg, baz,
			     struct foo *, struct bar *, struct baz *));
	return 0;
}
