#include <ccan/talloc/talloc.c>
#include <stdbool.h>
#include <ccan/tap/tap.h>

static int destroy_int(int *p)
{
	ok1(*p == 7);
	_exit(0);
}

int main(int argc, char *argv[])
{
	int *p;

	/* If autofree context doesn't work, we won't run all tests! */
	plan_tests(1);

	p = talloc(talloc_autofree_context(), int);
	*p = 7;
	talloc_set_destructor(p, destroy_int);

	/* Note!  We fail here, unless destructor called! */
	exit(1);
}
	
