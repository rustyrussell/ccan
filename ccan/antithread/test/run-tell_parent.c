#include "antithread/antithread.c"
#include <assert.h>
#include "tap/tap.h"

static void *test(struct at_pool *atp, int *pid)
{
	*pid = getpid();
	at_tell_parent(atp, test);
	return NULL;
};

int main(int argc, char *argv[])
{
	struct at_pool *atp;
	struct athread *at;
	int *pid;

	plan_tests(4);

	atp = at_pool(1*1024*1024);
	assert(atp);
	pid = talloc(at_pool_ctx(atp), int);
	assert(pid);
	ok1((char *)pid >= (char *)atp->pool
	    && (char *)pid < (char *)atp->pool + atp->poolsize);
	at = at_run(atp, test, pid);
	assert(at);

	ok1(at_read(at) == test);
	talloc_free(at);

	ok1(*pid != 0);
	ok1(*pid != getpid());

	return exit_status();
}
