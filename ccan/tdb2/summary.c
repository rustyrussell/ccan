 /* 
   Trivial Database 2: human-readable summary code
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
#include "private.h"
#include <assert.h>
#include <ccan/tally/tally.h>

static void sizes_for_bucket(unsigned bucket, size_t *min, size_t *max)
{
	if (bucket <= 8) {
		*min = *max = TDB_MIN_DATA_LEN + bucket * 8;
	} else if (bucket == 9) {
		/* FIXME: This is twisted; fix size_to_bucket. */
		*min = TDB_MIN_DATA_LEN + (1ULL << (bucket - 3)) + 8;
		*max = TDB_MIN_DATA_LEN + (1ULL << (bucket - 2)) - 8;
	} else {
		*min = TDB_MIN_DATA_LEN + (1ULL << (bucket - 3));
		*max = TDB_MIN_DATA_LEN + (1ULL << (bucket - 2)) - 8;
	}
	assert(size_to_bucket(63, *min) == bucket);
	assert(size_to_bucket(63, *max) == bucket);
	if (bucket > 8)
		assert(size_to_bucket(63, *min - 8) == bucket - 1);
	assert(size_to_bucket(63, *max + 8) == bucket + 1);
}

static int count_hash(struct tdb_context *tdb,
		      tdb_off_t hash_off, unsigned bits)
{
	const tdb_off_t *h;
	unsigned int i, count = 0;

	h = tdb_access_read(tdb, hash_off, sizeof(*h) << bits, true);
	if (!h)
		return -1;
	for (i = 0; i < (1 << bits); i++)
		count += (h[i] != 0);

	tdb_access_release(tdb, h);
	return count;
}

static tdb_len_t summarize_zone(struct tdb_context *tdb, tdb_off_t zone_off,
				struct tally *zones,
				struct tally *hashes,
				struct tally *free,
				struct tally *keys,
				struct tally *data,
				struct tally *extra,
				struct tally *uncoal,
				uint64_t bucketlen[],
				unsigned int *num_buckets)
{
	struct free_zone_header zhdr;
	tdb_off_t off, end;
	tdb_len_t len;
	unsigned int hdrlen;
	tdb_len_t unc = 0;

	if (tdb_read_convert(tdb, zone_off, &zhdr, sizeof(zhdr)) == -1)
		return TDB_OFF_ERR;

	tally_add(zones, 1ULL << zhdr.zone_bits);
	*num_buckets = BUCKETS_FOR_ZONE(zhdr.zone_bits);

	hdrlen = sizeof(zhdr)
		+ (BUCKETS_FOR_ZONE(zhdr.zone_bits) + 1) * sizeof(tdb_off_t);

	end = zone_off + (1ULL << zhdr.zone_bits);
	if (end > tdb->map_size)
		end = tdb->map_size;

	for (off = zone_off + hdrlen; off < end; off += len) {
		union {
			struct tdb_used_record u;
			struct tdb_free_record f;
		} pad, *p;
		p = tdb_get(tdb, off, &pad, sizeof(pad));
		if (!p)
			return TDB_OFF_ERR;
		if (rec_magic(&p->u) != TDB_MAGIC) {
			len = p->f.data_len;
			tally_add(free, len);
			bucketlen[size_to_bucket(frec_zone_bits(&p->f), len)]++;
			len += sizeof(p->u);
			unc++;
		} else {
			if (unc) {
				tally_add(uncoal, unc);
				unc = 0;
			}
			len = sizeof(p->u)
				+ rec_key_length(&p->u)
				+ rec_data_length(&p->u)
				+ rec_extra_padding(&p->u);

			/* FIXME: Use different magic for hashes? */
			if (!rec_key_length(&p->u) && !rec_hash(&p->u)) {
				int count = count_hash(tdb, off + sizeof(p->u),
						       TDB_SUBLEVEL_HASH_BITS);
				if (count == -1)
					return TDB_OFF_ERR;
				tally_add(hashes, count);
			} else {
				tally_add(keys, rec_key_length(&p->u));
				tally_add(data, rec_data_length(&p->u));
			}
			tally_add(extra, rec_extra_padding(&p->u));
		}
	}
	if (unc)
		tally_add(uncoal, unc);
	return 1ULL << zhdr.zone_bits;
}

