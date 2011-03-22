#include <ccan/antithread/antithread.c>
#include <assert.h>
#include <ccan/tap/tap.h>

int main(int argc, char *argv[])
{
	struct at_pool *atp;
	struct athread *at;
	int err, *pid;
	void *arg;
	char *bad_args[] = { (char *)"/", NULL };

	atp = at_get_pool(&argc, argv, &arg);
	if (atp) {
		*(int *)arg = getpid();
		at_tell_parent(atp, arg);
		exit(0);
	}
	assert(!argv[1]);

	err = errno;
	plan_tests(7);
	ok1(err == EINVAL);

	atp = at_pool(1*1024*1024);
	assert(atp);
	pid = talloc(at_pool_ctx(atp), int);
	assert(pid);
	ok1((char *)pid >= (char *)atp->p->pool
	    && (char *)pid < (char *)atp->p->pool + atp->p->poolsize);

	/* This is a failed spawn. */
	at = at_spawn(atp, pid, bad_args);
	ok1(at == NULL);

	/* This should work */
	at = at_spawn(atp, pid, argv);
	ok1(at);

	/* Should read back the pid pointer. */
	ok1(at_read(at) == pid);
	talloc_free(at);

	ok1(*pid != 0);
	ok1(*pid != getpid());

	return exit_status();
}
