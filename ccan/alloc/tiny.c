#include "tiny.h"
#include "bitops.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* One byte header, one byte data. */
#define MIN_BLOCK_SIZE 2

/* Bit 7 (in any byte) == start or end. */
#define TERM_BIT 0x80
/* Bit 6 (first and last byte) == one byte long. */
#define SINGLE_BYTE 0x40
/* Bit 5 (of first byte) == "is this block free?" */
#define FREE_BIT 0x20

/* Val is usually offset by MIN_BLOCK_SIZE here. */
static unsigned encode_length(unsigned long val)
{
	unsigned int bits = fls(val);
	/* 5 bits in first byte. */
	if (bits <= 5)
		return 1;
	/* 6 bits in last byte, 7 bits in middle ones. */
	return 2 + (bits - 5) / 7;
}

/* Header is included in length, so we might need an extra byte. */
static unsigned encode_len_with_header(unsigned long len)
{
	unsigned int hdrlen = 1;

	assert(len);
	while (encode_length(len + hdrlen - MIN_BLOCK_SIZE) != hdrlen)
		hdrlen = encode_length(len + hdrlen - MIN_BLOCK_SIZE);

	return hdrlen;
}

/* Encoding can be read from front or back; 0 is invalid at either
 * start or end.  Returns bytes used for header, or 0 if won't fit. */
static unsigned encode(unsigned long len, bool free, unsigned char arr[],
		       size_t limit)
{
	unsigned int hdrlen = 1;

	/* We can never have a length < MIN_BLOCK_SIZE. */
	assert(len >= MIN_BLOCK_SIZE);
	len -= MIN_BLOCK_SIZE;

	if (encode_length(len) > limit)
		return 0;

	/* First byte always contains free bit. */
	arr[0] = TERM_BIT | (free ? FREE_BIT : 0);
	/* Also holds 5 bits of data (0 - 31) */
	arr[0] |= (len & 0x1F);
	len >>= 5;

	/* One byte only? */
	if (!len) {
		arr[0] |= SINGLE_BYTE;
		return hdrlen;
	}

	/* Middle bytes. */
	while (len >= (1 << 6)) {
		/* Next 7 data bits */
		arr[hdrlen++] = (len & 0x7F);
		len >>= 7;
	}
	arr[hdrlen++] = (len | TERM_BIT);
	return hdrlen;
}

/* Returns bytes used for header. */
static unsigned decode(unsigned long *len, bool *free, const unsigned char *arr)
{
	unsigned int hdrlen = 0, bits = 5;

	/* Free flag is in bit 5 */
	*free = (arr[hdrlen] & FREE_BIT);
	/* Bottom five bits are data. */
	*len = (arr[hdrlen] & 0x1f);
	if (!(arr[hdrlen++] & SINGLE_BYTE)) {
		/* Multi-byte encoding? */
		while (!(arr[hdrlen] & TERM_BIT)) {
			/* 7 more data bits. */
			*len |= (arr[hdrlen] & 0x7fUL) << bits;
			hdrlen++;
			bits += 7;
		}
		/* Final byte has 6 bits. */
		*len |= (arr[hdrlen] & 0x3fUL) << bits;
		hdrlen++;
	}

	*len += MIN_BLOCK_SIZE;
	return hdrlen;
}

/* We keep a recently-freed array, one byte per k. */
static unsigned long free_array_size(unsigned long poolsize)
{
	return poolsize / 1024;
}

void tiny_alloc_init(void *pool, unsigned long poolsize)
{
	/* We start with free array, and then the rest is free. */
	unsigned long arrsize = free_array_size(poolsize);

	/* Do nothing with 1 byte or less! */
	if (poolsize < MIN_BLOCK_SIZE)
		return;

	memset(pool, 0, arrsize);
	encode(poolsize - arrsize, true,
	       (unsigned char *)pool + arrsize, poolsize - arrsize);
}

/* Walk through and try to coalesce */
static bool try_coalesce(unsigned char *pool, unsigned long poolsize)
{
	unsigned long len, hdrlen, prev_off = 0, prev_len = 0, off;
	bool free, prev_free = false, coalesced = false;

	off = free_array_size(poolsize);
	do {
		hdrlen = decode(&len, &free, pool + off);
		if (free && prev_free) {
			encode(prev_len + len, true, pool + prev_off,
			       poolsize - prev_off);
			coalesced = true;
		}
		prev_free = free;
		prev_off = off;
		prev_len = len;
		off += len;
	} while (off < poolsize);

	return coalesced;
}