#define SUMMARY_FORMAT \
	"Size of file/data: %zu/%zu\n" \
	"Number of zones: %zu\n" \
	"Smallest/average/largest zone size: %zu/%zu/%zu\n%s" \
	"Number of records: %zu\n" \
	"Smallest/average/largest keys: %zu/%zu/%zu\n%s" \
	"Smallest/average/largest data: %zu/%zu/%zu\n%s" \
	"Smallest/average/largest padding: %zu/%zu/%zu\n%s" \
	"Number of free records: %zu\n" \
	"Smallest/average/largest free records: %zu/%zu/%zu\n%s" \
	"Number of uncoalesced records: %zu\n" \
	"Smallest/average/largest uncoalesced runs: %zu/%zu/%zu\n%s" \
	"Toplevel hash used: %u of %u\n" \
	"Number of subhashes: %zu\n" \
	"Smallest/average/largest subhash entries: %zu/%zu/%zu\n%s" \
	"Percentage keys/data/padding/free/rechdrs/zonehdrs/hashes: %.0f/%.0f/%.0f/%.0f/%.0f/%.0f/%.0f\n"

#define BUCKET_SUMMARY_FORMAT_A					\
	"Free bucket %zu: total entries %zu.\n"			\
	"Smallest/average/largest length: %zu/%zu/%zu\n%s"
#define BUCKET_SUMMARY_FORMAT_B					\
	"Free bucket %zu-%zu: total entries %zu.\n"		\
	"Smallest/average/largest length: %zu/%zu/%zu\n%s"

#define HISTO_WIDTH 70
#define HISTO_HEIGHT 20

