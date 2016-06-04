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
#ifdef FAIL
#if !HAVE_FLEXIBLE_ARRAY_MEMBER
#error Need flexible array members to check type
#endif
	struct cousin *p = NULL;
#else
	struct child *p = NULL;
#endif

	tlist_init(&children);
	tlist_add(&children, &child, list);
	(void)tlist_prev(&children, p, list);
	return 0;
}
