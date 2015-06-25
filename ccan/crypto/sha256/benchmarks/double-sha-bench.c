/* Bitcoin does a lot of SHA of SHA.  Benchmark that. */
#include <ccan/crypto/sha256/sha256.h>
#include <ccan/time/time.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
	struct timeabs start;
	struct timerel diff;
	size_t i, n;
	struct sha256 h;

	n = atoi(argv[1] ? argv[1] : "1000000");
	sha256(&h, &n, sizeof(n));
	start = time_now();
	for (i = 0; i < n; i++)
		sha256(&h, &h, sizeof(h));
	diff = time_divide(time_between(time_now(), start), n);
	printf("Hashing time for %02x%02x%02x%02x%02x%02x... is %llu nsec\n",
	       h.u.u8[0], h.u.u8[1], h.u.u8[2], h.u.u8[3], h.u.u8[4], h.u.u8[5],
	       (unsigned long long)time_to_nsec(diff));
	return 0;
}
