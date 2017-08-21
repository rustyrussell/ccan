#include <ccan/tap/tap.h>
#include <ccan/jset/jset.c>

int main(int argc, char *argv[])
{
	struct jset_long {
		JSET_MEMBERS(unsigned long);
	} *set;
	size_t i;
	const char *err;

	plan_tests(34);

	set = jset_new(struct jset_long);
	ok1(jset_error(set) == NULL);

	ok1(jset_set(set, 0) == true);
	ok1(jset_set(set, 0) == false);
	ok1(jset_clear(set, 0) == true);
	ok1(jset_clear(set, 0) == false);
	ok1(jset_popcount(set, 0, -1) == 0);
	ok1(jset_nth(set, 0, 0) == 0);
	ok1(jset_nth(set, 0, -1) == (size_t)-1);
	ok1(jset_first(set) == 0);
	ok1(jset_last(set) == 0);
	ok1(jset_first_clear(set) == 1);
	ok1(jset_last_clear(set) == (size_t)-1);
	ok1(jset_prev_clear(set, 1) == 0);
	ok1(jset_next_clear(set, 1) == 2);
	ok1(jset_next_clear(set, -1) == 0);

	/* Set a million bits, 16 bits apart. */
	for (i = 0; i < 1000000; i++)
		jset_set(set, 1 + (i << 4));

	/* This only take 1.7MB on my 32-bit system. */
	diag("%u bytes memory used\n",
	     (unsigned)Judy1MemUsed(jset_raw_(set)->judy));

	ok1(jset_popcount(set, 0, -1) == 1000000);
	ok1(jset_nth(set, 0, -1) == 1);
	ok1(jset_nth(set, 999999, -1) == 1 + (999999 << 4));
	ok1(jset_nth(set, 1000000, -1) == (size_t)-1);
	ok1(jset_first(set) == 1);
	ok1(jset_last(set) == 1 + (999999 << 4));
	ok1(jset_first_clear(set) == 2);
	ok1(jset_last_clear(set) == (size_t)-1);
	ok1(jset_prev_clear(set, 3) == 2);
	ok1(jset_prev_clear(set, 2) == 0);
	ok1(jset_next(set, 1) == 1 + (1 << 4));
	ok1(jset_next(set, 1 + (999999 << 4)) == 0);
	ok1(jset_prev(set, 1) == 0);
	ok1(jset_prev(set, 2) == 1);
	ok1(jset_error(set) == NULL);

	/* Test error handling */
	JU_ERRNO(&jset_raw_(set)->err) = 100;
	JU_ERRID(&jset_raw_(set)->err) = 991;
	err = jset_error(set);
	ok1(err);
	ok1(strstr(err, "100"));
	ok1(strstr(err, "991"));
	ok1(err == jset_raw_(set)->errstr);
	jset_free(set);

	return exit_status();
}
