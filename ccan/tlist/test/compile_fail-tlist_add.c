#include <ccan/tlist/tlist.h>

TLIST_TYPE(children, struct child);
TLIST_TYPE(cousins, struct cousin);

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
	struct tlist_cousins cousins;
	struct child child = { "child" };
	struct cousin cousin = { "cousin" };

	tlist_init(&children);
	tlist_init(&cousins);
	tlist_add(&children, &child, list);
	tlist_add(&cousins, &cousin, list);
	tlist_del_from(&cousins, &cousin, list);
#ifdef FAIL
#if !HAVE_FLEXIBLE_ARRAY_MEMBER
#error Need flexible array members to check type
#endif
	tlist_add(&children, &cousin, list);
#endif
	return 0;
}
