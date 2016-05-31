#define CCAN_LIST_DEBUG 1
#include <ccan/tlist2/tlist2.h>
#include <ccan/tap/tap.h>

struct child {
	const char *name;
	struct list_node list;
};

struct parent {
	const char *name;
	TLIST2(struct child, list) children;
	unsigned int num_children;
};

int main(int argc, char *argv[])
{
	struct parent parent;
	struct child c1, c2, c3, *c, *n;
	unsigned int i;
	TLIST2(struct child, list) tlist = TLIST2_INIT(tlist);

	plan_tests(48);
	/* Test TLIST2_INIT, and tlist2_empty */
	ok1(tlist2_empty(&tlist));
	ok1(tlist2_check(&tlist, NULL));

	parent.num_children = 0;
	tlist2_init(&parent.children);
	/* Test tlist2_init */
	ok1(tlist2_empty(&parent.children));
	ok1(tlist2_check(&parent.children, NULL));

	c2.name = "c2";
	tlist2_add(&parent.children, &c2);
	/* Test tlist2_add and !tlist2_empty. */
	ok1(!tlist2_empty(&parent.children));
	ok1(c2.list.next == &tlist2_unwrap(&parent.children)->n);
	ok1(c2.list.prev == &tlist2_unwrap(&parent.children)->n);
	ok1(tlist2_unwrap(&parent.children)->n.next == &c2.list);
	ok1(tlist2_unwrap(&parent.children)->n.prev == &c2.list);
	/* Test tlist2_check */
	ok1(tlist2_check(&parent.children, NULL));

	c1.name = "c1";
	tlist2_add(&parent.children, &c1);
	/* Test list_add and !list_empty. */
	ok1(!tlist2_empty(&parent.children));
	ok1(c2.list.next == &tlist2_unwrap(&parent.children)->n);
	ok1(c2.list.prev == &c1.list);
	ok1(tlist2_unwrap(&parent.children)->n.next == &c1.list);
	ok1(tlist2_unwrap(&parent.children)->n.prev == &c2.list);
	ok1(c1.list.next == &c2.list);
	ok1(c1.list.prev == &tlist2_unwrap(&parent.children)->n);
	/* Test tlist2_check */
	ok1(tlist2_check(&parent.children, NULL));

	c3.name = "c3";
	tlist2_add_tail(&parent.children, &c3);
	/* Test list_add_tail and !list_empty. */
	ok1(!tlist2_empty(&parent.children));
	ok1(tlist2_unwrap(&parent.children)->n.next == &c1.list);
	ok1(tlist2_unwrap(&parent.children)->n.prev == &c3.list);
	ok1(c1.list.next == &c2.list);
	ok1(c1.list.prev == &tlist2_unwrap(&parent.children)->n);
	ok1(c2.list.next == &c3.list);
	ok1(c2.list.prev == &c1.list);
	ok1(c3.list.next == &tlist2_unwrap(&parent.children)->n);
	ok1(c3.list.prev == &c2.list);
	/* Test tlist2_check */
	ok1(tlist2_check(&parent.children, NULL));

	/* Test tlist2_top */
	ok1(tlist2_top(&parent.children) == &c1);

	/* Test list_tail */
	ok1(tlist2_tail(&parent.children) == &c3);

	/* Test tlist2_for_each. */
	i = 0;
	tlist2_for_each(&parent.children, c) {
		switch (i++) {
		case 0:
			ok1(c == &c1);
			break;
		case 1:
			ok1(c == &c2);
			break;
		case 2:
			ok1(c == &c3);
			break;
		}
		if (i > 2)
			break;
	}
	ok1(i == 3);

	/* Test tlist2_for_each_rev. */
	i = 0;
	tlist2_for_each_rev(&parent.children, c) {
		switch (i++) {
		case 0:
			ok1(c == &c3);
			break;
		case 1:
			ok1(c == &c2);
			break;
		case 2:
			ok1(c == &c1);
			break;
		}
		if (i > 2)
			break;
	}
	ok1(i == 3);

	/* Test tlist2_for_each_safe, tlist2_del and tlist2_del_from. */
	i = 0;
	tlist2_for_each_safe(&parent.children, c, n) {
		switch (i++) {
		case 0:
			ok1(c == &c1);
			tlist2_del_from(&parent.children, c);
			break;
		case 1:
			ok1(c == &c2);
			tlist2_del_from(&parent.children, c);
			break;
		case 2:
			ok1(c == &c3);
			tlist2_del_from(&parent.children, c);
			break;
		}
		ok1(tlist2_check(&parent.children, NULL));
		if (i > 2)
			break;
	}
	ok1(i == 3);
	ok1(tlist2_empty(&parent.children));

	/* Test list_top/list_tail on empty list. */
	ok1(tlist2_top(&parent.children) == (struct child *)NULL);
	ok1(tlist2_tail(&parent.children) == (struct child *)NULL);
	return exit_status();
}
