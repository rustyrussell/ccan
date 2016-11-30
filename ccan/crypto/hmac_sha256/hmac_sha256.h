#ifndef CCAN_CRYPTO_HMAC_SHA256_H
#define CCAN_CRYPTO_HMAC_SHA256_H
/* BSD-MIT - see LICENSE file for details */
#include "config.h"
#include <stdint.h>
#include <stdlib.h>
#include <ccan/crypto/sha256/sha256.h>

/* Uncomment this to use openssl's HMAC routines (and link with -lcrypto) */
/*#define CCAN_CRYPTO_HMAC_USE_OPENSSL 1*/

#ifdef CCAN_CRYPTO_HMAC_USE_OPENSSL
#include <openssl/hmac.h>
#endif

/**
 * struct hmac_sha256 - structure representing a completed HMAC.
 */
struct hmac_sha256 {
	struct sha256 sha;
};

/**
 * hmac_sha256 - return hmac of an object with a key.
 * @hmac: the hmac to fill in
 * @k: pointer to the key,
 * @ksize: the number of bytes pointed to by @k
 * @d: pointer to memory,
 * @dsize: the number of bytes pointed to by @d
 */
void hmac_sha256(struct hmac_sha256 *hmac,
		 const void *k, size_t ksize,
		 const void *d, size_t dsize);
#endif /* CCAN_CRYPTO_HMAC_SHA256_H */
