#include <ccan/antithread/antithread.c>
#include <assert.h>
#include <ccan/tap/tap.h>

int main(int argc, char *argv[])
{
	struct at_pool *atp;
	struct at_parent *parent;
	struct at_child *at;
	int err, *pid;
	void *arg;
	char *bad_args[] = { (char *)"/", NULL };

	parent = at_get_parent(&argc, argv, &arg);
	if (parent) {
		*(int *)arg = getpid();
		at_tell_parent(parent, arg);
		exit(0);
	}
	assert(!argv[1]);

	err = errno;
	plan_tests(9);
	ok1(err == EINVAL);

	atp = at_new_pool(1*1024*1024);
	assert(atp);
	ok1(tal_check(NULL, NULL));
	pid = tal(atp, int);
	assert(pid);
	ok1((char *)pid >= atp->map && (char *)pid < atp->map + atp->mapsize);
	ok1(tal_check(NULL, NULL));

	/* This is a failed spawn. */
	at = at_spawn(atp, pid, bad_args);
	ok1(at == NULL);

	/* This should work */
	at = at_spawn(atp, pid, argv);
	ok1(at);

	/* Should read back the pid pointer. */
	ok1(at_read_child(at) == pid);
	tal_free(at);

	ok1(*pid != 0);
	ok1(*pid != getpid());

	tal_cleanup();
	return exit_status();
}
