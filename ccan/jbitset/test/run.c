#include <ccan/tap/tap.h>
#include <ccan/jbitset/jbitset.c>

int main(int argc, char *argv[])
{
	struct jbitset *set;
	size_t i;
	const char *err;

	plan_tests(36);

	set = jbit_new();
	ok1(jbit_error(set) == NULL);

	ok1(jbit_set(set, 0) == true);
	ok1(jbit_set(set, 0) == false);
	ok1(jbit_clear(set, 0) == true);
	ok1(jbit_clear(set, 0) == false);
	ok1(jbit_popcount(set, 0, -1) == 0);
	ok1(jbit_nth(set, 0, 0) == 0);
	ok1(jbit_nth(set, 0, -1) == (size_t)-1);
	ok1(jbit_first(set, 0) == 0);
	ok1(jbit_first(set, -1) == (size_t)-1);
	ok1(jbit_last(set, 0) == 0);
	ok1(jbit_last(set, -1) == (size_t)-1);
	ok1(jbit_first_clear(set, -1) == 0);
	ok1(jbit_first_clear(set, -2) == 0);
	ok1(jbit_last_clear(set, 0) == (size_t)-1);
	ok1(jbit_prev_clear(set, 1, -1) == 0);
	ok1(jbit_next_clear(set, 0, -1) == 1);
	ok1(jbit_next_clear(set, -1, -1) == -1);

	/* Set a million bits, 16 bits apart. */
	for (i = 0; i < 1000000; i++)
		jbit_set(set, i << 4);

	/* This only take 1.7MB on my 32-bit system. */
	diag("%u bytes memory used\n", (unsigned)Judy1MemUsed(set->judy));
	
	ok1(jbit_popcount(set, 0, -1) == 1000000);
	ok1(jbit_nth(set, 0, -1) == 0);
	ok1(jbit_nth(set, 999999, -1) == 999999 << 4);
	ok1(jbit_nth(set, 1000000, -1) == (size_t)-1);
	ok1(jbit_first(set, -1) == 0);
	ok1(jbit_last(set, -1) == 999999 << 4);
	ok1(jbit_first_clear(set, -1) == 1);
	ok1(jbit_last_clear(set, 0) == (size_t)-1);
	ok1(jbit_prev_clear(set, 1, -1) == (size_t)-1);
	ok1(jbit_next(set, 0, -1) == 1 << 4);
	ok1(jbit_next(set, 999999 << 4, -1) == (size_t)-1);
	ok1(jbit_prev(set, 1, -1) == 0);
	ok1(jbit_prev(set, 0, -1) == (size_t)-1);
	ok1(jbit_error(set) == NULL);

	/* Test error handling */
	JU_ERRNO(&set->err) = 100;
	JU_ERRID(&set->err) = 991;
	err = jbit_error(set);
	ok1(err);
	ok1(strstr(err, "100"));
	ok1(strstr(err, "991"));
	ok1(err == set->errstr);
	jbit_free(set);

	return exit_status();
}
