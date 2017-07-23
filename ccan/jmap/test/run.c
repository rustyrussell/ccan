#include <ccan/tap/tap.h>
#define CCAN_JMAP_DEBUG
#include <ccan/jmap/jmap.c>

struct map {
	JMAP_MEMBERS(unsigned long, unsigned long);
};

int main(int argc, char *argv[])
{
	struct map *map;
	unsigned long i, *value;
	const char *err;

	plan_tests(51);

	map = jmap_new(struct map);
	ok1(jmap_error(map) == NULL);

	ok1(jmap_test(map, 0) == false);
	ok1(jmap_del(map, 0) == false);
	ok1(jmap_add(map, 0, 1) == true);
	ok1(jmap_test(map, 0) == true);
	ok1(jmap_get(map, 0) == 1);
	ok1(jmap_get(map, 1) == 0);
	ok1(jmap_del(map, 0) == true);

	ok1(jmap_popcount(map, 0, -1) == 0);
	ok1(jmap_nth(map, 0, 0) == 0);
	ok1(jmap_nth(map, 0, -1) == (size_t)-1);
	ok1(jmap_first(map) == 0);
	ok1(jmap_last(map) == 0);

	ok1(jmap_getval(map, 0) == NULL);

	/* Map a million indices, 16 apart. */
	for (i = 0; i < 1000000; i++)
		jmap_add(map, (i << 4) + 1, (i << 5) + 1);

	/* This only take 6.3MB on my 32-bit system. */
	diag("%u bytes memory used\n", (unsigned)JudyLMemUsed(jmap_raw_(map)));

	ok1(jmap_get(map, 1) == 1);
	ok1(jmap_get(map, (999999 << 4) + 1) == (999999 << 5) + 1);
	ok1(jmap_popcount(map, 0, -1) == 1000000);
	ok1(jmap_nth(map, 0, -1) == 1);
	ok1(jmap_nth(map, 999999, -1) == (999999 << 4) + 1);
	ok1(jmap_nth(map, 1000000, -1) == (size_t)-1);
	ok1(jmap_first(map) == 1);
	ok1(jmap_last(map) == (999999 << 4) + 1);
	ok1(jmap_next(map, 1) == (1 << 4) + 1);
	ok1(jmap_next(map, (999999 << 4) + 1) == 0);
	ok1(jmap_prev(map, 2) == 1);
	ok1(jmap_prev(map, 0) == 0);
	ok1(jmap_error(map) == NULL);

	/* Accessors. */
	value = jmap_getval(map, 1);
	ok1(value && *value == 1);
	*value = 2;
	ok1(jmap_get(map, 1) == 2);
	jmap_putval(map, &value);
	ok1(jmap_get(map, 1) == 2);
	ok1(jmap_set(map, 1, 1));

	value = jmap_getval(map, (999999 << 4) + 1);
	ok1(value && *value == (999999 << 5) + 1);
	jmap_putval(map, &value);

	value = jmap_nthval(map, 0, &i);
	ok1(i == 1);
	ok1(value && *value == 1);
	jmap_putval(map, &value);
	value = jmap_nthval(map, 999999, &i);
	ok1(i == (999999 << 4) + 1);
	ok1(value && *value == (999999 << 5) + 1);
	jmap_putval(map, &value);
	ok1(jmap_nthval(map, 1000000, &i) == NULL);

	value = jmap_firstval(map, &i);
	ok1(i == 1);
	ok1(value && *value == 1);
	jmap_putval(map, &value);
	ok1(jmap_prevval(map, &i) == NULL);

	i = 1;
	value = jmap_nextval(map, &i);
	ok1(i == (1 << 4) + 1);
	ok1(value && *value == (1 << 5) + 1);
	jmap_putval(map, &value);

	value = jmap_lastval(map, &i);
	ok1(i == (999999 << 4) + 1);
	ok1(value && *value == (999999 << 5) + 1);
	jmap_putval(map, &value);
	ok1(jmap_nextval(map, &i) == NULL);

	i = (999999 << 4) + 1;
	value = jmap_prevval(map, &i);
	ok1(i == (999998 << 4) + 1);
	ok1(value && *value == (999998 << 5) + 1);
	jmap_putval(map, &value);

	/* Test error handling */
	JU_ERRNO(&jmap_raw_(map)->err) = 100;
	JU_ERRID(&jmap_raw_(map)->err) = 991;
	err = jmap_error(map);
	ok1(err);
	ok1(strstr(err, "100"));
	ok1(strstr(err, "991"));
	ok1(err == jmap_raw_(map)->errstr);
	jmap_free(map);

	return exit_status();
}
