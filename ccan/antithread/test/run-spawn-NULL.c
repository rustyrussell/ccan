#include <ccan/antithread/antithread.c>
#include <assert.h>
#include <ccan/tap/tap.h>

int main(int argc, char *argv[])
{
	struct at_pool *atp;
	struct athread *at;
	int err;

	atp = at_get_pool(&argc, argv, NULL);
	if (atp) {
		at_tell_parent(atp, (void *)1UL);
		exit(0);
	}
	assert(!argv[1]);

	err = errno;
	plan_tests(3);
	ok1(err == EINVAL);

	atp = at_pool(1*1024*1024);
	assert(atp);

	/* This should work */
	at = at_spawn(atp, NULL, argv);
	ok1(at);

	/* Should read back the magic pointer. */
	ok1(at_read(at) == (void *)1);
	talloc_free(at);

	return exit_status();
}
