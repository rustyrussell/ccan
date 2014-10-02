#include <ccan/eratosthenes/eratosthenes.h>
#include <ccan/tap/tap.h>

#include <ccan/eratosthenes/eratosthenes.c>

#define LIMIT	500

#define ok_eq(a, b) \
	ok((a) == (b), "%s [%u] == %s [%u]", \
	   #a, (unsigned)(a), #b, (unsigned)(b))

static bool test_isprime(unsigned long n)
{
	int i;

	if (n < 2)
		return false;

	for (i = 2; i < n; i++)
		if ((n % i) == 0)
			return false;

	return true;
}

static unsigned long test_nextprime(struct eratosthenes *s, unsigned long n)
{
	unsigned long i = n + 1;

	while ((i < LIMIT) && !eratosthenes_isprime(s, i))
		i++;

	return (i >= LIMIT) ? 0 : i;
}

int main(void)
{
	struct eratosthenes s;
	unsigned long n;

	/* This is how many tests you plan to run */
	plan_tests(2 * LIMIT);

	eratosthenes_init(&s);

	eratosthenes_sieve(&s, LIMIT);

	for (n = 0; n < LIMIT; n++) {
		ok_eq(eratosthenes_isprime(&s, n), test_isprime(n));
		ok_eq(eratosthenes_nextprime(&s, n), test_nextprime(&s, n));
	}

	eratosthenes_reset(&s);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
