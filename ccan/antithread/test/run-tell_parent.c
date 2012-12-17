#include <ccan/antithread/antithread.c>
#include <assert.h>
#include <ccan/tap/tap.h>

static void *test(struct at_parent *parent, int *pid)
{
	*pid = getpid();
	at_tell_parent(parent, test);
	return NULL;
};

int main(int argc, char *argv[])
{
	struct at_pool *atp;
	struct at_child *at;
	int *pid;

	plan_tests(4);

	atp = at_new_pool(1*1024*1024);
	assert(atp);
	pid = tal(atp, int);
	assert(pid);
	ok1((char *)pid >= atp->map && (char *)pid < atp->map + atp->mapsize);
	at = at_run(atp, test, pid);
	assert(at);

	ok1(at_read_child(at) == test);
	tal_free(at);

	ok1(*pid != 0);
	ok1(*pid != getpid());

	tal_cleanup();
	return exit_status();
}
