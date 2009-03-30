#include "crcsync.h"
#include <ccan/crc/crc.h>
#include <string.h>
#include <assert.h>

void crc_of_blocks(const void *data, size_t len, unsigned int block_size,
		   unsigned int crcbits, uint32_t crc[])
{
	unsigned int i;
	const uint8_t *buf = data;
	uint32_t crcmask = crcbits < 32 ? (1 << crcbits) - 1 : 0xFFFFFFFF;

	for (i = 0; len >= block_size; i++) {
		crc[i] = (crc32c(0, buf, block_size) & crcmask);
		buf += block_size;
		len -= block_size;
	}
	if (len)
		crc[i] = (crc32c(0, buf, len) & crcmask);
}

struct crc_context {
	size_t block_size;
	uint32_t crcmask;

	/* Saved old buffer bytes (block_size bytes). */
	void *buffer;
	size_t buffer_start, buffer_end;

	/* Progress so far. */
	uint32_t running_crc;
	size_t literal_bytes;
	size_t total_bytes;
	int have_match;

	/* Uncrc tab. */
	uint32_t uncrc_tab[256];

	unsigned int num_crcs;
	uint32_t crc[];
};

/* Calculate the how the crc changes when we take a give char out of the
 * crc'd area. */
static void init_uncrc_tab(uint32_t uncrc_tab[], unsigned int wsize)
{
	unsigned int i;
	uint32_t part_crc;
	uint8_t buffer[wsize];

	/* Calculate crc(buffer+1, wsize-1) once, since it doesn't change. */
	memset(buffer, 0, wsize);
	part_crc = crc32c(0, buffer+1, wsize-1);

	for (i = 0; i < 256; i++) {
		buffer[0] = i;
		uncrc_tab[i] = (crc32c(0, buffer, wsize) ^ part_crc);
	}
}

struct crc_context *crc_context_new(size_t block_size, unsigned crcbits,
				    const uint32_t crc[], unsigned num_crcs)
{
	struct crc_context *ctx;

	ctx = malloc(sizeof(*ctx) + sizeof(crc[0])*num_crcs);
	if (ctx) {
		ctx->block_size = block_size;
		/* Technically, 1 << 32 is undefined. */
		if (crcbits >= 32)
			ctx->crcmask = 0xFFFFFFFF;
		else
			ctx->crcmask = (1 << crcbits)-1;
		ctx->num_crcs = num_crcs;
		memcpy(ctx->crc, crc, sizeof(crc[0])*num_crcs);
		ctx->buffer_end = 0;
		ctx->buffer_start = 0;
		ctx->running_crc = 0;
		ctx->literal_bytes = 0;
		ctx->total_bytes = 0;
		ctx->have_match = -1;
		init_uncrc_tab(ctx->uncrc_tab, block_size);
		ctx->buffer = malloc(block_size);
		if (!ctx->buffer) {
			free(ctx);
			ctx = NULL;
		}
	}
	return ctx;
}

/* Return -1 or index into matching crc. */
static int crc_matches(const struct crc_context *ctx)
{
	unsigned int i;

	if (ctx->literal_bytes < ctx->block_size)
		return -1;

	for (i = 0; i < ctx->num_crcs; i++)
		if ((ctx->running_crc & ctx->crcmask) == ctx->crc[i])
			return i;
	return -1;
}

static uint32_t crc_add_byte(uint32_t crc, uint8_t newbyte)
{
	return crc32c(crc, &newbyte, 1);
}

static uint32_t crc_remove_byte(uint32_t crc, uint8_t oldbyte,
				const uint32_t uncrc_tab[])
{
	return crc ^ uncrc_tab[oldbyte];
}

static uint32_t crc_roll(uint32_t crc, uint8_t oldbyte, uint8_t newbyte,
			 const uint32_t uncrc_tab[])
{
	return crc_add_byte(crc_remove_byte(crc, oldbyte, uncrc_tab), newbyte);
}

static size_t buffer_size(const struct crc_context *ctx)
{
	return ctx->buffer_end - ctx->buffer_start;
}

