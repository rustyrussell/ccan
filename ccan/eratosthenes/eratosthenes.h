/* Licensed under LGPLv2+ - see LICENSE file for details */
#ifndef CCAN_ERATOSTHENES_H_
#define CCAN_ERATOSTHENES_H_

#include "config.h"

#include <stdbool.h>

#include <ccan/bitmap/bitmap.h>

struct eratosthenes {
	unsigned long limit;
	bitmap *b;
};

void eratosthenes_init(struct eratosthenes *s);

void eratosthenes_reset(struct eratosthenes *s);

void eratosthenes_sieve(struct eratosthenes *s, unsigned long limit);

bool eratosthenes_isprime(const struct eratosthenes *s, unsigned long n);

unsigned long eratosthenes_nextprime(const struct eratosthenes *s,
				     unsigned long n);

#endif /* CCAN_ERATOSTHENES_H_ */
