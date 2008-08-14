#ifndef MD5_FINDER_H
#define MD5_FINDER_H
#include <stdint.h>
#include <stdbool.h>

#define MD5_HASH_WORDS		4

#define u32 uint32_t
#define u64 uint64_t
#define u8 uint8_t

struct md5_search {
	u32 mask[MD5_HASH_WORDS];
	u32 md5[MD5_HASH_WORDS];
	bool success;
	unsigned int num_tries;
	unsigned int num_bytes;
	u8 *pattern;
};

/* Child writes this value initially to say "ready". */
#define INITIAL_POINTER ((void *)1)

#endif /* MD5_FINDER_H */
