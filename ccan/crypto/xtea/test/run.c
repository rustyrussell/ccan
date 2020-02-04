#include <ccan/crypto/xtea/xtea.h>
/* Include the C files directly. */
#include <ccan/crypto/xtea/xtea.c>
#include <ccan/tap/tap.h>
#include <string.h>

int main(void)
{
	uint64_t v, e;	
	struct xtea_secret s;

	/* This is how many tests you plan to run */
	plan_tests(66);

	memset(&s, 1, sizeof(s));

	for (v = 1; v; v <<= 1) {
		e = xtea_encipher(&s, v);
		ok1(xtea_decipher(&s, e) == v);
	}

	/* The only 32-iteration from the "test vectors" at
	 * http://www.cix.co.uk/~klockstone/teavect.htm:
	 * in=af20a390547571aa, N=32, k=27f917b1c1da899360e2acaaa6eb923d, out=d26428af0a202283
	 */
	v = 0xaf20a390547571aaULL;
	s.u.u32[0] = 0x27f917b1;
	s.u.u32[1] = 0xc1da8993;
	s.u.u32[2] = 0x60e2acaa;
	s.u.u32[3] = 0xa6eb923d;
	e = xtea_encipher(&s, v);
	ok1(e == 0xd26428af0a202283ULL);
	ok1(xtea_decipher(&s, e) == v);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
