/*
 * This code implements the MD4 message-digest algorithm.
 * The algorithm is due to Ron Rivest.	This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 * Todd C. Miller modified the MD5 code to do MD4 based on RFC 1186.
 *
 * CCAN-ized by Rusty Russell, 2013, based on Public Domain part of
 * FreeBSD's contrib/wpa/src/crypto/md4-internal.c.
 */
#include "md4.h"
#include <ccan/endian/endian.h>
#include <ccan/array_size/array_size.h>
#include <string.h>

#define	MD4_BLOCK_LENGTH	sizeof(((struct md4_ctx *)0)->buffer)

static inline void le32_to_cpu_array(uint32_t *buf, unsigned int words)
{
	while (words--) {
		*buf = le32_to_cpu(*buf);
		buf++;
	}
}

static inline void cpu_to_le32_array(uint32_t *buf, unsigned int words)
{
	while (words--) {
		*buf = cpu_to_le32(*buf);
		buf++;
	}
}

/* The three core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) ((x & y) | (x & z) | (y & z))
#define F3(x, y, z) (x ^ y ^ z)

/* This is the central step in the MD4 algorithm. */
#define MD4STEP(f, w, x, y, z, data, s) \
	( w += f(x, y, z) + data,  w = w<<s | w>>(32-s) )

/*
 * The core of the MD4 algorithm, this alters an existing MD4 hash to
 * reflect the addition of 16 longwords of new data.  MD4Update blocks
 * the data and converts bytes into longwords for this routine.
 */
static void
MD4Transform(uint32_t state[4], const uint8_t block[MD4_BLOCK_LENGTH])
{
	uint32_t a, b, c, d, in[MD4_BLOCK_LENGTH / 4];

	memcpy(in, block, sizeof(in));
	cpu_to_le32_array(in, ARRAY_SIZE(in));

	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];

	MD4STEP(F1, a, b, c, d, in[ 0],  3);
	MD4STEP(F1, d, a, b, c, in[ 1],  7);
	MD4STEP(F1, c, d, a, b, in[ 2], 11);
	MD4STEP(F1, b, c, d, a, in[ 3], 19);
	MD4STEP(F1, a, b, c, d, in[ 4],  3);
	MD4STEP(F1, d, a, b, c, in[ 5],  7);
	MD4STEP(F1, c, d, a, b, in[ 6], 11);
	MD4STEP(F1, b, c, d, a, in[ 7], 19);
	MD4STEP(F1, a, b, c, d, in[ 8],  3);
	MD4STEP(F1, d, a, b, c, in[ 9],  7);
	MD4STEP(F1, c, d, a, b, in[10], 11);
	MD4STEP(F1, b, c, d, a, in[11], 19);
	MD4STEP(F1, a, b, c, d, in[12],  3);
	MD4STEP(F1, d, a, b, c, in[13],  7);
	MD4STEP(F1, c, d, a, b, in[14], 11);
	MD4STEP(F1, b, c, d, a, in[15], 19);

	MD4STEP(F2, a, b, c, d, in[ 0] + 0x5a827999,  3);
	MD4STEP(F2, d, a, b, c, in[ 4] + 0x5a827999,  5);
	MD4STEP(F2, c, d, a, b, in[ 8] + 0x5a827999,  9);
	MD4STEP(F2, b, c, d, a, in[12] + 0x5a827999, 13);
	MD4STEP(F2, a, b, c, d, in[ 1] + 0x5a827999,  3);
	MD4STEP(F2, d, a, b, c, in[ 5] + 0x5a827999,  5);
	MD4STEP(F2, c, d, a, b, in[ 9] + 0x5a827999,  9);
	MD4STEP(F2, b, c, d, a, in[13] + 0x5a827999, 13);
	MD4STEP(F2, a, b, c, d, in[ 2] + 0x5a827999,  3);
	MD4STEP(F2, d, a, b, c, in[ 6] + 0x5a827999,  5);
	MD4STEP(F2, c, d, a, b, in[10] + 0x5a827999,  9);
	MD4STEP(F2, b, c, d, a, in[14] + 0x5a827999, 13);
	MD4STEP(F2, a, b, c, d, in[ 3] + 0x5a827999,  3);
	MD4STEP(F2, d, a, b, c, in[ 7] + 0x5a827999,  5);
	MD4STEP(F2, c, d, a, b, in[11] + 0x5a827999,  9);
	MD4STEP(F2, b, c, d, a, in[15] + 0x5a827999, 13);

	MD4STEP(F3, a, b, c, d, in[ 0] + 0x6ed9eba1,  3);
	MD4STEP(F3, d, a, b, c, in[ 8] + 0x6ed9eba1,  9);
	MD4STEP(F3, c, d, a, b, in[ 4] + 0x6ed9eba1, 11);
	MD4STEP(F3, b, c, d, a, in[12] + 0x6ed9eba1, 15);
	MD4STEP(F3, a, b, c, d, in[ 2] + 0x6ed9eba1,  3);
	MD4STEP(F3, d, a, b, c, in[10] + 0x6ed9eba1,  9);
	MD4STEP(F3, c, d, a, b, in[ 6] + 0x6ed9eba1, 11);
	MD4STEP(F3, b, c, d, a, in[14] + 0x6ed9eba1, 15);
	MD4STEP(F3, a, b, c, d, in[ 1] + 0x6ed9eba1,  3);
	MD4STEP(F3, d, a, b, c, in[ 9] + 0x6ed9eba1,  9);
	MD4STEP(F3, c, d, a, b, in[ 5] + 0x6ed9eba1, 11);
	MD4STEP(F3, b, c, d, a, in[13] + 0x6ed9eba1, 15);
	MD4STEP(F3, a, b, c, d, in[ 3] + 0x6ed9eba1,  3);
	MD4STEP(F3, d, a, b, c, in[11] + 0x6ed9eba1,  9);
	MD4STEP(F3, c, d, a, b, in[ 7] + 0x6ed9eba1, 11);
	MD4STEP(F3, b, c, d, a, in[15] + 0x6ed9eba1, 15);

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
}

