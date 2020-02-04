/* CC0 license (public domain) - see LICENSE file for details */
#include "xtea.h"

/* Based on http://www.cix.co.uk/~klockstone/xtea.pdf, and the modernized
 * source at https://en.wikipedia.org/wiki/XTEA */

/* Each round below represents two rounds, so we usee 32 not 64 here */
#define NUM_DOUBLE_ROUNDS 32

uint64_t xtea_encipher(const struct xtea_secret *secret, uint64_t v)
{
	const uint32_t delta=0x9E3779B9;
	uint32_t v0=(v>>32), v1=v, sum=0;

	for (int i=0; i < NUM_DOUBLE_ROUNDS; i++) {
		v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + secret->u.u32[sum & 3]);
		sum += delta;
		v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + secret->u.u32[(sum>>11) & 3]);
	}
	return ((uint64_t)v0 << 32) | v1;
}

uint64_t xtea_decipher(const struct xtea_secret *secret, uint64_t e)
{
	const uint32_t delta=0x9E3779B9;
	uint32_t v0=(e>>32), v1=e, sum=delta*NUM_DOUBLE_ROUNDS;

	for (int i=0; i < NUM_DOUBLE_ROUNDS; i++) {
		v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + secret->u.u32[(sum>>11) & 3]);
		sum -= delta;
		v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + secret->u.u32[sum & 3]);
	}
	return ((uint64_t)v0 << 32) | v1;
}