static bool long_enough(unsigned long offset, unsigned long len,
			unsigned long size, unsigned long align)
{
	unsigned long end = offset + len;

	offset += encode_len_with_header(len);
	offset = align_up(offset, align);
	return offset + size <= end;
}

void *tiny_alloc_get(void *pool, unsigned long poolsize,
		     unsigned long size, unsigned long align)
{
	unsigned long arrsize = free_array_size(poolsize);
	unsigned long len, off, actual, hdr, hdrlen;
	bool free;

	/* We can't do anything with tiny pools. */
	if (poolsize < MIN_BLOCK_SIZE)
		return NULL;

	/* We don't do zero-allocs; allows 1 more offset in encoding. */
	if (!size)
		size = 1;

	/* FIXME: Look through free array. */

again:
	off = arrsize;

	hdrlen = decode(&len, &free, (unsigned char *)pool + off);
	while (!free || !long_enough(off, len, size, align)) {
		/* FIXME: Refill free array if this block is free. */

		/* Hit end? */
		off += len;
		if (off == poolsize) {
			if (try_coalesce(pool, poolsize))
				goto again;
			return NULL;
		}
		hdrlen = decode(&len, &free, (unsigned char *)pool + off);
	}

	/* We have a free block.  Since we walk from front, take far end. */
	actual = ((off + len - size) & ~(align - 1));
	hdr = actual - encode_len_with_header(off + len - actual);

	/* Do we have enough room to split? */
	if (hdr - off >= MIN_BLOCK_SIZE) {
		encode(hdr - off, true, (unsigned char *)pool + off, poolsize);
	} else {
		hdr = off;
	}

	/* Make sure that we are all-zero up to actual, so we can walk back
	 * and find header. */
	memset((unsigned char *)pool + hdr, 0, actual - hdr);

	/* Create header for allocated block. */
	encode(off + len - hdr, false, (unsigned char *)pool + hdr, poolsize);

	return (unsigned char *)pool + actual;
}

static unsigned char *to_hdr(void *p)
{
	unsigned char *hdr = p;

	/* Walk back to find end of header. */
	while (!*(--hdr));
	assert(*hdr & TERM_BIT);

	/* Now walk back to find start of header. */
	if (!(*hdr & SINGLE_BYTE)) {
		while (!(*(--hdr) & TERM_BIT));
	}
	return hdr;
}

void tiny_alloc_free(void *pool, unsigned long poolsize, void *freep)
{
	unsigned char *hdr;

	/* Too small to do anything. */
	if (poolsize < MIN_BLOCK_SIZE)
		return;

	hdr = to_hdr(freep);

	/* FIXME: Put in free array. */
	hdr[0] |= FREE_BIT;
}

unsigned long tiny_alloc_size(void *pool, unsigned long poolsize, void *p)
{
	unsigned char *hdr = to_hdr(p);
	unsigned long len, hdrlen;
	bool free;

	hdrlen = decode(&len, &free, hdr);
	return len - hdrlen;
}

/* Useful for gdb breakpoints. */
static bool tiny_check_fail(void)
{
	return false;
}

bool tiny_alloc_check(void *pool, unsigned long poolsize)
{
	unsigned long arrsize = free_array_size(poolsize);
	unsigned char *arr = pool;
	unsigned long len, off, hdrlen;
	bool free;

	if (poolsize < MIN_BLOCK_SIZE)
		return true;

	/* For the moment, free array is all zeroes. */
	for (off = 0; off < arrsize; off++) {
		if (arr[off] != 0)
			return tiny_check_fail();
	}

	for (off = arrsize; off < poolsize; off += len) {
		/* We should have a valid header. */
		if (!(arr[off] & TERM_BIT))
			return tiny_check_fail();
		if (!(arr[off] & SINGLE_BYTE)) {
			unsigned long off2;
			for (off2 = off+1; off2 < poolsize; off2++) {
				if (arr[off2] & TERM_BIT)
					break;
			}
			if (off2 == poolsize)
				return tiny_check_fail();
		}
		hdrlen = decode(&len, &free, arr + off);
		if (off + len > poolsize)
			return tiny_check_fail();
		if (hdrlen != encode_length(len - MIN_BLOCK_SIZE))
			return tiny_check_fail();
	}
	return true;
}

/* FIXME: Implement. */
void tiny_alloc_visualize(FILE *out, void *pool, unsigned long poolsize)
{
}
