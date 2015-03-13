#include "ntdb-source.h"
#include "tap-interface.h"

static unsigned int dumb_fls(uint64_t num)
{
	int i;

	for (i = 63; i >= 0; i--) {
		if (num & (1ULL << i))
			break;
	}
	return i + 1;
}

int main(int argc, char *argv[])
{
	unsigned int i, j;

	plan_tests(64 * 64 + 2);

	ok1(fls64(0) == 0);
	ok1(dumb_fls(0) == 0);

	for (i = 0; i < 64; i++) {
		for (j = 0; j < 64; j++) {
			uint64_t val = (1ULL << i) | (1ULL << j);
			ok(fls64(val) == dumb_fls(val),
			   "%llu -> %u should be %u", (long long)val,
			   fls64(val), dumb_fls(val));
		}
	}
	return exit_status();
}