#define PUT_64BIT_LE(cp, value) do {					\
	(cp)[7] = (value) >> 56;					\
	(cp)[6] = (value) >> 48;					\
	(cp)[5] = (value) >> 40;					\
	(cp)[4] = (value) >> 32;					\
	(cp)[3] = (value) >> 24;					\
	(cp)[2] = (value) >> 16;					\
	(cp)[1] = (value) >> 8;						\
	(cp)[0] = (value); } while (0)

#define PUT_32BIT_LE(cp, value) do {					\
	(cp)[3] = (value) >> 24;					\
	(cp)[2] = (value) >> 16;					\
	(cp)[1] = (value) >> 8;						\
	(cp)[0] = (value); } while (0)

static uint8_t PADDING[MD4_BLOCK_LENGTH] = {
	0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * Start MD4 accumulation.
 * Set bit count to 0 and buffer to mysterious initialization constants.
 */
void md4_init(struct md4_ctx *ctx)
{
	ctx->byte_count = 0;
	ctx->hash.words[0] = 0x67452301;
	ctx->hash.words[1] = 0xefcdab89;
	ctx->hash.words[2] = 0x98badcfe;
	ctx->hash.words[3] = 0x10325476;
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
void md4_hash(struct md4_ctx *ctx, const void *p, size_t len)
{
	size_t have, need;
	const unsigned char *input = p;

	/* Check how many bytes we already have and how many more we need. */
	have = (size_t)((ctx->byte_count >> 3) & (MD4_BLOCK_LENGTH - 1));
	need = MD4_BLOCK_LENGTH - have;

	/* Update bitcount */
	ctx->byte_count += (uint64_t)len << 3;

	if (len >= need) {
		if (have != 0) {
			memcpy(ctx->buffer + have, input, need);
			MD4Transform(ctx->hash.words, ctx->buffer);
			input += need;
			len -= need;
			have = 0;
		}

		/* Process data in MD4_BLOCK_LENGTH-byte chunks. */
		while (len >= MD4_BLOCK_LENGTH) {
			MD4Transform(ctx->hash.words, input);
			input += MD4_BLOCK_LENGTH;
			len -= MD4_BLOCK_LENGTH;
		}
	}

	/* Handle any remaining bytes of data. */
	if (len != 0)
		memcpy(ctx->buffer + have, input, len);
}

/*
 * Pad pad to 64-byte boundary with the bit pattern
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
static void MD4Pad(struct md4_ctx *ctx)
{
	uint64_t count;
	size_t padlen;

	/* Convert count to 8 bytes in little endian order. */
	count = cpu_to_le64(ctx->byte_count);

	/* Pad out to 56 mod 64. */
	padlen = MD4_BLOCK_LENGTH -
	    ((ctx->byte_count >> 3) & (MD4_BLOCK_LENGTH - 1));
	if (padlen < 1 + 8)
		padlen += MD4_BLOCK_LENGTH;
	md4_hash(ctx, PADDING, padlen - 8);		/* padlen - 8 <= 64 */
	md4_hash(ctx, &count, 8);
}

/*
 * Final wrapup--call MD4Pad, fill in digest and zero out ctx.
 */
void md4_finish(struct md4_ctx *ctx)
{
	MD4Pad(ctx);
	cpu_to_le32_array(ctx->hash.words, ARRAY_SIZE(ctx->hash.words));
}


