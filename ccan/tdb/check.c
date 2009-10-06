 /* 
   Unix SMB/CIFS implementation.

   trivial database library

   Copyright (C) Rusty Russell		   2009
   
     ** NOTE! The following LGPL license applies to the tdb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#include "tdb_private.h"
#include <limits.h>

/*
  For each value, we flip F bits in a bitmap of size 2^B.  So we can think
  of this as a F*B bit hash (this isn't quite true due to hash collisions,
  but it seems good enough for F << B).

  Assume that we only have a single error; this is *not* the birthday
  problem, since the question is: "does that error hash to the same as
  the correct value", ie. a simple 1 in 2^F*B.  The chances of detecting
  multiple errors is even higher (since we only need to detect one of
  them).

  Given that ldb uses a hash size of 10000, using 512 bytes per hash chain
  (5M) seems reasonable.  With 128 hashes, that's about 1 in a million chance
  of missing a single linked list error.
*/
#define NUM_HASHES 128
#define BITMAP_BITS (512 * CHAR_BIT)

/* We use the excellent Jenkins lookup3 hash; this is based on hash_word2.
 * See: http://burtleburtle.net/bob/c/lookup3.c
 */
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c,4);  \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}

static void hash(uint32_t key, uint32_t *pc, uint32_t *pb)
{
	uint32_t a,b,c;

	/* Set up the internal state */
	a = b = c = 0xdeadbeef + *pc;
	c += *pb;

	a += key;
	final(a,b,c);
	*pc=c; *pb=b;
}

static void bit_flip(unsigned char bits[], unsigned int idx)
{
	bits[idx / CHAR_BIT] ^= (1 << (idx % CHAR_BIT));
}

static void add_to_hash(unsigned char bits[], tdb_off_t off)
{
	uint32_t h1 = off, h2 = 0;
	unsigned int i;
	for (i = 0; i < NUM_HASHES / 2; i++) {
		hash(off, &h1, &h2);
		bit_flip(bits, h1 % BITMAP_BITS);
		bit_flip(bits, h2 % BITMAP_BITS);
		h2++;
	}
}

/* Since we opened it, these shouldn't fail unless it's recent corruption. */
static bool tdb_check_header(struct tdb_context *tdb, tdb_off_t *recovery)
{
	struct tdb_header hdr;

	if (tdb->methods->tdb_read(tdb, 0, &hdr, sizeof(hdr), DOCONV()) == -1)
		return false;
	if (strcmp(hdr.magic_food, TDB_MAGIC_FOOD) != 0)
		goto corrupt;

	CONVERT(hdr);
	if (hdr.version != TDB_VERSION)
		goto corrupt;

	if (hdr.rwlocks != 0)
		goto corrupt;

	if (hdr.hash_size == 0)
		goto corrupt;

	if (hdr.hash_size != tdb->header.hash_size)
		goto corrupt;

	if (hdr.recovery_start != 0 &&
	    hdr.recovery_start < TDB_DATA_START(tdb->header.hash_size))
		goto corrupt;

	*recovery = hdr.recovery_start;
	return true;

corrupt:
	tdb->ecode = TDB_ERR_CORRUPT;
	return false;
}

static bool tdb_check_record(struct tdb_context *tdb,
			     tdb_off_t off,
			     const struct list_struct *rec)
{
	tdb_off_t tailer;

	/* Check rec->next: 0 or points to record offset, aligned. */
	if (rec->next > 0 && rec->next < TDB_DATA_START(tdb->header.hash_size))
		goto corrupt;
	if (rec->next + sizeof(*rec) < rec->next)
		goto corrupt;
	if ((rec->next % TDB_ALIGNMENT) != 0)
		goto corrupt;
	if (tdb->methods->tdb_oob(tdb, rec->next+sizeof(*rec), 1))
		goto corrupt;

	/* Check rec_len: similar to rec->next, implies next record. */
	if ((rec->rec_len % TDB_ALIGNMENT) != 0)
		goto corrupt;
	/* Must fit tailer. */
	if (rec->rec_len < sizeof(tailer))
		goto corrupt;
	/* OOB allows "right at the end" access, so this works for last rec. */
	if (tdb->methods->tdb_oob(tdb, off+sizeof(*rec)+rec->rec_len, 1))
		goto corrupt;

	/* Check tailer. */
	if (tdb_ofs_read(tdb, off+sizeof(*rec)+rec->rec_len-sizeof(tailer),
			 &tailer) == -1)
		goto corrupt;
	if (tailer != sizeof(*rec) + rec->rec_len)
		goto corrupt;

	return true;

corrupt:
	tdb->ecode = TDB_ERR_CORRUPT;
	return false;
}

static TDB_DATA get_data(struct tdb_context *tdb, tdb_off_t off, tdb_len_t len)
{
	TDB_DATA d;

	d.dsize = len;

	/* We've already done bounds check here. */
	if (tdb->transaction == NULL && tdb->map_ptr != NULL)
		d.dptr = (unsigned char *)tdb->map_ptr + off;
	else
		d.dptr = tdb_alloc_read(tdb, off, d.dsize);
	return d;
}

static void put_data(struct tdb_context *tdb, TDB_DATA d)
{
	if (tdb->transaction == NULL && tdb->map_ptr != NULL)
		return;
	free(d.dptr);
}

