/* Licensed under GPLv2+ - see LICENSE file for details */
#ifndef CCAN_MD4_H
#define CCAN_MD4_H
#include <stdint.h>
#include <stdlib.h>

/**
 * md4_ctx - context structure for md4 hashing
 * @hash: the 128-bit hash value (only valid after md4_finish)
 * @block: internal working state.
 * @byte_count: the total number of bytes processed.
 */
struct md4_ctx {
	union {
		unsigned char bytes[16];
		uint32_t words[4];
	} hash;
	uint32_t block[16];
	uint64_t byte_count;
};

/**
 * md4_init - (re-)initialize the struct md4_ctx before md4_hash.
 * @mctx: the struct md4_ctx which will be handed to md4_hash.
 *
 * Contexts can be safely re-used by calling md4_init() on them again.
 *
 * Example:
 *	struct md4_ctx ctx;
 *
 *	md4_init(&ctx);
 *	...
 */
void md4_init(struct md4_ctx *mctx);

/**
 * md4_hash - add these bytes into the hash
 * @mctx: the struct md4_ctx.
 * @p: pointer to the bytes to hash.
 * @len: the number of bytes pointed to by @p.
 *
 * Example:
 *	struct md4_ctx ctx;
 *
 *	md4_init(&ctx);
 *	md4_hash(&ctx, "hello", 5);
 *	md4_hash(&ctx, " ", 1);
 *	md4_hash(&ctx, "world", 5);
 */
void md4_hash(struct md4_ctx *mctx, const void *p, size_t len);

/**
 * md4_finish - complete the MD4 hash
 * @mctx: the struct md4_ctx.
 *
 * Example:
 *	struct md4_ctx ctx;
 *
 *	md4_init(&ctx);
 *	md4_hash(&ctx, "hello world", strlen("hello world"));
 *	md4_finish(&ctx);
 */
void md4_finish(struct md4_ctx *mctx);

#endif /* CCAN_MD4_H */
