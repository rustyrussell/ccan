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
#include "tdb_private.h"
#include <ccan/tally/tally.h>

#define SUMMARY_FORMAT \
	"Size of file/data: %u/%zu\n" \
	"Number of records: %zu\n" \
	"Smallest/average/largest keys: %zu/%zu/%zu\n%s" \
	"Smallest/average/largest data: %zu/%zu/%zu\n%s" \
	"Smallest/average/largest padding: %zu/%zu/%zu\n%s" \
	"Number of dead records: %zu\n" \
	"Smallest/average/largest dead records: %zu/%zu/%zu\n%s" \
	"Number of free records: %zu\n" \
	"Smallest/average/largest free records: %zu/%zu/%zu\n%s" \
	"Number of hash chains: %zu\n" \
	"Smallest/average/largest hash chains: %zu/%zu/%zu\n%s" \
	"Number of uncoalesced records: %zu\n" \
	"Smallest/average/largest uncoalesced runs: %zu/%zu/%zu\n%s" \
	"Percentage keys/data/padding/free/dead/rechdrs&tailers/hashes: %.0f/%.0f/%.0f/%.0f/%.0f/%.0f/%.0f\n"

#define HISTO_WIDTH 70
#define HISTO_HEIGHT 20

/* Slow, but should be very rare. */
static size_t dead_space(struct tdb_context *tdb, tdb_off_t off)
{
	size_t len;

	for (len = 0; off + len < tdb->map_size; len++) {
		char c;
		if (tdb->methods->tdb_read(tdb, off, &c, 1, 0))
			return 0;
		if (c != 0 && c != 0x42)
			break;
	}
	return len;
}

static size_t get_hash_length(struct tdb_context *tdb, unsigned int i)
{
	tdb_off_t rec_ptr;
	size_t count = 0;

	if (tdb_ofs_read(tdb, TDB_HASH_TOP(i), &rec_ptr) == -1)
		return 0;

	/* keep looking until we find the right record */
	while (rec_ptr) {
		struct tdb_record r;
		++count;
		if (tdb_rec_read(tdb, rec_ptr, &r) == -1)
			return 0;
		rec_ptr = r.next;
	}
	return count;
}