static bool tdb_check_used_record(struct tdb_context *tdb,
				  tdb_off_t off,
				  const struct list_struct *rec,
				  unsigned char **hashes,
				  int (*check)(TDB_DATA, TDB_DATA, void *),
				  void *private)
{
	TDB_DATA key, data;

	if (!tdb_check_record(tdb, off, rec))
		return false;

	/* key + data + tailer must fit in record */
	if (rec->key_len + rec->data_len + sizeof(tdb_off_t) > rec->rec_len)
		return false;

	key = get_data(tdb, off + sizeof(*rec), rec->key_len);
	if (!key.dptr)
		return false;

	if (tdb->hash_fn(&key) != rec->full_hash)
		goto fail_put_key;

	add_to_hash(hashes[BUCKET(rec->full_hash)+1], off);
	if (rec->next)
		add_to_hash(hashes[BUCKET(rec->full_hash)+1], rec->next);

	/* If they supply a check function, get data. */
	if (check) {
		data = get_data(tdb, off + sizeof(*rec) + rec->key_len,
				rec->data_len);
		if (!data.dptr)
			goto fail_put_key;

		if (check(key, data, private) == -1)
			goto fail_put_data;
		put_data(tdb, data);
	}

	put_data(tdb, key);
	return true;

fail_put_data:
	put_data(tdb, data);
fail_put_key:
	put_data(tdb, key);
	return false;
}

static bool tdb_check_free_record(struct tdb_context *tdb,
				  tdb_off_t off,
				  const struct list_struct *rec,
				  unsigned char **hashes)
{
	if (!tdb_check_record(tdb, off, rec))
		return false;

	add_to_hash(hashes[0], off);
	if (rec->next)
		add_to_hash(hashes[0], rec->next);
	return true;
}

/* We do this via linear scan, even though it's not 100% accurate. */
int tdb_check(struct tdb_context *tdb,
	      int (*check)(TDB_DATA key, TDB_DATA data, void *private),
	      void *private)
{
	unsigned int h;
	unsigned char **hashes;
	tdb_off_t off, recovery_start;
	struct list_struct rec;
	bool found_recovery = false;

	if (tdb_lockall(tdb) == -1)
		return -1;

	/* Make sure we know true size of the underlying file. */
	tdb->methods->tdb_oob(tdb, tdb->map_size + 1, 1);

	if (!tdb_check_header(tdb, &recovery_start))
		goto unlock;

	if (tdb->map_size < TDB_DATA_START(tdb->header.hash_size)) {
		tdb->ecode = TDB_ERR_CORRUPT;
		goto unlock;
	}

	/* One big malloc: pointers then bit arrays. */
	hashes = calloc(1, sizeof(hashes[0]) * (1+tdb->header.hash_size)
			+ BITMAP_BITS / CHAR_BIT * (1+tdb->header.hash_size));
	if (!hashes) {
		tdb->ecode = TDB_ERR_OOM;
		goto unlock;
	}

	/* Initialize pointers */
	hashes[0] = (unsigned char *)(&hashes[1+tdb->header.hash_size]);
	for (h = 1; h < 1+tdb->header.hash_size; h++)
		hashes[h] = hashes[h-1] + BITMAP_BITS / CHAR_BIT;

	/* Freelist and hash headers are all in a row. */
	for (h = 0; h < 1+tdb->header.hash_size; h++) {
		if (tdb_ofs_read(tdb, FREELIST_TOP + h*sizeof(tdb_off_t),
				 &off) == -1)
			goto free;
		if (off)
			add_to_hash(hashes[h], off);
	}

	for (off = TDB_DATA_START(tdb->header.hash_size);
	     off < tdb->map_size;
	     off += sizeof(rec) + rec.rec_len) {
		if (tdb->methods->tdb_read(tdb, off, &rec, sizeof(rec),
					   DOCONV()) == -1)
			goto free;
		switch (rec.magic) {
		case TDB_MAGIC:
		case TDB_DEAD_MAGIC:
			if (!tdb_check_used_record(tdb, off, &rec, hashes,
						   check, private))
				goto free;
			break;
		case TDB_FREE_MAGIC:
			if (!tdb_check_free_record(tdb, off, &rec, hashes))
				goto free;
			break;
		case TDB_RECOVERY_MAGIC:
			if (recovery_start != off)
				goto free;
			found_recovery = true;
			break;
		default:
			tdb->ecode = TDB_ERR_CORRUPT;
			goto free;
		}
	}

	/* Now, hashes should all be empty: each record exists and is referred
	 * to by one other. */
	for (h = 0; h < 1+tdb->header.hash_size; h++) {
		unsigned int i;
		for (i = 0; i < BITMAP_BITS / CHAR_BIT; i++) {
			if (hashes[h][i] != 0) {
				tdb->ecode = TDB_ERR_CORRUPT;
				goto free;
			}
		}
	}

	/* We must have found recovery area. */
	if (recovery_start != 0 && !found_recovery)
		goto free;

	free(hashes);
	tdb_unlockall(tdb);
	return 0;

free:
	free(hashes);
unlock:
	tdb_unlockall(tdb);
	return -1;
}

		

	

	
