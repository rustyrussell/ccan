#ifndef CCAN_SIPHASH_H_
#define CCAN_SIPHASH_H_

/* Licensed under GPL v2+ - see LICENSE file for details */
/* Written in 2013 by Ulrik Sverdrup */

#include <stdint.h>
#include <stddef.h>

/**
 * siphash - a keyed hash function
 *
 * SipHash-c-d, where `c` is the number of rounds per message chunk
 *              and `d` the number of finalization rounds,
 * "is a family of pseudorandom functions optimized for speed on short
 * messages"
 *
 * Implemented from the paper https://131002.net/siphash/
 * The designers recommend using SipHash-2-4 or SipHash-4-8
 *
 * SipHash-c-d uses a 16-byte key.
 * To defend against hash-flooding, the application needs to use
 * a new random key regularly.
 *
 * The designers of SipHash claim it is cryptographically strong
 * to use as MAC with secret key but _not_collision_resistant_.
 *
 * Returns one 64-bit word as the hash function result.
 *
 * Example:
 *	// Outputs cf2794e0277187b7
 *	#include <stdio.h>
 *	#include <ccan/siphash/siphash.h>
 *
 *	int main(void)
 *	{
 * 		unsigned char t_key[16] =
 *			{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
 *		unsigned char data[4] = "\0\1\2\3";
 *		uint64_t hash = siphash_2_4(data, sizeof(data), t_key);
 *		printf("%llx\n", (unsigned long long)hash);
 *
 *		return 0;
 *	}
 *
 */
uint64_t siphash_2_4(const void *in, size_t len, const unsigned char key[16]);

#endif /* CCAN_SIPHASH_H_ */