char *tdb_summary(struct tdb_context *tdb, enum tdb_summary_flags flags)
{
	tdb_off_t off;
	struct tally *freet, *keys, *data, *dead, *extra, *hash, *uncoal;
	char *freeg, *keysg, *datag, *deadg, *extrag, *hashg, *uncoalg;
	struct tdb_record rec;
	char *ret = NULL;
	bool locked;
	size_t len, unc = 0;

	freeg = keysg = datag = deadg = extrag = hashg = uncoalg = NULL;

	/* Read-only databases use no locking at all: it's best-effort.
	 * We may have a write lock already, so skip that case too. */
	if (tdb->read_only || tdb->allrecord_lock.count != 0) {
		locked = false;
	} else {
		if (tdb_lockall_read(tdb) == -1)
			return NULL;
		locked = true;
	}

	freet = tally_new(HISTO_HEIGHT);
	keys = tally_new(HISTO_HEIGHT);
	data = tally_new(HISTO_HEIGHT);
	dead = tally_new(HISTO_HEIGHT);
	extra = tally_new(HISTO_HEIGHT);
	hash = tally_new(HISTO_HEIGHT);
	uncoal = tally_new(HISTO_HEIGHT);
	if (!freet || !keys || !data || !dead || !extra || !hash || !uncoal) {
		tdb->ecode = TDB_ERR_OOM;
		goto unlock;
	}

	for (off = TDB_DATA_START(tdb->header.hash_size);
	     off < tdb->map_size - 1;
	     off += sizeof(rec) + rec.rec_len) {
		if (tdb->methods->tdb_read(tdb, off, &rec, sizeof(rec),
					   DOCONV()) == -1)
			goto unlock;
		switch (rec.magic) {
		case TDB_MAGIC:
			tally_add(keys, rec.key_len);
			tally_add(data, rec.data_len);
			tally_add(extra, rec.rec_len - (rec.key_len
							+ rec.data_len));
			break;
		case TDB_FREE_MAGIC:
			tally_add(freet, rec.rec_len);
			unc++;
			break;
		/* If we crash after ftruncate, we can get zeroes or fill. */
		case TDB_RECOVERY_INVALID_MAGIC:
		case 0x42424242:
			unc++;
			rec.rec_len = dead_space(tdb, off) - sizeof(rec);
			/* Fall through */
		case TDB_DEAD_MAGIC:
			tally_add(dead, rec.rec_len);
			break;
		default:
			TDB_LOG((tdb, TDB_DEBUG_ERROR,
				 "Unexpected record magic 0x%x at offset %d\n",
				 rec.magic, off));
			goto unlock;
		}

		if (unc &&
		    (rec.magic == TDB_MAGIC || rec.magic == TDB_DEAD_MAGIC)) {
			tally_add(uncoal, unc);
			unc = 0;
		}
	}
	if (unc)
		tally_add(uncoal, unc);

	for (off = 0; off < tdb->header.hash_size; off++)
		tally_add(hash, get_hash_length(tdb, off));

	if (flags & TDB_SUMMARY_HISTOGRAMS) {
		freeg = tally_histogram(freet, HISTO_WIDTH, HISTO_HEIGHT);
		keysg = tally_histogram(keys, HISTO_WIDTH, HISTO_HEIGHT);
		datag = tally_histogram(data, HISTO_WIDTH, HISTO_HEIGHT);
		deadg = tally_histogram(dead, HISTO_WIDTH, HISTO_HEIGHT);
		extrag = tally_histogram(extra, HISTO_WIDTH, HISTO_HEIGHT);
		hashg = tally_histogram(hash, HISTO_WIDTH, HISTO_HEIGHT);
		uncoalg = tally_histogram(uncoal, HISTO_WIDTH, HISTO_HEIGHT);
	}

	/* 20 is max length of a %zu. */
	len = strlen(SUMMARY_FORMAT) + 29*20 + 1
		+ (freeg ? strlen(freeg) : 0)
		+ (keysg ? strlen(keysg) : 0)
		+ (datag ? strlen(datag) : 0)
		+ (deadg ? strlen(deadg) : 0)
		+ (extrag ? strlen(extrag) : 0)
		+ (hashg ? strlen(hashg) : 0)
		+ (uncoalg ? strlen(uncoalg) : 0);
	ret = malloc(len);
	if (!ret)
		goto unlock;

	sprintf(ret, SUMMARY_FORMAT,
		tdb->map_size, tally_total(keys, NULL)+tally_total(data, NULL),
		tally_num(keys),
		tally_min(keys), tally_mean(keys), tally_max(keys),
		keysg ? keysg : "",
		tally_min(data), tally_mean(data), tally_max(data),
		datag ? datag : "",
		tally_min(extra), tally_mean(extra), tally_max(extra),
		extrag ? extrag : "",
		tally_num(dead),
		tally_min(dead), tally_mean(dead), tally_max(dead),
		deadg ? deadg : "",
		tally_num(freet),
		tally_min(freet), tally_mean(freet), tally_max(freet),
		freeg ? freeg : "",
		tally_num(hash),
		tally_min(hash), tally_mean(hash), tally_max(hash),
		hashg ? hashg : "",
		tally_total(uncoal, NULL),
		tally_min(uncoal), tally_mean(uncoal), tally_max(uncoal),
		uncoalg ? uncoalg : "",
		tally_total(keys, NULL) * 100.0 / tdb->map_size,
		tally_total(data, NULL) * 100.0 / tdb->map_size,
		tally_total(extra, NULL) * 100.0 / tdb->map_size,
		tally_total(freet, NULL) * 100.0 / tdb->map_size,
		tally_total(dead, NULL) * 100.0 / tdb->map_size,
		(tally_num(keys) + tally_num(freet) + tally_num(dead))
		* (sizeof(struct tdb_record) + sizeof(uint32_t))
		* 100.0 / tdb->map_size,
		tdb->header.hash_size * sizeof(tdb_off_t)
		* 100.0 / tdb->map_size);

unlock:
	free(freeg);
	free(keysg);
	free(datag);
	free(deadg);
	free(extrag);
	free(hashg);
	free(uncoalg);
	free(freet);
	free(keys);
	free(data);
	free(dead);
	free(extra);
	free(hash);
	free(uncoal);
	if (locked) {
		tdb_unlockall_read(tdb);
	}
	return ret;
}
