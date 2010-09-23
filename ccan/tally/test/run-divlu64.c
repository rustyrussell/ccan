#include <ccan/tally/tally.c>
#include <ccan/tap/tap.h>

int main(void)
{
	unsigned int i, j;

	plan_tests(5985);
	/* Simple tests. */
	for (i = 0; i < 127; i++) {
		uint64_t u1, u0;
		if (i < 64) {
			u1 = 0;
			u0 = 1ULL << i;
			j = 0;
		} else {
			u1 = 1ULL << (i - 64);
			u0 = 0;
			j = i - 63;
		}
		for (; j < 63; j++) {
			uint64_t answer;
			if (j > i)
				answer = 0;
			else
				answer = 1ULL << (i - j);
			ok1(divlu64(u1, u0, 1ULL << j) == answer);
		}
	}
	return exit_status();
}
