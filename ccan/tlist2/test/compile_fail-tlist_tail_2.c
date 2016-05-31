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
	struct child child = { "child" };
#ifdef FAIL
	struct cousin *c;
#else
	struct child *c;
#endif

	tlist2_init(&children);
	tlist2_add(&children, &child);

	c = tlist2_tail(&children);
	(void) c; /* Suppress unused-but-set-variable warning. */
	return 0;
}
