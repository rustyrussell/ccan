#include <ccan/tal/autoptr/autoptr.h>
/* Include the C files directly. */
#include <ccan/tal/autoptr/autoptr.c>
#include <ccan/tap/tap.h>

int main(void)
{
	char *p1, *p2, *p3;
	struct autonull *a;

	/* This is how many tests you plan to run */
	plan_tests(8);

	p1 = tal(NULL, char);

	// Sets p1 to point to p2.
	autonull_set_ptr(NULL, &p2, p1);
	ok1(p2 == p1);
	tal_free(p1);
	ok1(p2 == NULL);

	// Using p1 as the parent is the same. */
	p1 = tal(NULL, char);
	autonull_set_ptr(p1, &p2, p1);
	ok1(p2 == p1);
	tal_free(p1);
	ok1(p2 == NULL);

	// Freeing autodata deactivates it.
	p1 = tal(NULL, char);
	a = autonull_set_ptr(NULL, &p2, p1);
	ok1(p2 == p1);
	tal_free(a);
	tal_free(p1);
	ok1(p2 == p1);

	// Making p3 the parent means freeing p3 deactivates it.
	p3 = tal(NULL, char);
	p1 = tal(NULL, char);
	autonull_set_ptr(p3, &p2, p1);
	ok1(p2 == p1);
	tal_free(p3);
	tal_free(p1);
	ok1(p2 == p1);


	/* This exits depending on whether all tests passed */
	return exit_status();
}
