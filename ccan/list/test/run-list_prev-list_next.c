#include <ccan/list/list.h>
#include <ccan/tap/tap.h>
#include <ccan/list/list.c>
#include "helper.h"

struct parent {
	const char *name;
	unsigned int num_children;
	struct list_head children;
};

struct child {
	const char *name;
	struct list_node list;
};

int main(int argc, char *argv[])
{
	struct parent parent;
	struct child c1, c2, c3;

	plan_tests(12);
	parent.num_children = 0;
	list_head_init(&parent.children);

	c1.name = "c1";
	list_add(&parent.children, &c1.list);

	ok1(list_next(&parent.children, &c1, list) == NULL);
	ok1(list_prev(&parent.children, &c1, list) == NULL);

	c2.name = "c2";
	list_add_tail(&parent.children, &c2.list);

	ok1(list_next(&parent.children, &c1, list) == &c2);
	ok1(list_prev(&parent.children, &c1, list) == NULL);
	ok1(list_next(&parent.children, &c2, list) == NULL);
	ok1(list_prev(&parent.children, &c2, list) == &c1);

	c3.name = "c3";
	list_add_tail(&parent.children, &c3.list);

	ok1(list_next(&parent.children, &c1, list) == &c2);
	ok1(list_prev(&parent.children, &c1, list) == NULL);
	ok1(list_next(&parent.children, &c2, list) == &c3);
	ok1(list_prev(&parent.children, &c2, list) == &c1);
	ok1(list_next(&parent.children, &c3, list) == NULL);
	ok1(list_prev(&parent.children, &c3, list) == &c2);
	return exit_status();
}
