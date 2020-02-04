/* CC0 license (public domain) - see LICENSE file for details */
#ifndef CCAN_CRYPTO_XTEA_H
#define CCAN_CRYPTO_XTEA_H
/* Public domain - see LICENSE file for details */
#include "config.h"
#include <stdint.h>

/**
 * struct xtea_secret - secret to use for xtea encryption
 * @u.u8: an unsigned char array.
 * @u.u32: a 32-bit integer array.
 * @u.u64: a 64-bit integer array.
 *
 * Other fields may be added to the union in future.
 */
struct xtea_secret {
	union {
		/* Array of chars */
		unsigned char u8[16];
		/* Array of uint32_t */
		uint32_t u32[4];
		/* Array of uint64_t */
		uint64_t u64[2];
	} u;
};

/**
 * xtea_encipher - encrypt a 64-bit value.
 * @secret: the xtea secret
 * @v: the 64 bit value
 *
 * Returns the 64-bit encrypted value: use xtea_decipher to decrypt.
 */
uint64_t xtea_encipher(const struct xtea_secret *secret, uint64_t v);

/**
 * xtea_decipher - decrypt a 64-bit value.
 * @secret: the xtea secret
 * @e: the 64 bit encrypted value
 *
 * Returns the 64-bit decryptted value.
 */
uint64_t xtea_decipher(const struct xtea_secret *secret, uint64_t e);

#endif /* CCAN_CRYPTO_SIPHASH24_H */