size_t crc_read_block(struct crc_context *ctx, long *result,
		      const void *buf, size_t buflen)
{
	size_t consumed = 0, len;
	int crcmatch = -1;
	const uint8_t *old, *p = buf;

	/* Simple optimization, if we found a match last time. */
	if (ctx->have_match >= 0) {
		crcmatch = ctx->have_match;
		goto have_match;
	}

	/* old is the trailing edge of the checksum window. */
	if (buffer_size(ctx) >= ctx->block_size)
		old = ctx->buffer + ctx->buffer_start;
	else
		old = NULL;

	while (!old || (crcmatch = crc_matches(ctx)) < 0) {
		if (consumed == buflen)
			break;

		/* Update crc. */
		if (old) {
			ctx->running_crc = crc_roll(ctx->running_crc,
						    *old, *p,
						    ctx->uncrc_tab);
			old++;
			/* End of stored buffer?  Start on data they gave us. */
			if (old == ctx->buffer + ctx->buffer_end)
				old = buf;
		} else {
			ctx->running_crc = crc_add_byte(ctx->running_crc, *p);
			if (p == (uint8_t *)buf + ctx->block_size - 1)
				old = buf;
		}

		ctx->literal_bytes++;
		ctx->total_bytes++;
		consumed++;
		p++;
	}

	if (crcmatch >= 0) {
		/* We have a match! */
		if (ctx->literal_bytes > ctx->block_size) {
			/* Output literal first. */
			*result = ctx->literal_bytes - ctx->block_size;
			ctx->literal_bytes = ctx->block_size;
			/* Remember for next time! */
			ctx->have_match = crcmatch;
		} else {
		have_match:
			*result = -crcmatch-1;
			ctx->literal_bytes -= ctx->block_size;
			assert(ctx->literal_bytes == 0);
			ctx->have_match = -1;
			ctx->running_crc = 0;
			/* Nothing more in the buffer. */
			ctx->buffer_start = ctx->buffer_end = 0;
		}
	} else {
		/* Output literal if it's more than 1 block ago. */
		if (ctx->literal_bytes > ctx->block_size) {
			*result = ctx->literal_bytes - ctx->block_size;
			ctx->literal_bytes -= *result;
			ctx->buffer_start += *result;
		} else
			*result = 0;

		/* Now save any literal bytes we'll need in future. */
		len = ctx->literal_bytes - buffer_size(ctx);

		/* Move down old data if we don't have room.  */
		if (ctx->buffer_end + len > ctx->block_size) {
			memmove(ctx->buffer, ctx->buffer + ctx->buffer_start,
				buffer_size(ctx));
			ctx->buffer_end -= ctx->buffer_start;
			ctx->buffer_start = 0;
		}
		memcpy(ctx->buffer + ctx->buffer_end, buf, len);
		ctx->buffer_end += len;
		assert(buffer_size(ctx) <= ctx->block_size);
	}
	return consumed;
}

/* We could try many techniques to match the final block.  We can
 * simply try to checksum whatever's left at the end and see if it
 * matches the final block checksum.  This works for the exact-match
 * case.
 *
 * We can do slightly better than this: if we try to match the checksum
 * on every block (starting with block_size 1) from where we end to EOF,
 * we can capture the "data appended" case as well.
 */
static size_t final_block_match(struct crc_context *ctx)
{
	size_t size;
	uint32_t crc;

	if (ctx->num_crcs == 0)
		return 0;

	crc = 0;
	for (size = 0; size < buffer_size(ctx); size++) {
		const uint8_t *p = ctx->buffer;
		crc = crc_add_byte(crc, p[ctx->buffer_start+size]);
		if ((crc & ctx->crcmask) == ctx->crc[ctx->num_crcs-1])
			return size+1;
	}
	return 0;
}

long crc_read_flush(struct crc_context *ctx)
{
	long ret;
	size_t final;

	/* We might have ended right on a matched block. */
	if (ctx->have_match != -1) {
		ctx->literal_bytes -= ctx->block_size;
		assert(ctx->literal_bytes == 0);
		ret = -ctx->have_match-1;
		ctx->have_match = -1;
		ctx->running_crc = 0;
		/* Nothing more in the buffer. */
		ctx->buffer_start = ctx->buffer_end;
		return ret;
	}

	/* Look for truncated final block. */
	final = final_block_match(ctx);
	if (!final) {
		/* Nope?  Just a literal. */
		ret = buffer_size(ctx);
		ctx->buffer_start += ret;
		ctx->literal_bytes -= ret;
		return ret;
	}

	/* We matched (some of) what's left. */
	ret = -(ctx->num_crcs-1)-1;
	ctx->buffer_start += final;
	ctx->literal_bytes -= final;
	return ret;
}

/**
 * crc_context_free - free a context returned from crc_context_new.
 * @ctx: the context returned from crc_context_new, or NULL.
 */
void crc_context_free(struct crc_context *ctx)
{
	free(ctx->buffer);
	free(ctx);
}
