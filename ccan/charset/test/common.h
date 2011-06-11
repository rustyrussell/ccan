#include <stdint.h>
#include <stdlib.h>

/*
 * Finds a pseudorandom 32-bit number from 0 to 2^32-1 .
 * Uses the BCPL linear congruential generator method.
 *
 * Used instead of system RNG to ensure tests are consistent.
 */
static uint32_t rand32(void)
{
#if 0
	/*
	 * Tests should be run with a different random function
	 * from time to time.  I've found that the method below
	 * sometimes behaves poorly for testing purposes.
	 * For example, rand32() % N might only return even numbers.
	 */
	assert(RAND_MAX == 2147483647);
	return ((random() & 0xFFFF) << 16) | (random() & 0xFFFF);
#else
	static uint32_t rand32_state = 0;
	rand32_state *= (uint32_t)0x7FF8A3ED;
	rand32_state += (uint32_t)0x2AA01D31;
	return rand32_state;
#endif
}
