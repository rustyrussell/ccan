#define CCAN_DGRAPH_DEBUG 1
#include <ccan/dgraph/dgraph.h>
/* Include the C files directly. */
#include <ccan/dgraph/dgraph.c>
#include <ccan/tap/tap.h>

static bool count_nodes(struct dgraph_node *n, unsigned int *count)
{
	(*count)++;
	return true;
}

static bool stop_traverse(struct dgraph_node *n, unsigned int *count)
{
	if (--(*count) == 0)
		return false;
	return true;
}

int main(void)
{
	struct dgraph_node n1, n2, n3;
	unsigned int count = 0;

	/* This is how many tests you plan to run */
	plan_tests(42);

	dgraph_init_node(&n1);
	ok1(dgraph_check(&n1, NULL) == &n1);
	count = 0;
	dgraph_traverse_from(&n1, count_nodes, &count);
	ok1(count == 0);
	count = 0;
	dgraph_traverse_to(&n1, count_nodes, &count);
	ok1(count == 0);

	dgraph_init_node(&n2);
	ok1(dgraph_check(&n2, NULL) == &n2);
	dgraph_add_edge(&n1, &n2);
	ok1(dgraph_check(&n1, NULL) == &n1);
	ok1(dgraph_check(&n2, NULL) == &n2);
	count = 0;
	dgraph_traverse_from(&n1, count_nodes, &count);
	ok1(count == 1);
	count = 0;
	dgraph_traverse_to(&n1, count_nodes, &count);
	ok1(count == 0);
	count = 0;
	dgraph_traverse_from(&n2, count_nodes, &count);
	ok1(count == 0);
	count = 0;
	dgraph_traverse_to(&n2, count_nodes, &count);
	ok1(count == 1);

	dgraph_init_node(&n3);
	ok1(dgraph_check(&n3, NULL) == &n3);
	dgraph_add_edge(&n2, &n3);
	ok1(dgraph_check(&n1, NULL) == &n1);
	ok1(dgraph_check(&n2, NULL) == &n2);
	ok1(dgraph_check(&n3, NULL) == &n3);
	count = 0;
	dgraph_traverse_from(&n1, count_nodes, &count);
	ok1(count == 2);
	count = 0;
	dgraph_traverse_to(&n1, count_nodes, &count);
	ok1(count == 0);
	count = 0;
	dgraph_traverse_from(&n2, count_nodes, &count);
	ok1(count == 1);
	count = 0;
	dgraph_traverse_to(&n2, count_nodes, &count);
	ok1(count == 1);
	count = 0;
	dgraph_traverse_from(&n3, count_nodes, &count);
	ok1(count == 0);
	count = 0;
	dgraph_traverse_to(&n3, count_nodes, &count);
	ok1(count == 2);

	/* Check stopping traverse. */
	count = 1;
	dgraph_traverse_from(&n1, stop_traverse, &count);
	ok1(count == 0);
	count = 2;
	dgraph_traverse_from(&n1, stop_traverse, &count);
	ok1(count == 0);
	count = 3;
	dgraph_traverse_from(&n1, stop_traverse, &count);
	ok1(count == 1);

	dgraph_clear_node(&n1);
	ok1(dgraph_check(&n1, NULL) == &n1);
	ok1(dgraph_check(&n2, NULL) == &n2);
	ok1(dgraph_check(&n3, NULL) == &n3);

	count = 0;
	dgraph_traverse_from(&n2, count_nodes, &count);
	ok1(count == 1);
	count = 0;
	dgraph_traverse_to(&n2, count_nodes, &count);
	ok1(count == 0);
	count = 0;
	dgraph_traverse_from(&n3, count_nodes, &count);
	ok1(count == 0);
	count = 0;
	dgraph_traverse_to(&n3, count_nodes, &count);
	ok1(count == 1);

	ok1(dgraph_del_edge(&n2, &n3));
	count = 0;
	dgraph_traverse_from(&n2, count_nodes, &count);
	ok1(count == 0);
	count = 0;
	dgraph_traverse_to(&n2, count_nodes, &count);
	ok1(count == 0);
	count = 0;
	dgraph_traverse_from(&n3, count_nodes, &count);
	ok1(count == 0);
	count = 0;
	dgraph_traverse_to(&n3, count_nodes, &count);
	ok1(count == 0);
	ok1(dgraph_check(&n1, NULL) == &n1);
	ok1(dgraph_check(&n2, NULL) == &n2);
	ok1(dgraph_check(&n3, NULL) == &n3);

	ok1(!dgraph_del_edge(&n2, &n3));
	dgraph_clear_node(&n2);

	ok1(dgraph_check(&n1, NULL) == &n1);
	ok1(dgraph_check(&n2, NULL) == &n2);
	ok1(dgraph_check(&n3, NULL) == &n3);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
