#include <ccan/idtree/idtree.c>
#include <ccan/tap/tap.h>
#include <limits.h>

#define ALLOC_MAX (2 * IDTREE_SIZE)

int main(int argc, char *argv[])
{
	unsigned int i;
	const char allocated[ALLOC_MAX] = { 0 };
	struct idtree *idtree;
	void *ctx;

	plan_tests(ALLOC_MAX * 5 + 2);
	ctx = talloc_named_const(NULL, 1, "test root");
	idtree = idtree_new(ctx);
	ok1(talloc_find_parent_byname(idtree, "test root") == ctx);

	for (i = 0; i < ALLOC_MAX; i++) {
		int id = idtree_add(idtree, &allocated[i], ALLOC_MAX-1);
		ok1(id == i);
		ok1(idtree_lookup(idtree, i) == &allocated[i]);
	}
	ok1(idtree_add(idtree, &allocated[i], ALLOC_MAX-1) == -1);

	/* Remove every second one. */
	for (i = 0; i < ALLOC_MAX; i += 2)
		ok1(idtree_remove(idtree, i));

	for (i = 0; i < ALLOC_MAX; i++) {
		if (i % 2 == 0)
			ok1(!idtree_lookup(idtree, i));
		else
			ok1(idtree_lookup(idtree, i) == &allocated[i]);
	}

	/* Now, finally, reallocate. */
	for (i = 0; i < ALLOC_MAX/2; i++) {
		ok1(idtree_add(idtree, &allocated[i*2], INT_MAX) == i * 2);
	}
	
	for (i = 0; i < ALLOC_MAX; i++) {
		ok1(idtree_lookup(idtree, i) == &allocated[i]);
	}
	talloc_free(ctx);
	exit(exit_status());
}
