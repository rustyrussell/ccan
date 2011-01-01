#include <ccan/tlist/tlist.h>

TLIST_TYPE(children, struct child);

struct child {
	const char *name;
	struct list_node list;
};

struct cousin {
	const char *name;
	struct list_node list;
};

int main(int argc, char *argv[])
{
	struct tlist_children children;
	struct child child = { "child" };
	void *c;

	tlist_init(&children);
	tlist_add(&children, &child, list);

	c = tlist_top(&children,
#ifdef FAIL
#if !HAVE_FLEXIBLE_ARRAY_MEMBER
#error Need flexible array members to check type
#endif
		      struct cousin,
#else
		      struct child,
#endif
		      list);
	return 0;
}