char *tdb_summary(struct tdb_context *tdb, enum tdb_summary_flags flags)
{
	tdb_off_t off;
	tdb_len_t len;
	unsigned int i, num_buckets, max_bucket = 0;
	uint64_t total_buckets = 0;
	struct tally *zones, *hashes, *freet, *keys, *data, *extra, *uncoal,
		*buckets[BUCKETS_FOR_ZONE(63)+1] = { NULL };
	char *zonesg, *hashesg, *freeg, *keysg, *datag, *extrag, *uncoalg,
		*bucketsg[BUCKETS_FOR_ZONE(63)+1] = { NULL };
	char *ret = NULL;

	zonesg = hashesg = freeg = keysg = datag = extrag = uncoalg = NULL;

	if (tdb_allrecord_lock(tdb, F_RDLCK, TDB_LOCK_WAIT, false) != 0)
		return NULL;

	if (tdb_lock_expand(tdb, F_RDLCK) != 0) {
		tdb_allrecord_unlock(tdb, F_RDLCK);
		return NULL;
	}

	/* Start stats off empty. */
	zones = tally_new(HISTO_HEIGHT);
	hashes = tally_new(HISTO_HEIGHT);
	freet = tally_new(HISTO_HEIGHT);
	keys = tally_new(HISTO_HEIGHT);
	data = tally_new(HISTO_HEIGHT);
	extra = tally_new(HISTO_HEIGHT);
	uncoal = tally_new(HISTO_HEIGHT);
	if (!zones || !hashes || !freet || !keys || !data || !extra
	    || !uncoal) {
		tdb->ecode = TDB_ERR_OOM;
		goto unlock;
	}

	for (i = 0; i < sizeof(buckets)/sizeof(buckets[0]); i++) {
		buckets[i] = tally_new(HISTO_HEIGHT);
		if (!buckets[i]) {
			tdb->ecode = TDB_ERR_OOM;
			goto unlock;
		}
	}

	for (off = sizeof(struct tdb_header);
	     off < tdb->map_size - 1;
	     off += len) {
		uint64_t bucketlen[BUCKETS_FOR_ZONE(63)+1] = { 0 };
		len = summarize_zone(tdb, off, zones, hashes, freet, keys,
				     data, extra, uncoal, bucketlen,
				     &num_buckets);
		if (len == TDB_OFF_ERR)
			goto unlock;
		for (i = 0; i < num_buckets; i++)
			tally_add(buckets[i], bucketlen[i]);
		if (num_buckets > max_bucket)
			max_bucket = num_buckets;
		total_buckets += num_buckets;
	}

	if (flags & TDB_SUMMARY_HISTOGRAMS) {
		zonesg = tally_histogram(zones, HISTO_WIDTH, HISTO_HEIGHT);
		hashesg = tally_histogram(hashes, HISTO_WIDTH, HISTO_HEIGHT);
		freeg = tally_histogram(freet, HISTO_WIDTH, HISTO_HEIGHT);
		keysg = tally_histogram(keys, HISTO_WIDTH, HISTO_HEIGHT);
		datag = tally_histogram(data, HISTO_WIDTH, HISTO_HEIGHT);
		extrag = tally_histogram(extra, HISTO_WIDTH, HISTO_HEIGHT);
		uncoalg = tally_histogram(uncoal, HISTO_WIDTH, HISTO_HEIGHT);
		for (i = 0; i < sizeof(buckets)/sizeof(buckets[0]); i++) {
			bucketsg[i] = tally_histogram(buckets[i],
						      HISTO_WIDTH,
						      HISTO_HEIGHT);
		}
	}

	/* 20 is max length of a %llu. */
	len = strlen(SUMMARY_FORMAT) + 33*20 + 1
		+ (zonesg ? strlen(zonesg) : 0)
		+ (hashesg ? strlen(hashesg) : 0)
		+ (freeg ? strlen(freeg) : 0)
		+ (keysg ? strlen(keysg) : 0)
		+ (datag ? strlen(datag) : 0)
		+ (extrag ? strlen(extrag) : 0)
		+ (uncoalg ? strlen(uncoalg) : 0);
	for (i = 0; i < max_bucket; i++) {
		len += strlen(BUCKET_SUMMARY_FORMAT_B) + 6 * 20
		        + (bucketsg[i] ? strlen(bucketsg[i]) : 0);
	}

	ret = malloc(len);
	if (!ret)
		goto unlock;

	len = sprintf(ret, SUMMARY_FORMAT,
		      (size_t)tdb->map_size,
		      tally_num(keys) + tally_num(data),
		      tally_num(zones),
		      tally_min(zones), tally_mean(zones), tally_max(zones),
		      zonesg ? zonesg : "",
		      tally_num(keys),
		      tally_min(keys), tally_mean(keys), tally_max(keys),
		      keysg ? keysg : "",
		      tally_min(data), tally_mean(data), tally_max(data),
		      datag ? datag : "",
		      tally_min(extra), tally_mean(extra), tally_max(extra),
		      extrag ? extrag : "",
		      tally_num(freet),
		      tally_min(freet), tally_mean(freet), tally_max(freet),
		      freeg ? freeg : "",
		      tally_total(uncoal, NULL),
		      tally_min(uncoal), tally_mean(uncoal), tally_max(uncoal),
		      uncoalg ? uncoalg : "",
		      count_hash(tdb, offsetof(struct tdb_header, hashtable),
				 TDB_TOPLEVEL_HASH_BITS),
		      1 << TDB_TOPLEVEL_HASH_BITS,
		      tally_num(hashes),
		      tally_min(hashes), tally_mean(hashes), tally_max(hashes),
		      hashesg ? hashesg : "",
		      tally_total(keys, NULL) * 100.0 / tdb->map_size,
		      tally_total(data, NULL) * 100.0 / tdb->map_size,
		      tally_total(extra, NULL) * 100.0 / tdb->map_size,
		      tally_total(freet, NULL) * 100.0 / tdb->map_size,
		      (tally_num(keys) + tally_num(freet) + tally_num(hashes))
		      * sizeof(struct tdb_used_record) * 100.0 / tdb->map_size,
		      (tally_num(zones) * sizeof(struct free_zone_header)
		       + total_buckets * sizeof(tdb_off_t))
		      * 100.0 / tdb->map_size,
		      (tally_num(hashes)
		       * (sizeof(tdb_off_t) << TDB_SUBLEVEL_HASH_BITS)
		       + (sizeof(tdb_off_t) << TDB_TOPLEVEL_HASH_BITS))
		      * 100.0 / tdb->map_size);

	for (i = 0; i < max_bucket; i++) {
		size_t min, max;
		sizes_for_bucket(i, &min, &max);
		if (min == max) {
			len += sprintf(ret + len, BUCKET_SUMMARY_FORMAT_A,
				       min, tally_total(buckets[i], NULL),
				       tally_min(buckets[i]),
				       tally_mean(buckets[i]),
				       tally_max(buckets[i]),
				       bucketsg[i] ? bucketsg[i] : "");
		} else {
			len += sprintf(ret + len, BUCKET_SUMMARY_FORMAT_B,
				       min, max, tally_total(buckets[i], NULL),
				       tally_min(buckets[i]),
				       tally_mean(buckets[i]),
				       tally_max(buckets[i]),
				       bucketsg[i] ? bucketsg[i] : "");
		}
	}

unlock:
	free(zonesg);
	free(hashesg);
	free(freeg);
	free(keysg);
	free(datag);
	free(extrag);
	free(uncoalg);
	free(zones);
	free(hashes);
	free(freet);
	free(keys);
	free(data);
	free(extra);
	free(uncoal);
	for (i = 0; i < sizeof(buckets)/sizeof(buckets[0]); i++) {
		free(buckets[i]);
		free(bucketsg[i]);
	}

	tdb_allrecord_unlock(tdb, F_RDLCK);
	tdb_unlock_expand(tdb, F_RDLCK);
	return ret;
}
