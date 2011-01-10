/* failtest with valgrind caught this read-after-free. */
#include <ccan/talloc/talloc.c>
#include <stdbool.h>
#include <ccan/tap/tap.h>

int main(int argc, char *argv[])
{
	void *root, *p1;

	plan_tests(2);
	talloc_enable_null_tracking();
	root = talloc_new(NULL);
	p1 = talloc_strdup(root, "foo");
	talloc_increase_ref_count(p1);
	talloc_free(root);
	ok1(strcmp(p1, "foo") == 0);
	talloc_unlink(NULL, p1);

	/* This closes the leak, but make sure we're not freeing unexpected. */
	ok1(!talloc_chunk_from_ptr(null_context)->child);
	talloc_disable_null_tracking();

	return exit_status();
}
