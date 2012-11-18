#include <ccan/tal/tal.h>
#include <ccan/tal/tal.c>
#include <ccan/tap/tap.h>

static int error_count;

static void my_error(const char *msg)
{
	error_count++;
	ok1(strstr(msg, "overflow"));
}

int main(void)
{
	void *p;

	plan_tests(6);

	tal_set_backend(NULL, NULL, NULL, my_error);

	p = tal_arr(NULL, int, (size_t)-1);
	ok1(!p);
	ok1(error_count == 1);

	p = tal_arr(NULL, char, (size_t)-2);
	ok1(!p);
	ok1(error_count == 2);
	return exit_status();
}
