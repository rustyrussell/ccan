#include <ccan/antithread/antithread.c>
#include <assert.h>
#include <unistd.h>
#include <ccan/tap/tap.h>

#define NUM_RUNS 100

static void *test(struct at_parent *parent, int *val)
{
	unsigned int i;

	if (at_read_parent(parent) != test) {
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
	struct at_child *at;
	int *val, i;

	plan_tests(3);

	atp = at_new_pool(1*1024*1024);
	assert(atp);
	val = talz(atp, int);
	at = at_run(atp, test, val);
	assert(at);

	ok1(*val == 0);

	at_tell_child(at, test);

	/* We increment val, then sleep a little. */
	for (i = 0; i < NUM_RUNS; i++) {
		at_lock(val);
		(*(volatile int *)val)++;
		usleep(i * 100);
		at_unlock(val);
		usleep(i * 100);
	}
	ok1(at_read_child(at) == val);
	tal_free(at);

	ok1(*val == NUM_RUNS*2);

	tal_cleanup();
	return exit_status();
}
