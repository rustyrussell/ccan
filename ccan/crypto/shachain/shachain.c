/* MIT (BSD) license - see LICENSE file for details */
#include <ccan/crypto/shachain/shachain.h>
#include <ccan/ilog/ilog.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

static void change_bit(unsigned char *arr, size_t index)
{
	arr[index / CHAR_BIT] ^= (1 << (index % CHAR_BIT));
}

static void derive(shachain_index_t index, size_t bits, struct sha256 *hash)
{
	int i;

	for (i = bits - 1; i >= 0; i--) {
		if (!((index >> i) & 1)) {
			change_bit(hash->u.u8, i);
			sha256(hash, hash, 1);
		}
	}
}

void shachain_from_seed(const struct sha256 *seed, shachain_index_t index,
			struct sha256 *hash)
{
	*hash = *seed;
	derive(index, sizeof(index) * CHAR_BIT, hash);
}

void shachain_init(struct shachain *shachain)
{
	shachain->num_valid = 0;
}

/* We can only ever *unset* bits, so to must only have bits in from. */
static bool can_derive(shachain_index_t from, shachain_index_t to)
{
	return (~from & to) == 0;
}

void shachain_add_hash(struct shachain *chain,
		       shachain_index_t index, const struct sha256 *hash)
{
	int i;

	for (i = 0; i < chain->num_valid; i++) {
		/* If we could derive this value, we don't need it,
		 * not any others (since they're in order). */
		if (can_derive(index, chain->known[i].index))
			break;
	}

	/* This can happen if you skip indices! */
	assert(i < sizeof(chain->known) / sizeof(chain->known[0]));
	chain->known[i].index = index;
	chain->known[i].hash = *hash;
	chain->num_valid = i+1;
}

bool shachain_get_hash(const struct shachain *chain,
		       shachain_index_t index, struct sha256 *hash)
{
	int i;

	for (i = 0; i < chain->num_valid; i++) {
		shachain_index_t diff;

		/* If we can get from key to index only by resetting bits,
		 * we can derive from it => index has no bits key doesn't. */
		if (!can_derive(chain->known[i].index, index))
			continue;

		/* Start from this hash. */
		*hash = chain->known[i].hash;

		/* This indicates the bits which are in 'index' and
		 * not the key */
		diff = index ^ chain->known[i].index;

		/* Using ilog64 here is an optimization. */
		derive(~diff, ilog64(diff), hash);
		return true;
	}
	return false;
}
