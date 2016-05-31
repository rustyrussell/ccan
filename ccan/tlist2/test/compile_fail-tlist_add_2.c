#include <ccan/tlist2/tlist2.h>

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
	TLIST2(struct child, list) children;
	TLIST2(struct cousin, list) cousins;
	struct child child = { "child" };
	struct cousin cousin = { "cousin" };

	tlist2_init(&children);
	tlist2_init(&cousins);
	tlist2_add(&children, &child);
	tlist2_add(&cousins, &cousin);
	tlist2_del_from(&cousins, &cousin);
#ifdef FAIL
#if !HAVE_FLEXIBLE_ARRAY_MEMBER
#error Need flexible array members to check type
#endif
	tlist2_add(&children, &cousin);
#endif
	return 0;
}
