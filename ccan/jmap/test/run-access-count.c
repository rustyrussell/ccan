/* Test our access counting failures. */
#include <ccan/jmap/jmap.c>
#include <ccan/tap/tap.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

struct map {
	JMAP_MEMBERS(unsigned long, unsigned long);
};

int main(int argc, char *argv[])
{
	struct map *map;
	unsigned long *value;
	int status;

	plan_tests(9);

	map = jmap_new(struct map);
	ok1(jmap_error(map) == NULL);
	ok1(jmap_add(map, 0, 1));

	/* add while holding value. */
	value = jmap_getval(map, 0);
	ok1(value);
	if (!fork()) {
		jmap_add(map, 1, 2);
		exit(0);
	} else {
		wait(&status);
		ok1(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
	}
	jmap_putval(map, &value);

	/* del while holding value. */
	value = jmap_getval(map, 0);
	ok1(value);
	if (!fork()) {
		jmap_del(map, 0);
		exit(0);
	} else {
		wait(&status);
		ok1(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
	}
	jmap_putval(map, &value);

	ok1(jmap_add(map, 0, 1));

	/* set while holding value ok. */
	value = jmap_getval(map, 0);
	ok1(value);
	if (!fork()) {
		jmap_set(map, 0, 2);
		exit(0);
	} else {
		wait(&status);
		ok1(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	}
	jmap_putval(map, &value);

	/* FIXME: test jmap_nthval, jmap_firstval etc. */
	jmap_free(map);

	return exit_status();
}
