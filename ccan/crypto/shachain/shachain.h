/* MIT (BSD) license - see LICENSE file for details */
#ifndef CCAN_CRYPTO_SHACHAIN_H
#define CCAN_CRYPTO_SHACHAIN_H
#include "config.h"
#include <ccan/crypto/sha256/sha256.h>
#include <stdbool.h>
#include <stdint.h>

/* Useful for testing. */
#ifndef shachain_index_t
#define shachain_index_t uint64_t
#endif

void shachain_from_seed(const struct sha256 *seed, shachain_index_t index,
			struct sha256 *hash);

struct shachain {
	shachain_index_t max_index;
	unsigned int num_valid;
	struct {
		shachain_index_t index;
		struct sha256 hash;
	} known[sizeof(shachain_index_t) * 8];
};

void shachain_init(struct shachain *shachain);

bool shachain_add_hash(struct shachain *shachain,
		       shachain_index_t index, const struct sha256 *hash);

bool shachain_get_hash(const struct shachain *shachain,
		       shachain_index_t index, struct sha256 *hash);
#endif /* CCAN_CRYPTO_SHACHAIN_H */
