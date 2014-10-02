#include <ccan/eratosthenes/eratosthenes.h>
#include <ccan/tap/tap.h>

#include <ccan/eratosthenes/eratosthenes.c>

#define LIMIT	500

#define ok_eq(a, b) \
	ok((a) == (b), "%s [%u] == %s [%u]", \
	   #a, (unsigned)(a), #b, (unsigned)(b))

int main(void)
{
	struct eratosthenes s1, s2;
	unsigned long n;

	/* This is how many tests you plan to run */
	plan_tests(LIMIT);

	eratosthenes_init(&s1);
	eratosthenes_sieve(&s1, LIMIT);

	eratosthenes_init(&s2);
	for (n = 1; n <= LIMIT; n++)
		eratosthenes_sieve(&s2, n);

	for (n = 0; n < LIMIT; n++)
		ok1(eratosthenes_isprime(&s1, n)
		    == eratosthenes_isprime(&s2, n));

	eratosthenes_reset(&s1);
	eratosthenes_reset(&s2);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
