#define CCAN_LIST_DEBUG 1
#include <ccan/tlist/tlist.h>
#include <ccan/tap/tap.h>

TLIST_TYPE(children, struct child);

struct parent {
	const char *name;
	unsigned int num_children;
	struct tlist_children children;
};

struct child {
	const char *name;
	struct list_node list;
};

int main(int argc, char *argv[])
{
	struct parent parent;
	struct child c1, c2, c3, *c, *n;
	unsigned int i;
	struct tlist_children tlist = TLIST_INIT(tlist);

	plan_tests(60);
	/* Test TLIST_INIT, and tlist_empty */
	ok1(tlist_empty(&tlist));
	ok1(tlist_check(&tlist, NULL));

	parent.num_children = 0;
	tlist_init(&parent.children);
	/* Test tlist_init */
	ok1(tlist_empty(&parent.children));
	ok1(tlist_check(&parent.children, NULL));

	c2.name = "c2";
	tlist_add(&parent.children, &c2, list);
	/* Test tlist_add and !tlist_empty. */
	ok1(!tlist_empty(&parent.children));
	ok1(c2.list.next == &tcon_unwrap(&parent.children)->n);
	ok1(c2.list.prev == &tcon_unwrap(&parent.children)->n);
	ok1(tcon_unwrap(&parent.children)->n.next == &c2.list);
	ok1(tcon_unwrap(&parent.children)->n.prev == &c2.list);
	ok1(tlist_next(&parent.children, &c2, list) == NULL);
	ok1(tlist_prev(&parent.children, &c2, list) == NULL);
	/* Test tlist_check */
	ok1(tlist_check(&parent.children, NULL));

	c1.name = "c1";
	tlist_add(&parent.children, &c1, list);
	/* Test list_add and !list_empty. */
	ok1(!tlist_empty(&parent.children));
	ok1(c2.list.next == &tcon_unwrap(&parent.children)->n);
	ok1(c2.list.prev == &c1.list);
	ok1(tcon_unwrap(&parent.children)->n.next == &c1.list);
	ok1(tcon_unwrap(&parent.children)->n.prev == &c2.list);
	ok1(c1.list.next == &c2.list);
	ok1(c1.list.prev == &tcon_unwrap(&parent.children)->n);
	ok1(tlist_next(&parent.children, &c1, list) == &c2);
	ok1(tlist_next(&parent.children, &c2, list) == NULL);
	ok1(tlist_prev(&parent.children, &c2, list) == &c1);
	ok1(tlist_prev(&parent.children, &c1, list) == NULL);
	/* Test tlist_check */
	ok1(tlist_check(&parent.children, NULL));

	c3.name = "c3";
	tlist_add_tail(&parent.children, &c3, list);
	/* Test list_add_tail and !list_empty. */
	ok1(!tlist_empty(&parent.children));
	ok1(tcon_unwrap(&parent.children)->n.next == &c1.list);
	ok1(tcon_unwrap(&parent.children)->n.prev == &c3.list);
	ok1(c1.list.next == &c2.list);
	ok1(c1.list.prev == &tcon_unwrap(&parent.children)->n);
	ok1(c2.list.next == &c3.list);
	ok1(c2.list.prev == &c1.list);
	ok1(c3.list.next == &tcon_unwrap(&parent.children)->n);
	ok1(c3.list.prev == &c2.list);
	ok1(tlist_next(&parent.children, &c1, list) == &c2);
	ok1(tlist_next(&parent.children, &c2, list) == &c3);
	ok1(tlist_next(&parent.children, &c3, list) == NULL);
	ok1(tlist_prev(&parent.children, &c3, list) == &c2);
	ok1(tlist_prev(&parent.children, &c2, list) == &c1);
	ok1(tlist_prev(&parent.children, &c1, list) == NULL);
	/* Test tlist_check */
	ok1(tlist_check(&parent.children, NULL));

	/* Test tlist_top */
	ok1(tlist_top(&parent.children, list) == &c1);

	/* Test list_tail */
	ok1(tlist_tail(&parent.children, list) == &c3);

	/* Test tlist_for_each. */
	i = 0;
	tlist_for_each(&parent.children, c, list) {
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

	/* Test tlist_for_each_rev. */
	i = 0;
	tlist_for_each_rev(&parent.children, c, list) {
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

	/* Test tlist_for_each_safe, tlist_del and tlist_del_from. */
	i = 0;
	tlist_for_each_safe(&parent.children, c, n, list) {
		switch (i++) {
		case 0:
			ok1(c == &c1);	
			tlist_del(c, list);
			break;
		case 1:
			ok1(c == &c2);
			tlist_del_from(&parent.children, c, list);
			break;
		case 2:
			ok1(c == &c3);
			tlist_del_from(&parent.children, c, list);
			break;
		}
		ok1(tlist_check(&parent.children, NULL));
		if (i > 2)
			break;
	}
	ok1(i == 3);
	ok1(tlist_empty(&parent.children));

	/* Test list_top/list_tail on empty list. */
	ok1(tlist_top(&parent.children, list) == (struct child *)NULL);
	ok1(tlist_tail(&parent.children, list) == (struct child *)NULL);
	return exit_status();
}
