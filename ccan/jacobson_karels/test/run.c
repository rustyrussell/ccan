#include <ccan/jacobson_karels/jacobson_karels.h>
#include <ccan/tap/tap.h>

static void first_test(void)
{
	struct jacobson_karels_state s;

	jacobson_karels_init(&s, 0, 0);
	jacobson_karels_update(&s, 200);

	ok1(jacobson_karels_timeout(&s, 2, 1000) == 225);
}

int main(void)
{
	/* This is how many tests you plan to run */
	plan_tests(1);

	first_test();

	/* This exits depending on whether all tests passed */
	return exit_status();
}
