#include <ccan/idtree/idtree.c>
#include <ccan/tap/tap.h>
#include <limits.h>

int main(int argc, char *argv[])
{
	unsigned int i;
	struct idtree *idtree;

	plan_tests(6);
	idtree = idtree_new(NULL);

	ok1(idtree_add_above(idtree, &i, INT_MAX-1, INT_MAX) == INT_MAX-1);
	ok1(idtree_add_above(idtree, &i, INT_MAX-1, INT_MAX) == INT_MAX);
	ok1(idtree_add_above(idtree, &i, INT_MAX-1, INT_MAX) == -1);

	ok1(idtree_remove(idtree, INT_MAX-1) == true);
	ok1(idtree_add_above(idtree, &i, INT_MAX-1, INT_MAX) == INT_MAX-1);
	ok1(idtree_add_above(idtree, &i, INT_MAX-1, INT_MAX) == -1);
	tal_free(idtree);
	exit(exit_status());
}
