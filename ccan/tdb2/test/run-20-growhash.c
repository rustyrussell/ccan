#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/open.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tdb2/check.c>
#include <ccan/tap/tap.h>
#include "logging.h"

static uint64_t myhash(const void *key, size_t len, uint64_t seed, void *priv)
{
	return *(const uint64_t *)key;
}

static void add_bits(uint64_t *val, unsigned new, unsigned new_bits,
		     unsigned *done)
{
	*done += new_bits;
	*val |= ((uint64_t)new << (64 - *done));
}

static uint64_t make_key(unsigned topgroup, unsigned topbucket,
			 unsigned subgroup1, unsigned subbucket1,
			 unsigned subgroup2, unsigned subbucket2)
{
	uint64_t key = 0;
	unsigned done = 0;

	add_bits(&key, topgroup, TDB_TOPLEVEL_HASH_BITS - TDB_HASH_GROUP_BITS,
		 &done);
	add_bits(&key, topbucket, TDB_HASH_GROUP_BITS, &done);
	add_bits(&key, subgroup1, TDB_SUBLEVEL_HASH_BITS - TDB_HASH_GROUP_BITS,
		 &done);
	add_bits(&key, subbucket1, TDB_HASH_GROUP_BITS, &done);
	add_bits(&key, subgroup2, TDB_SUBLEVEL_HASH_BITS - TDB_HASH_GROUP_BITS,
		 &done);
	add_bits(&key, subbucket2, TDB_HASH_GROUP_BITS, &done);
	return key;
}

int main(int argc, char *argv[])
{
	unsigned int i, j;
	struct tdb_context *tdb;
	uint64_t kdata;
	struct tdb_used_record rec;
	struct tdb_data key = { (unsigned char *)&kdata, sizeof(kdata) };
	struct tdb_data dbuf = { (unsigned char *)&kdata, sizeof(kdata) };
	union tdb_attribute hattr = { .hash = { .base = { TDB_ATTRIBUTE_HASH },
						.fn = myhash } };
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT,
			TDB_NOMMAP|TDB_CONVERT,
	};

	hattr.base.next = &tap_log_attr;

	plan_tests(sizeof(flags) / sizeof(flags[0])
		   * (9 + (20 + 2 * ((1 << TDB_HASH_GROUP_BITS) - 2))
		      * (1 << TDB_HASH_GROUP_BITS)) + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		struct hash_info h;

		tdb = tdb_open("run-04-basichash.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &hattr);
		ok1(tdb);
		if (!tdb)
			continue;

		/* Fill a group. */
		for (j = 0; j < (1 << TDB_HASH_GROUP_BITS); j++) {
			kdata = make_key(0, j, 0, 0, 0, 0);
			ok1(tdb_store(tdb, key, dbuf, TDB_INSERT) == 0);
		}
		ok1(tdb_check(tdb, NULL, NULL) == 0);

		/* Check first still exists. */
		kdata = make_key(0, 0, 0, 0, 0, 0);
		ok1(find_and_lock(tdb, key, F_RDLCK, &h, &rec, NULL) != 0);
		/* Should have created correct hash. */
		ok1(h.h == tdb_hash(tdb, key.dptr, key.dsize));
		/* Should have located space in group 0, bucket 0. */
		ok1(h.group_start == offsetof(struct tdb_header, hashtable));
		ok1(h.home_bucket == 0);
		ok1(h.found_bucket == 0);
		ok1(h.hash_used == TDB_TOPLEVEL_HASH_BITS);
		/* Entire group should be full! */
		for (j = 0; j < (1 << TDB_HASH_GROUP_BITS); j++)
			ok1(h.group[j] != 0);

		ok1(tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range,
				      F_RDLCK) == 0);

		/* Now, add one more to each should expand (that) bucket. */
		for (j = 0; j < (1 << TDB_HASH_GROUP_BITS); j++) {
			unsigned int k;
			kdata = make_key(0, j, 0, 1, 0, 0);
			ok1(tdb_store(tdb, key, dbuf, TDB_INSERT) == 0);
			ok1(tdb_check(tdb, NULL, NULL) == 0);

			ok1(find_and_lock(tdb, key, F_RDLCK, &h, &rec, NULL));
			/* Should have created correct hash. */
			ok1(h.h == tdb_hash(tdb, key.dptr, key.dsize));
			/* Should have moved to subhash */
			ok1(h.group_start >= sizeof(struct tdb_header));
			ok1(h.home_bucket == 1);
			ok1(h.found_bucket == 1);
			ok1(h.hash_used == TDB_TOPLEVEL_HASH_BITS
			    + TDB_SUBLEVEL_HASH_BITS);
			ok1(tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range,
					      F_RDLCK) == 0);

			/* Keep adding, make it expand again. */
			for (k = 2; k < (1 << TDB_HASH_GROUP_BITS); k++) {
				kdata = make_key(0, j, 0, k, 0, 0);
				ok1(tdb_store(tdb, key, dbuf, TDB_INSERT) == 0);
				ok1(tdb_check(tdb, NULL, NULL) == 0);
			}

			/* This should tip it over to sub-sub-hash. */
			kdata = make_key(0, j, 0, 0, 0, 1);
			ok1(tdb_store(tdb, key, dbuf, TDB_INSERT) == 0);
			ok1(tdb_check(tdb, NULL, NULL) == 0);

			ok1(find_and_lock(tdb, key, F_RDLCK, &h, &rec, NULL));
			/* Should have created correct hash. */
			ok1(h.h == tdb_hash(tdb, key.dptr, key.dsize));
			/* Should have moved to subhash */
			ok1(h.group_start >= sizeof(struct tdb_header));
			ok1(h.home_bucket == 1);
			ok1(h.found_bucket == 1);
			ok1(h.hash_used == TDB_TOPLEVEL_HASH_BITS
			    + TDB_SUBLEVEL_HASH_BITS + TDB_SUBLEVEL_HASH_BITS);
			ok1(tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range,
					      F_RDLCK) == 0);
		}
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
