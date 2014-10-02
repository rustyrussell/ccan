/* Licensed under LGPLv2+ - see LICENSE file for details */
#include <ccan/eratosthenes/eratosthenes.h>
#include <ccan/bitmap/bitmap.h>

#include <assert.h>
#include <stdlib.h>

#define VAL_TO_BIT(v)		(((v) - 3) / 2)
#define LIMIT_TO_NBITS(l)	((l > 2) ? ((l) - 2) / 2 : 0)

#define BIT_TO_VAL(b)		(((b) * 2) + 3)

void eratosthenes_init(struct eratosthenes *s)
{
	s->limit = 0;
	s->b = NULL;
}

void eratosthenes_reset(struct eratosthenes *s)
{
	if (s->b)
		free(s->b);
	eratosthenes_init(s);
}

static void eratosthenes_once(struct eratosthenes *s, unsigned long limit, unsigned long p)
{
	unsigned long n = VAL_TO_BIT(3*p);
	unsigned long obits = LIMIT_TO_NBITS(s->limit);

	if (obits > n) {
		n = obits + p - 1 - ((obits - n - 1) % p);
	}

	assert((BIT_TO_VAL(n) % p) == 0);
	assert((BIT_TO_VAL(n) / p) > 1);

	while (n < LIMIT_TO_NBITS(limit)) {
		bitmap_clear_bit(s->b, n);
		n += p;
	}
}

static void eratosthenes_sieve_(struct eratosthenes *s, unsigned long limit)
{
	unsigned long p = 3;

	while ((p * p) < limit) {
		unsigned long n;

		eratosthenes_once(s, limit, p);

		n = bitmap_ffs(s->b, VAL_TO_BIT(p) + 1, LIMIT_TO_NBITS(limit));

		/* We should never run out of primes */
		assert(n < LIMIT_TO_NBITS(limit));

		p = BIT_TO_VAL(n);
	}
}

void eratosthenes_sieve(struct eratosthenes *s, unsigned long limit)
{
	if ((limit < 3) || (limit <= s->limit))
		/* Nothing to do */
		return;

	if (s->limit < 3)
		s->b = bitmap_alloc1(LIMIT_TO_NBITS(limit));
	else
		s->b = bitmap_realloc1(s->b, LIMIT_TO_NBITS(s->limit),
				       LIMIT_TO_NBITS(limit));

	if (!s->b)
		abort();

	eratosthenes_sieve_(s, limit);

	s->limit = limit;
}

bool eratosthenes_isprime(const struct eratosthenes *s, unsigned long n)
{
	assert(n < s->limit);

	if ((n % 2) == 0)
		return (n == 2);

	if (n < 3) {
		assert(n == 1);
		return false;
	}

	return bitmap_test_bit(s->b, VAL_TO_BIT(n));
}

unsigned long eratosthenes_nextprime(const struct eratosthenes *s, unsigned long n)
{
	unsigned long i;

	if ((n + 1) >= s->limit)
		return 0;

	if (n < 2)
		return 2;

	if (n == 2)
		return 3;

	i = bitmap_ffs(s->b, VAL_TO_BIT(n) + 1, LIMIT_TO_NBITS(s->limit));
	if (i == LIMIT_TO_NBITS(s->limit))
		/* Reached the end of the sieve */
		return 0;

	return BIT_TO_VAL(i);
}
