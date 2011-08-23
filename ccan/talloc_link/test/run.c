#include <ccan/talloc_link/talloc_link.h>
#include <ccan/tap/tap.h>
#include <ccan/talloc_link/talloc_link.c>
#include <stdlib.h>
#include <err.h>

static unsigned int destroy_count = 0;
static int destroy_obj(void *obj)
{
	destroy_count++;
	return 0;
}

int main(int argc, char *argv[])
{
	void *obj, *p1, *p2, *p3;

	plan_tests(16);

	talloc_enable_leak_report();

	/* Single parent case. */
	p1 = talloc(NULL, char);
	obj = talloc_linked(p1, talloc(NULL, char));
	talloc_set_destructor(obj, destroy_obj);

	ok(destroy_count == 0, "destroy_count = %u", destroy_count);
	talloc_free(p1);
	ok(destroy_count == 1, "destroy_count = %u", destroy_count);

	/* Dual parent case. */
	p1 = talloc(NULL, char);
	obj = talloc_linked(p1, talloc(NULL, char));
	talloc_set_destructor(obj, destroy_obj);
	p2 = talloc(NULL, char);
	talloc_link(p2, obj);

	talloc_free(p1);
	ok(destroy_count == 1, "destroy_count = %u", destroy_count);
	talloc_free(p2);
	ok(destroy_count == 2, "destroy_count = %u", destroy_count);
	
	/* Triple parent case. */
	p1 = talloc(NULL, char);
	obj = talloc_linked(p1, talloc(NULL, char));
	talloc_set_destructor(obj, destroy_obj);

	p2 = talloc(NULL, char);
	p3 = talloc(NULL, char);

	talloc_link(p2, obj);
	talloc_link(p3, obj);

	talloc_free(p1);
	ok(destroy_count == 2, "destroy_count = %u", destroy_count);
	talloc_free(p2);
	ok(destroy_count == 2, "destroy_count = %u", destroy_count);
	talloc_free(p3);
	ok(destroy_count == 3, "destroy_count = %u", destroy_count);

	/* Single delink case. */
	p1 = talloc(NULL, char);
	obj = talloc_linked(p1, talloc(NULL, char));
	talloc_set_destructor(obj, destroy_obj);

	ok(destroy_count == 3, "destroy_count = %u", destroy_count);
	talloc_delink(p1, obj);
	ok(destroy_count == 4, "destroy_count = %u", destroy_count);
	talloc_free(p1);

	/* Double delink case. */
	p1 = talloc(NULL, char);
	obj = talloc_linked(p1, talloc(NULL, char));
	talloc_set_destructor(obj, destroy_obj);

	p2 = talloc(NULL, char);
	talloc_link(p2, obj);

	talloc_delink(p1, obj);
	ok(destroy_count == 4, "destroy_count = %u", destroy_count);
	talloc_delink(p2, obj);
	ok(destroy_count == 5, "destroy_count = %u", destroy_count);
	talloc_free(p1);
	talloc_free(p2);

	/* Delink and free. */
	p1 = talloc(NULL, char);
	obj = talloc_linked(p1, talloc(NULL, char));
	talloc_set_destructor(obj, destroy_obj);
	p2 = talloc(NULL, char);
	talloc_link(p2, obj);

	talloc_delink(p1, obj);
	ok(destroy_count == 5, "destroy_count = %u", destroy_count);
	talloc_free(p2);
	ok(destroy_count == 6, "destroy_count = %u", destroy_count);
	talloc_free(p1);

	/* Free and delink. */
	p1 = talloc(NULL, char);
	obj = talloc_linked(p1, talloc(NULL, char));
	talloc_set_destructor(obj, destroy_obj);
	p2 = talloc(NULL, char);
	talloc_link(p2, obj);

	talloc_free(p1);
	ok(destroy_count == 6, "destroy_count = %u", destroy_count);
	talloc_delink(p2, obj);
	ok(destroy_count == 7, "destroy_count = %u", destroy_count);
	talloc_free(p2);

	/* No leaks? */
	ok1(talloc_total_size(NULL) == 0);

	talloc_disable_null_tracking();

	return exit_status();
}
