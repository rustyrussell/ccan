#include "config.h"

#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include <ccan/array_size/array_size.h>
#include <ccan/permutation/permutation.h>
#include <ccan/tap/tap.h>

#define MAX_ITEMS	10

#define PERMUTE(pi, arr)					\
	do {							\
		ok1(PERMUTATION_CHANGE_ARRAY((pi), (arr)));	\
	} while (0)

#define CHECK_ORDER(a, t, ...)					\
	do {							\
		t cmp[] = { __VA_ARGS__ };			\
		ok1(memcmp((a), cmp, sizeof(cmp)) == 0);	\
	} while (0)

#define WORDLEN		6
typedef char word[WORDLEN];

int main(void)
{
	struct permutation *pi;
	int single = 12345;
	char pair[] = { 'P', 'Q' };
	uint16_t triple[] = {7, 9, 1000};
	word four[] = {"ZERO", "ONE", "TWO", "THREE"};
	int i;

	plan_tests(2 * permutation_count(1) + 1
		   + 2 * permutation_count(2) + 1
		   + 2 * permutation_count(3) + 1
		   + 2 * permutation_count(4) + 1
		   + MAX_ITEMS + 1);

	/* One */
	pi = permutation_new(1);
	CHECK_ORDER(&single, int, 12345);
	ok1(!permutation_change_array(pi, &single, sizeof(single)));
	CHECK_ORDER(&single, int, 12345);
	free(pi);

	/* Pair */
	pi = PERMUTATION_NEW(pair);
	CHECK_ORDER(pair, char, 'P', 'Q');
	PERMUTE(pi, pair);
	CHECK_ORDER(pair, char, 'Q', 'P');
	ok1(!PERMUTATION_CHANGE_ARRAY(pi, pair));
	CHECK_ORDER(pair, char, 'Q', 'P');
	free(pi);

	/* Triple */
	pi = PERMUTATION_NEW(triple);
	CHECK_ORDER(triple, uint16_t, 7, 9, 1000);
	PERMUTE(pi, triple);
	CHECK_ORDER(triple, uint16_t, 7, 1000, 9);
	PERMUTE(pi, triple);
	CHECK_ORDER(triple, uint16_t, 1000, 7, 9);
	PERMUTE(pi, triple);
	CHECK_ORDER(triple, uint16_t, 1000, 9, 7);
	PERMUTE(pi, triple);
	CHECK_ORDER(triple, uint16_t, 9, 1000, 7);
	PERMUTE(pi, triple);
	CHECK_ORDER(triple, uint16_t, 9, 7, 1000);
	ok1(!PERMUTATION_CHANGE_ARRAY(pi, triple));
	CHECK_ORDER(triple, uint16_t, 9, 7, 1000);
	free(pi);

	/* Four */
	pi = PERMUTATION_NEW(four);
	CHECK_ORDER(four, word, "ZERO", "ONE", "TWO", "THREE");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "ZERO", "ONE", "THREE", "TWO");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "ZERO", "THREE", "ONE", "TWO");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "THREE", "ZERO", "ONE", "TWO");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "THREE", "ZERO", "TWO", "ONE");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "ZERO", "THREE", "TWO", "ONE");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "ZERO", "TWO", "THREE", "ONE");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "ZERO", "TWO", "ONE", "THREE");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "TWO", "ZERO", "ONE", "THREE");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "TWO", "ZERO", "THREE", "ONE");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "TWO", "THREE", "ZERO", "ONE");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "THREE", "TWO", "ZERO", "ONE");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "THREE", "TWO", "ONE", "ZERO");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "TWO", "THREE", "ONE", "ZERO");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "TWO", "ONE", "THREE", "ZERO");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "TWO", "ONE", "ZERO", "THREE");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "ONE", "TWO", "ZERO", "THREE");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "ONE", "TWO", "THREE", "ZERO");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "ONE", "THREE", "TWO", "ZERO");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "THREE", "ONE", "TWO", "ZERO");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "THREE", "ONE", "ZERO", "TWO");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "ONE", "THREE", "ZERO", "TWO");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "ONE", "ZERO", "THREE", "TWO");
	PERMUTE(pi, four);
	CHECK_ORDER(four, word, "ONE", "ZERO", "TWO", "THREE");
	ok1(!PERMUTATION_CHANGE_ARRAY(pi, four));
	CHECK_ORDER(four, word, "ONE", "ZERO", "TWO", "THREE");
	free(pi);

	for (i = 0; i <= MAX_ITEMS; i++) {
		uint64_t nperms = 1;

		diag("Counting permutations of %d\n", i);

		pi = permutation_new(i);
		while (permutation_change(pi))
			nperms++;

		ok(nperms == permutation_count(i),
		   "%"PRId64" permutations of %d (%d! == %lld)",
		   nperms, i, i, permutation_count(i));
		free(pi);
	}

	/* This exits depending on whether all tests passed */

	return exit_status();
}
