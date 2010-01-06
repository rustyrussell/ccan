#include <ccan/antithread/antithread.c>
#include <assert.h>
#include <unistd.h>
#include <ccan/tap/tap.h>

#define NUM_RUNS 100

static void *test(struct at_pool *atp, int *val)
{
	unsigned int i;

	if (at_read_parent(atp) != test) {
		diag("Woah, at_read said bad");
		return NULL;
	}

	/* We increment val, then sleep a little. */
	for (i = 0; i < NUM_RUNS; i++) {
		at_lock(val);
		(*(volatile int *)val)++;
		usleep(i * 100);
		at_unlock(val);
		usleep(i * 100);
	}

	return val;
};

int main(int argc, char *argv[])
{
	struct at_pool *atp;
	struct athread *at;
	int *val, i;

	plan_tests(3);

	atp = at_pool(1*1024*1024);
	assert(atp);
	val = talloc_zero(at_pool_ctx(atp), int);
	at = at_run(atp, test, val);
	assert(at);

	ok1(*val == 0);

	at_tell(at, test);

	/* We increment val, then sleep a little. */
	for (i = 0; i < NUM_RUNS; i++) {
		at_lock(val);
		(*(volatile int *)val)++;
		usleep(i * 100);
		at_unlock(val);
		usleep(i * 100);
	}
	ok1(at_read(at) == val);
	talloc_free(at);

	ok1(*val == NUM_RUNS*2);

	return exit_status();
}
