#include <ccan/antithread/antithread.c>
#include <assert.h>
#include <ccan/tap/tap.h>

static void *test(struct at_pool *atp, void *unused)
{
	char *p;
	p = at_read_parent(atp);
	at_tell_parent(atp, p + 1);
	return NULL;
};

int main(int argc, char *argv[])
{
	struct at_pool *atp;
	struct athread *at;

	plan_tests(1);

	atp = at_pool(1*1024*1024);
	assert(atp);
	at = at_run(atp, test, NULL);
	assert(at);

	at_tell(at, argv[0]);
	ok1(at_read(at) == argv[0] + 1);
	talloc_free(at);

	return exit_status();
}
