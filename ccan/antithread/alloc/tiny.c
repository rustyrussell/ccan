/* Licensed under LGPLv2.1+ - see LICENSE file for details */
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

#define MAX_FREE_CACHED_SIZE 256

/* Val is usually offset by MIN_BLOCK_SIZE here. */
static unsigned encode_length(unsigned long val)
{
	unsigned int bits = afls(val);
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
 * start or end.  Returns bytes used for header. */
static unsigned encode(unsigned long len, bool free, unsigned char arr[])
{
	unsigned int hdrlen = 1;

	/* We can never have a length < MIN_BLOCK_SIZE. */
	assert(len >= MIN_BLOCK_SIZE);
	len -= MIN_BLOCK_SIZE;

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

/* We keep a helper array for freed mem, one byte per k. */
static unsigned long free_array_size(unsigned long poolsize)
{
	return poolsize / 1024;
}

/* We have series of 69 free sizes like so:
 * 1, 2, 3, 4.  6, 8, 10, 12, 14, 16. 20, 24, 28, 32... 252.
 */
static unsigned long free_array_off(unsigned long size)
{
	unsigned long off;

	if (size <= 4)
		off = size - 1;
	else if (size <= 16)
		off = size / 2 + 1;
	else
		off = size / 4 + 5;

	off *= 3;
	return off;
}

void tiny_alloc_init(void *pool, unsigned long poolsize)
{
	/* We start with free array, and then the rest is free. */
	unsigned long arrsize = free_array_size(poolsize);

	/* Do nothing with 1 byte or less! */
	if (poolsize < MIN_BLOCK_SIZE)
		return;

	memset(pool, 0, arrsize);
	encode(poolsize - arrsize, true, (unsigned char *)pool + arrsize);
}

/* Walk through and try to coalesce */
static bool try_coalesce(unsigned char *pool, unsigned long poolsize)
{
	unsigned long len, prev_off = 0, prev_len = 0, off;
	bool free, prev_free = false, coalesced = false;

	off = free_array_size(poolsize);
	do {
		decode(&len, &free, pool + off);
		if (free && prev_free) {
			prev_len += len;
			encode(prev_len, true, pool + prev_off);
			coalesced = true;
		} else {
			prev_free = free;
			prev_off = off;
			prev_len = len;
		}
		off += len;
	} while (off < poolsize);

	/* Clear the free array. */
	if (coalesced)
		memset(pool, 0, free_array_size(poolsize));

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

static void add_to_free_array(unsigned char *arr,
			      unsigned long poolsize,
			      unsigned long size,
			      unsigned long off)
{
	unsigned long fa_off;

	if (size >= MAX_FREE_CACHED_SIZE)
		return;

	for (fa_off = free_array_off(size);
	     fa_off + 3 < free_array_size(poolsize);
	     fa_off += free_array_off(MAX_FREE_CACHED_SIZE)) {
		if (!arr[fa_off] && !arr[fa_off+1] && !arr[fa_off+2]) {
			arr[fa_off] = (off >> 16);
			arr[fa_off+1] = (off >> 8);
			arr[fa_off+2] = off;
			break;
		}
	}
}

void *tiny_alloc_get(void *pool, unsigned long poolsize,
		     unsigned long size, unsigned long align)
{
	unsigned long arrsize = free_array_size(poolsize);
	unsigned long len, off, actual, hdr, free_bucket;
	long fa_off;
	unsigned char *arr = pool;
	bool free, coalesced = false;

	/* We can't do anything with tiny pools. */
	if (poolsize < MIN_BLOCK_SIZE)
		return NULL;

	/* We don't do zero-allocs; allows 1 more offset in encoding. */
	if (!size)
		size = 1;

	/* Look for entries in free array, starting from right size up. */
	for (free_bucket = free_array_off(size);
	     free_bucket < free_array_off(MAX_FREE_CACHED_SIZE);
	     free_bucket += 3) {
		for (fa_off = free_bucket;
		     fa_off + 3 < free_array_size(poolsize);
		     fa_off += free_array_off(MAX_FREE_CACHED_SIZE)) {
			off = ((unsigned long)arr[fa_off]) << 16
				| ((unsigned long)arr[fa_off+1]) << 8
				| ((unsigned long)arr[fa_off+2]);
			if (!off)
				continue;

			decode(&len, &free, arr + off);
			if (long_enough(off, len, size, align)) {
				/* Remove it. */
				memset(&arr[fa_off], 0, 3);
				goto found;
			}
		}
	}

again:
	off = arrsize;

	decode(&len, &free, arr + off);
	while (!free || !long_enough(off, len, size, align)) {
		/* Refill free array as we go. */
		if (free && coalesced)
			add_to_free_array(arr, poolsize, len, off);

		off += len;
		/* Hit end? */
		if (off == poolsize) {
			if (!coalesced && try_coalesce(pool, poolsize)) {
				coalesced = true;
				goto again;
			}
			return NULL;
		}
		decode(&len, &free, arr + off);
	}

found:
	/* We have a free block.  Since we walk from front, take far end. */
	actual = ((off + len - size) & ~(align - 1));
	hdr = actual - encode_len_with_header(off + len - actual);


	/* Do we have enough room to split? */
	if (hdr - off >= MIN_BLOCK_SIZE) {
		encode(hdr - off, true, arr + off);
		add_to_free_array(arr, poolsize, hdr - off, off);
	} else {
		hdr = off;
	}

	/* Make sure that we are all-zero up to actual, so we can walk back
	 * and find header. */
	memset(arr + hdr, 0, actual - hdr);

	/* Create header for allocated block. */
	encode(off + len - hdr, false, arr + hdr);

	return arr + actual;
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
	unsigned long len;
	unsigned char *arr = pool;
	unsigned char *hdr;
	bool free;

	/* Too small to do anything. */
	if (poolsize < MIN_BLOCK_SIZE)
		return;

	hdr = to_hdr(freep);

	decode(&len, &free, hdr);
	assert(!free);
	hdr[0] |= FREE_BIT;

	/* If an empty slot, put this in free array. */
	add_to_free_array(pool, poolsize, len, hdr - arr);
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

static bool check_decode(const unsigned char *arr, unsigned long len)
{
	unsigned int i;

	if (len == 0)
		return tiny_check_fail();
	if (!(arr[0] & TERM_BIT))
		return tiny_check_fail();
	if (arr[0] & SINGLE_BYTE)
		return true;
	for (i = 1; i < len; i++) {
		if (arr[i] & TERM_BIT)
			return true;
	}
	return tiny_check_fail();
}

bool tiny_alloc_check(void *pool, unsigned long poolsize)
{
	unsigned long arrsize = free_array_size(poolsize);
	unsigned char *arr = pool;
	unsigned long len, off, hdrlen;
	unsigned long i, freearr[arrsize], num_freearr = 0;
	bool free;

	if (poolsize < MIN_BLOCK_SIZE)
		return true;

	for (i = 0; i + 3 < free_array_size(poolsize); i += 3) {
		off = ((unsigned long)arr[i]) << 16
			| ((unsigned long)arr[i+1]) << 8
			| ((unsigned long)arr[i+2]);
		if (!off)
			continue;

		if (off >= poolsize)
			return tiny_check_fail();
		freearr[num_freearr++] = off;
	}

	for (off = arrsize; off < poolsize; off += len) {
		/* We should have a valid header. */
		if (!check_decode(arr + off, poolsize - off))
			return false;
		hdrlen = decode(&len, &free, arr + off);
		if (off + len > poolsize)
			return tiny_check_fail();
		if (hdrlen != encode_length(len - MIN_BLOCK_SIZE))
			return tiny_check_fail();
		for (i = 0; i < num_freearr; i++) {
			if (freearr[i] == off) {
				if (!free)
					return tiny_check_fail();
				memmove(&freearr[i], &freearr[i+1],
					(num_freearr-- - (i + 1))
					* sizeof(freearr[i]));
				break;
			}
		}
	}

	/* Now we should have found everything in freearr. */
	if (num_freearr)
		return tiny_check_fail();

	/* Now check that sizes are correct. */
	for (i = 0; i + 3 < free_array_size(poolsize); i += 3) {
		unsigned long fa_off;

		off = ((unsigned long)arr[i]) << 16
			| ((unsigned long)arr[i+1]) << 8
			| ((unsigned long)arr[i+2]);
		if (!off)
			continue;

		decode(&len, &free, arr + off);

		/* Would we expect to find this length in this bucket? */
		if (len >= MAX_FREE_CACHED_SIZE)
			return tiny_check_fail();

		for (fa_off = free_array_off(len);
		     fa_off + 3 < free_array_size(poolsize);
		     fa_off += free_array_off(MAX_FREE_CACHED_SIZE)) {
			if (fa_off == i)
				break;
		}
		if (fa_off != i)
			return tiny_check_fail();
	}

	return true;
}

/* FIXME: Implement. */
void tiny_alloc_visualize(FILE *out, void *pool, unsigned long poolsize)
{
}
