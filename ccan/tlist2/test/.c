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
	struct tlist2_children children;
	struct tlist2_cousins cousins;
	struct child child = { "child" };
	struct cousin cousin = { "cousin" };

	tlist2_init(&children);
	tlist2_init(&cousins);
	tlist2_add(&children, &child, list);
	tlist2_add(&cousins, &cousin, list);
	tlist2_del_from(&cousins, &cousin, list);
#ifdef FAIL
#if !HAVE_FLEXIBLE_ARRAY_MEMBER
#error Need flexible array members to check type
#endif
	tlist2_add(&children, &cousin, list);
#endif
	return 0;
}
