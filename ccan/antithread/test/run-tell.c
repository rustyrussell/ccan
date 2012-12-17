#include <ccan/antithread/antithread.c>
#include <assert.h>
#include <ccan/tap/tap.h>

static void *test(struct at_parent *parent, void *unused)
{
	char *p;
	p = at_read_parent(parent);
	at_tell_parent(parent, p + 1);
	return NULL;
};

int main(int argc, char *argv[])
{
	struct at_pool *atp;
	struct at_child *at;

	plan_tests(1);

	atp = at_new_pool(1*1024*1024);
	assert(atp);
	at = at_run(atp, test, NULL);
	assert(at);

	at_tell_child(at, argv[0]);
	ok1(at_read_child(at) == argv[0] + 1);
	tal_free(at);

	tal_cleanup();
	return exit_status();
}
