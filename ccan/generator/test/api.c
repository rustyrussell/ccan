#include <ccan/generator/generator.h>
#include <ccan/tap/tap.h>
#include <ccan/str/str.h>

#include "example-gens.h"

generator_def_static(genx, const char *)
{
	generator_yield("one");
	generator_yield("two");
	generator_yield("three");
	generator_yield("four");
}

static void test1(void)
{
	generator_t(int) state1 = gen1();
	int *ret;

	ok1((ret = generator_next(state1)) != NULL);
	ok1(*ret == 1);
	ok1((ret = generator_next(state1)) != NULL);
	ok1(*ret == 3);
	ok1((ret = generator_next(state1)) != NULL);
	ok1(*ret == 17);
	ok1((ret = generator_next(state1)) == NULL);

	/* Test that things don't go bad if we try to invoke an
	 * already completed generator */
	ok1((ret = generator_next(state1)) == NULL);

	generator_free(state1);
}

static void test2(void)
{
	generator_t(int) state2 = gen2(100);
	int *ret;

	ok1((ret = generator_next(state2)) != NULL);
	ok1(*ret == 101);
	ok1((ret = generator_next(state2)) != NULL);
	ok1(*ret == 103);
	ok1((ret = generator_next(state2)) != NULL);
	ok1(*ret == 117);
	ok1((ret = generator_next(state2)) == NULL);

	generator_free(state2);
}

static void test3(void)
{
	int i;

	for (i = 0; i < 4; i++) {
		generator_t(const char *) state3 = gen3("test", i);
		const char *s;
		int j;

		for (j = 0; j < i; j++) {
			ok1(generator_next_val(s, state3));
			ok1(streq(s, "test"));
		}
		ok1(!generator_next_val(s, state3));
		generator_free(state3);
	}
}

static void testx(void)
{
	generator_t(const char *) statex = genx();
	const char *val;

	ok1(generator_next_val(val, statex));
	ok1(streq(val, "one"));
	ok1(generator_next_val(val, statex));
	ok1(streq(val, "two"));
	ok1(generator_next_val(val, statex));
	ok1(streq(val, "three"));
	ok1(generator_next_val(val, statex));
	ok1(streq(val, "four"));
	ok1(!generator_next_val(val, statex));
	generator_free(statex);
}

int main(void)
{
	/* This is how many tests you plan to run */
	plan_tests(8 + 7 + 16 + 9);

	test1();
	test2();
	test3();
	testx();

	/* This exits depending on whether all tests passed */
	return exit_status();
}
