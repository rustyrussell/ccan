 /*
   Trivial Database: human-readable summary code
   Copyright (C) Rusty Russell 2010

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
#include "tdb1_private.h"

#define SUMMARY_FORMAT1 \
	"Size of file/data: %u/%zu\n" \
	"Number of records: %zu\n" \
	"Smallest/average/largest keys: %zu/%zu/%zu\n" \
	"Smallest/average/largest data: %zu/%zu/%zu\n" \
	"Smallest/average/largest padding: %zu/%zu/%zu\n" \
	"Number of dead records: %zu\n" \
	"Smallest/average/largest dead records: %zu/%zu/%zu\n" \
	"Number of free records: %zu\n" \
	"Smallest/average/largest free records: %zu/%zu/%zu\n" \
	"Number of hash chains: %zu\n" \
	"Smallest/average/largest hash chains: %zu/%zu/%zu\n" \
	"Number of uncoalesced records: %zu\n" \
	"Smallest/average/largest uncoalesced runs: %zu/%zu/%zu\n" \
	"Percentage keys/data/padding/free/dead/rechdrs&tailers/hashes: %.0f/%.0f/%.0f/%.0f/%.0f/%.0f/%.0f\n"

/* We don't use tally module, to keep upstream happy. */
struct tally {
	size_t min, max, total;
	size_t num;
};

static void tally1_init(struct tally *tally)
{
	tally->total = 0;
	tally->num = 0;
	tally->min = tally->max = 0;
}

static void tally1_add(struct tally *tally, size_t len)
{
	if (tally->num == 0)
		tally->max = tally->min = len;
	else if (len > tally->max)
		tally->max = len;
	else if (len < tally->min)
		tally->min = len;
	tally->num++;
	tally->total += len;
}

static size_t tally1_mean(const struct tally *tally)
{
	if (!tally->num)
		return 0;
	return tally->total / tally->num;
}

static size_t get_hash_length(struct tdb1_context *tdb, unsigned int i)
{
	tdb1_off_t rec_ptr;
	size_t count = 0;

	if (tdb1_ofs_read(tdb, TDB1_HASH_TOP(i), &rec_ptr) == -1)
		return 0;

	/* keep looking until we find the right record */
	while (rec_ptr) {
		struct tdb1_record r;
		++count;
		if (tdb1_rec_read(tdb, rec_ptr, &r) == -1)
			return 0;
		rec_ptr = r.next;
	}
	return count;
}

char *tdb1_summary(struct tdb1_context *tdb)
{
	tdb1_off_t off, rec_off;
	struct tally freet, keys, data, dead, extra, hash, uncoal;
	struct tdb1_record rec;
	char *ret = NULL;
	bool locked;
	size_t len, unc = 0;
	struct tdb1_record recovery;

	/* Read-only databases use no locking at all: it's best-effort.
	 * We may have a write lock already, so skip that case too. */
	if (tdb->read_only || tdb->allrecord_lock.count != 0) {
		locked = false;
	} else {
		if (tdb1_lockall_read(tdb) == -1)
			return NULL;
		locked = true;
	}

	if (tdb1_recovery_area(tdb, tdb->methods, &rec_off, &recovery) != 0) {
		goto unlock;
	}

	tally1_init(&freet);
	tally1_init(&keys);
	tally1_init(&data);
	tally1_init(&dead);
	tally1_init(&extra);
	tally1_init(&hash);
	tally1_init(&uncoal);

	for (off = TDB1_DATA_START(tdb->header.hash_size);
	     off < tdb->map_size - 1;
	     off += sizeof(rec) + rec.rec_len) {
		if (tdb->methods->tdb1_read(tdb, off, &rec, sizeof(rec),
					   TDB1_DOCONV()) == -1)
			goto unlock;
		switch (rec.magic) {
		case TDB1_MAGIC:
			tally1_add(&keys, rec.key_len);
			tally1_add(&data, rec.data_len);
			tally1_add(&extra, rec.rec_len - (rec.key_len
							 + rec.data_len));
			if (unc > 1)
				tally1_add(&uncoal, unc - 1);
			unc = 0;
			break;
		case TDB1_FREE_MAGIC:
			tally1_add(&freet, rec.rec_len);
			unc++;
			break;
		/* If we crash after ftruncate, we can get zeroes or fill. */
		case TDB1_RECOVERY_INVALID_MAGIC:
		case 0x42424242:
			unc++;
			/* If it's a valid recovery, we can trust rec_len. */
			if (off != rec_off) {
				rec.rec_len = tdb1_dead_space(tdb, off)
					- sizeof(rec);
			}
			/* Fall through */
		case TDB1_DEAD_MAGIC:
			tally1_add(&dead, rec.rec_len);
			break;
		default:
			tdb->last_error = tdb_logerr(tdb, TDB_ERR_CORRUPT,
						TDB_LOG_ERROR,
						"Unexpected record magic 0x%x"
						" at offset %d",
						rec.magic, off);
			goto unlock;
		}
	}
	if (unc > 1)
		tally1_add(&uncoal, unc - 1);

	for (off = 0; off < tdb->header.hash_size; off++)
		tally1_add(&hash, get_hash_length(tdb, off));

	/* 20 is max length of a %zu. */
	len = strlen(SUMMARY_FORMAT1) + 35*20 + 1;
	ret = (char *)malloc(len);
	if (!ret)
		goto unlock;

	snprintf(ret, len, SUMMARY_FORMAT1,
		 tdb->map_size, keys.total+data.total,
		 keys.num,
		 keys.min, tally1_mean(&keys), keys.max,
		 data.min, tally1_mean(&data), data.max,
		 extra.min, tally1_mean(&extra), extra.max,
		 dead.num,
		 dead.min, tally1_mean(&dead), dead.max,
		 freet.num,
		 freet.min, tally1_mean(&freet), freet.max,
		 hash.num,
		 hash.min, tally1_mean(&hash), hash.max,
		 uncoal.total,
		 uncoal.min, tally1_mean(&uncoal), uncoal.max,
		 keys.total * 100.0 / tdb->map_size,
		 data.total * 100.0 / tdb->map_size,
		 extra.total * 100.0 / tdb->map_size,
		 freet.total * 100.0 / tdb->map_size,
		 dead.total * 100.0 / tdb->map_size,
		 (keys.num + freet.num + dead.num)
		 * (sizeof(struct tdb1_record) + sizeof(uint32_t))
		 * 100.0 / tdb->map_size,
		 tdb->header.hash_size * sizeof(tdb1_off_t)
		 * 100.0 / tdb->map_size);

unlock:
	if (locked) {
		tdb1_unlockall_read(tdb);
	}
	return ret;
}
