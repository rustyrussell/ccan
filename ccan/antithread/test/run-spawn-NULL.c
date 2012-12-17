#include <ccan/antithread/antithread.c>
#include <assert.h>
#include <ccan/tap/tap.h>

int main(int argc, char *argv[])
{
	struct at_pool *pool;
	struct at_parent *parent;
	struct at_child *at;
	int err;

	parent = at_get_parent(&argc, argv, NULL);
	if (parent) {
		at_tell_parent(parent, (void *)1UL);
		exit(0);
	}
	assert(!argv[1]);

	err = errno;
	plan_tests(3);
	ok1(err == EINVAL);

	pool = at_new_pool(1*1024*1024);
	assert(pool);

	/* This should work */
	at = at_spawn(pool, NULL, argv);
	ok1(at);

	/* Should read back the magic pointer. */
	ok1(at_read_child(at) == (void *)1);
	tal_free(at);

	tal_cleanup();
	return exit_status();
}
