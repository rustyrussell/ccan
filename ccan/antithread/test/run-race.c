#include <ccan/antithread/antithread.c>
#include <assert.h>
#include <ccan/tap/tap.h>

#define PROCESSES 20
#define LOOPS 1000

static void *test(struct at_parent *parent, void *unused)
{
	unsigned int i = 0;
	char *p;

	p = at_read_parent(parent);
	for (i = 0; i < LOOPS; i++)
		at_tell_parent(parent, tal_arr(p, char, i));

	return NULL;
};

/* Get all children to allocate off the same node. */
int main(int argc, char *argv[])
{
	struct at_pool *atp;
	struct at_child *at[PROCESSES];
	char *p;
	unsigned int i;

	plan_tests(PROCESSES + 6);

	atp = at_new_pool(PROCESSES*(32 + LOOPS)*LOOPS);
	ok1(tal_check(NULL, NULL));
	ok1(tal_check(atp, NULL));

	p = tal(atp, char);
	ok1(at_check_pool(atp, NULL));

	for (i = 0; i < PROCESSES; i++)
		at[i] = at_run(atp, test, NULL);

	ok1(at_check_pool(atp, NULL));

	for (i = 0; i < PROCESSES; i++)
		at_tell_child(at[i], atp);

	for (i = 0; i < PROCESSES * LOOPS; i++) {
		p = at_read_child(at[i % PROCESSES]);
		assert(p);
	}
	ok1(at_check_pool(atp, NULL));

	for (i = 0; i < PROCESSES; i++)
		ok1(at_read_child(at[i]) == NULL);

	tal_free(atp);
	ok1(tal_check(NULL, NULL));

	tal_cleanup();
	return exit_status();
}
