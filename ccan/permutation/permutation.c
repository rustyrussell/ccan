/* GNU LGPL version 2 (or later) - see LICENSE file for details */
#include "config.h"

#include <assert.h>

#include <ccan/build_assert/build_assert.h>
#include <ccan/mem/mem.h>
#include <ccan/permutation/permutation.h>

unsigned long long permutation_count(int n)
{
	unsigned long long count = 1;
	int i;

	for (i = 1; i <=n; i++)
		count *= i;

	return count;
}

struct plain_change_state {
	int8_t dir : 2;
	uint8_t pos : PERMUTATION_MAX_SHIFT;
};

static inline struct plain_change_state *get_state(struct permutation *pi)
{
	BUILD_ASSERT(sizeof(struct plain_change_state) == 1);

	return (struct plain_change_state *)(pi + 1) + pi->n;
}

struct permutation *permutation_new(int n)
{
	struct permutation *pi;
	int i;
	struct plain_change_state *xs;

	assert(n <= PERMUTATION_MAX_ITEMS);

	pi = malloc(sizeof(*pi) + 2 * n);
	if (!pi)
		return NULL;

	pi->n = n;

	xs = get_state(pi);

	for (i = 0; i < pi->n; i++) {
		pi->v[i] = i;
		xs[i].pos = i;
		xs[i].dir = (i == 0) ? 0 : -1;
	}

	return pi;
}

permutation_swap_t permutation_change(struct permutation *pi)
{
	int v, w, i;
	int a, b, vdir;
	struct plain_change_state *xs = get_state(pi);

	if (!pi->n)
		return 0;

	for (v = pi->n - 1; v > 0; v--) {
		if (xs[v].dir)
			break;
	}

	a = xs[v].pos;
	vdir = xs[v].dir;
	if (!vdir)
		return 0;

	b = a + vdir;
	w = pi->v[b];

	pi->v[a] = w;
	pi->v[b] = v;
	xs[v].pos = b;
	xs[w].pos = a;

	/* If we reach the end, or the next item is larger, set
	 * direction to 0 */
	if ((b == 0) || (b == (pi->n-1))
	    || (pi->v[b + vdir] > v))
		xs[v].dir = 0;

	/* Reset direction on all elements greater than the chosen one */
	for (i = v + 1; i < pi->n; i++) {
		if (xs[i].pos < b)
			xs[i].dir = 1;
		else
			xs[i].dir = -1;
	}

	return (b * PERMUTATION_MAX_ITEMS) + a;
}
