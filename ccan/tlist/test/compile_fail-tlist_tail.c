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
	struct cousin *c;
#else
	struct child *c;
#endif

	tlist_init(&children);
	tlist_add(&children, &child, list);

	c = tlist_tail(&children, list);
	(void) c; /* Suppress unused-but-set-variable warning. */
	return 0;
}
