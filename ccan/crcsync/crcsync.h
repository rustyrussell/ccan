/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef CCAN_CRCSYNC_H
#define CCAN_CRCSYNC_H
#include <stdint.h>
#include <stddef.h>

/**
 * crc_of_blocks - calculate the crc of the blocks.
 * @data: pointer to the buffer to CRC
 * @len: length of the buffer
 * @blocksize: CRC of each block (final block may be shorter)
 * @crcbits: the number of bits of crc you want (currently 64 maximum)
 * @crc: the crcs (array will have (len + blocksize-1)/blocksize entries).
 *
 * Calculates the CRC of each block, and output the lower @crcbits to
 * @crc array.
 */
void crc_of_blocks(const void *data, size_t len, unsigned int blocksize,
		   unsigned int crcbits, uint64_t crc[]);

/**
 * crc_context_new - allocate and initialize state for crc_find_block
 * @blocksize: the size of each block
 * @crcbits: the bits valid in the CRCs (<= 64)
 * @crc: array of block crcs (including final block, if any)
 * @num_crcs: number of block crcs
 * @tail_size: the size of final partial block, if any (< blocksize).
 *
 * Returns an allocated pointer to the structure for crc_find_block,
 * or NULL.  Makes a copy of @crc.
 */
struct crc_context *crc_context_new(size_t blocksize, unsigned crcbits,
				    const uint64_t crc[], unsigned num_crcs,
				    size_t final_size);

/**
 * crc_read_block - search for block matches in the buffer.
 * @ctx: struct crc_context from crc_context_new.
 * @result: unmatched bytecount, or crc which matched.
 * @buf: pointer to bytes
 * @buflen: length of buffer
 *
 * Returns the number of bytes of the buffer which have been digested,
 * and sets @result either to a negagive number (== -crc_number - 1)
 * to show that a block matched a crc, or zero or more to represent a
 * length of unmatched data.
 *
 * Note that multiple lengths of unmatched data might be returned in a row:
 * you'll probably want to merge them yourself.
 */
size_t crc_read_block(struct crc_context *ctx, long *result,
		      const void *buf, size_t buflen);

/**
 * crc_read_flush - flush any remaining data from crc_read_block.
 * @ctx: the context passed to crc_read_block.
 *
 * Matches the final data.  This can be used to create a boundary, or
 * simply flush the final data.  Keep calling it until it returns 0.
 */
long crc_read_flush(struct crc_context *ctx);

/**
 * crc_context_free - free a context returned from crc_context_new.
 * @ctx: the context returned from crc_context_new, or NULL.
 */
void crc_context_free(struct crc_context *ctx);

#endif /* CCAN_CRCSYNC_H */
