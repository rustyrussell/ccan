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
	encode(poolsize - arrsize, true, (unsigned char *)pool + arrsize);
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
			encode(prev_len + len, true, pool + prev_off);
			coalesced = true;
		}
		prev_free = free;
		prev_off = off;
		prev_len = len;
		off += len;
	} while (off < poolsize);

	/* FIXME: Refill free_array here. */
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

static unsigned long find_free_end(unsigned char *arr, unsigned long arrsize)
{
	long i;

	for (i = arrsize-1; i >= 0; i--) {
		if (arr[i])
			return i + 1;
	}
	return 0;
}

void *tiny_alloc_get(void *pool, unsigned long poolsize,
		     unsigned long size, unsigned long align)
{
	unsigned long arrsize = free_array_size(poolsize);
	unsigned long len, off, fa_off, fa_hdrlen, actual, hdr, hdrlen, freelen;
	unsigned char *arr = pool;
	bool free;

	/* We can't do anything with tiny pools. */
	if (poolsize < MIN_BLOCK_SIZE)
		return NULL;

	/* We don't do zero-allocs; allows 1 more offset in encoding. */
	if (!size)
		size = 1;

	/* Look for entries in free array. */
	freelen = find_free_end(pool, arrsize);
	for (fa_off = 0; fa_off < freelen; fa_off += fa_hdrlen) {
		fa_hdrlen = decode(&off, &free, arr + fa_off);
		off -= MIN_BLOCK_SIZE;
		hdrlen = decode(&len, &free, arr + off);
		if (long_enough(off, len, size, align)) {
			/* Move every successive entry down. */
			memmove(arr + fa_off, arr + fa_off + fa_hdrlen,
				freelen - fa_hdrlen);
			memset(arr + freelen - fa_hdrlen, 0, fa_hdrlen);
			goto found;
		}
	}

again:
	off = arrsize;

	hdrlen = decode(&len, &free, arr + off);
	while (!free || !long_enough(off, len, size, align)) {
		/* FIXME: Refill free array if this block is free. */

		/* Hit end? */
		off += len;
		if (off == poolsize) {
			if (try_coalesce(pool, poolsize))
				goto again;
			return NULL;
		}
		hdrlen = decode(&len, &free, arr + off);
	}

found:
	/* We have a free block.  Since we walk from front, take far end. */
	actual = ((off + len - size) & ~(align - 1));
	hdr = actual - encode_len_with_header(off + len - actual);

	/* Do we have enough room to split? */
	if (hdr - off >= MIN_BLOCK_SIZE) {
		encode(hdr - off, true, arr + off);
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
	unsigned long len, end, arrsize = free_array_size(poolsize);
	unsigned char *arr = pool;
	unsigned char *hdr;

	/* Too small to do anything. */
	if (poolsize < MIN_BLOCK_SIZE)
		return;

	hdr = to_hdr(freep);
	hdr[0] |= FREE_BIT;

	end = find_free_end(pool, arrsize);

	/* If we can fit this block, encode it. */
	len = encode_length(hdr - arr);
	if (end + len <= arrsize)
		encode(hdr - arr + MIN_BLOCK_SIZE, true, arr + end);
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
	unsigned long len, off, off2, hdrlen, end;
	unsigned long i, freearr[arrsize], num_freearr = 0;
	bool free;

	if (poolsize < MIN_BLOCK_SIZE)
		return true;

	end = find_free_end(pool, arrsize);
	for (off = 0; off < end; off += hdrlen) {
		if (!check_decode(arr + off, end - off))
			return false;
		hdrlen = decode(&off2, &free, arr + off);
		off2 -= MIN_BLOCK_SIZE;
		if (off2 >= poolsize)
			return tiny_check_fail();
		if (!free)
			return tiny_check_fail();
		freearr[num_freearr++] = off2;
	}
	/* Rest of free array should be all zeroes. */
	for (off = end; off < arrsize; off++) {
		if (arr[off] != 0)
			return tiny_check_fail();
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
		
	return true;
}

/* FIXME: Implement. */
void tiny_alloc_visualize(FILE *out, void *pool, unsigned long poolsize)
{
}
