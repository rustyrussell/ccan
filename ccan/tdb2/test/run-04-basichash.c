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

/* We rig the hash so adjacent-numbered records always clash. */
static uint64_t clash(const void *key, size_t len, uint64_t seed, void *priv)
{
	return ((uint64_t)*(const unsigned int *)key)
		<< (64 - TDB_TOPLEVEL_HASH_BITS - 1);
}

int main(int argc, char *argv[])
{
	unsigned int i, j;
	struct tdb_context *tdb;
	unsigned int v;
	struct tdb_used_record rec;
	struct tdb_data key = { (unsigned char *)&v, sizeof(v) };
	struct tdb_data dbuf = { (unsigned char *)&v, sizeof(v) };
	union tdb_attribute hattr = { .hash = { .base = { TDB_ATTRIBUTE_HASH },
						.fn = clash } };
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT,
			TDB_NOMMAP|TDB_CONVERT,
	};

	hattr.base.next = &tap_log_attr;

	plan_tests(sizeof(flags) / sizeof(flags[0])
		   * (91 + (2 * ((1 << TDB_HASH_GROUP_BITS) - 1))) + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		struct hash_info h;
		tdb_off_t new_off, off, subhash;

		tdb = tdb_open("run-04-basichash.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &hattr);
		ok1(tdb);
		if (!tdb)
			continue;

		v = 0;
		/* Should not find it. */
		ok1(find_and_lock(tdb, key, F_WRLCK, &h, &rec, NULL) == 0);
		/* Should have created correct hash. */
		ok1(h.h == tdb_hash(tdb, key.dptr, key.dsize));
		/* Should have located space in group 0, bucket 0. */
		ok1(h.group_start == offsetof(struct tdb_header, hashtable));
		ok1(h.home_bucket == 0);
		ok1(h.found_bucket == 0);
		ok1(h.hash_used == TDB_TOPLEVEL_HASH_BITS);

		/* Should have lock on bucket 0 */
		ok1(h.hlock_start == 0);
		ok1(h.hlock_range == 
		    1ULL << (64-(TDB_TOPLEVEL_HASH_BITS-TDB_HASH_GROUP_BITS)));
		ok1((tdb->flags & TDB_NOLOCK) || tdb->file->num_lockrecs == 1);
		ok1((tdb->flags & TDB_NOLOCK)
		    || tdb->file->lockrecs[0].off == TDB_HASH_LOCK_START);
		/* FIXME: Check lock length */

		/* Allocate a new record. */
		new_off = alloc(tdb, key.dsize, dbuf.dsize, h.h,
				TDB_USED_MAGIC, false);
		ok1(!TDB_OFF_IS_ERR(new_off));

		/* We should be able to add it now. */
		ok1(add_to_hash(tdb, &h, new_off) == 0);

		/* Make sure we fill it in for later finding. */
		off = new_off + sizeof(struct tdb_used_record);
		ok1(!tdb->methods->twrite(tdb, off, key.dptr, key.dsize));
		off += key.dsize;
		ok1(!tdb->methods->twrite(tdb, off, dbuf.dptr, dbuf.dsize));

		/* We should be able to unlock that OK. */
		ok1(tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range,
				      F_WRLCK) == 0);

		/* Database should be consistent. */
		ok1(tdb_check(tdb, NULL, NULL) == 0);

		/* Now, this should give a successful lookup. */
		ok1(find_and_lock(tdb, key, F_WRLCK, &h, &rec, NULL)
		    == new_off);
		/* Should have created correct hash. */
		ok1(h.h == tdb_hash(tdb, key.dptr, key.dsize));
		/* Should have located space in group 0, bucket 0. */
		ok1(h.group_start == offsetof(struct tdb_header, hashtable));
		ok1(h.home_bucket == 0);
		ok1(h.found_bucket == 0);
		ok1(h.hash_used == TDB_TOPLEVEL_HASH_BITS);

		/* Should have lock on bucket 0 */
		ok1(h.hlock_start == 0);
		ok1(h.hlock_range == 
		    1ULL << (64-(TDB_TOPLEVEL_HASH_BITS-TDB_HASH_GROUP_BITS)));
		ok1((tdb->flags & TDB_NOLOCK) || tdb->file->num_lockrecs == 1);
		ok1((tdb->flags & TDB_NOLOCK)
		    || tdb->file->lockrecs[0].off == TDB_HASH_LOCK_START);
		/* FIXME: Check lock length */

		ok1(tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range,
				      F_WRLCK) == 0);
		
		/* Database should be consistent. */
		ok1(tdb_check(tdb, NULL, NULL) == 0);

		/* Test expansion. */
		v = 1;
		ok1(find_and_lock(tdb, key, F_WRLCK, &h, &rec, NULL) == 0);
		/* Should have created correct hash. */
		ok1(h.h == tdb_hash(tdb, key.dptr, key.dsize));
		/* Should have located space in group 0, bucket 1. */
		ok1(h.group_start == offsetof(struct tdb_header, hashtable));
		ok1(h.home_bucket == 0);
		ok1(h.found_bucket == 1);
		ok1(h.hash_used == TDB_TOPLEVEL_HASH_BITS);

		/* Should have lock on bucket 0 */
		ok1(h.hlock_start == 0);
		ok1(h.hlock_range == 
		    1ULL << (64-(TDB_TOPLEVEL_HASH_BITS-TDB_HASH_GROUP_BITS)));
		ok1((tdb->flags & TDB_NOLOCK) || tdb->file->num_lockrecs == 1);
		ok1((tdb->flags & TDB_NOLOCK)
		    || tdb->file->lockrecs[0].off == TDB_HASH_LOCK_START);
		/* FIXME: Check lock length */

		/* Make it expand 0'th bucket. */
		ok1(expand_group(tdb, &h) == 0);
		/* First one should be subhash, next should be empty. */
		ok1(is_subhash(h.group[0]));
		subhash = (h.group[0] & TDB_OFF_MASK);
		for (j = 1; j < (1 << TDB_HASH_GROUP_BITS); j++)
			ok1(h.group[j] == 0);

		ok1(tdb_write_convert(tdb, h.group_start,
				      h.group, sizeof(h.group)) == 0);
		ok1(tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range,
				      F_WRLCK) == 0);

		/* Should be happy with expansion. */
		ok1(tdb_check(tdb, NULL, NULL) == 0);

		/* Should be able to find it. */
		v = 0;
		ok1(find_and_lock(tdb, key, F_WRLCK, &h, &rec, NULL)
		    == new_off);
		/* Should have created correct hash. */
		ok1(h.h == tdb_hash(tdb, key.dptr, key.dsize));
		/* Should have located space in expanded group 0, bucket 0. */
		ok1(h.group_start == subhash + sizeof(struct tdb_used_record));
		ok1(h.home_bucket == 0);
		ok1(h.found_bucket == 0);
		ok1(h.hash_used == TDB_TOPLEVEL_HASH_BITS
		    + TDB_SUBLEVEL_HASH_BITS);

		/* Should have lock on bucket 0 */
		ok1(h.hlock_start == 0);
		ok1(h.hlock_range == 
		    1ULL << (64-(TDB_TOPLEVEL_HASH_BITS-TDB_HASH_GROUP_BITS)));
		ok1((tdb->flags & TDB_NOLOCK) || tdb->file->num_lockrecs == 1);
		ok1((tdb->flags & TDB_NOLOCK)
		    || tdb->file->lockrecs[0].off == TDB_HASH_LOCK_START);
		/* FIXME: Check lock length */

		/* Simple delete should work. */
		ok1(delete_from_hash(tdb, &h) == 0);
		ok1(add_free_record(tdb, new_off,
				    sizeof(struct tdb_used_record)
				    + rec_key_length(&rec)
				    + rec_data_length(&rec)
				    + rec_extra_padding(&rec)) == 0);
		ok1(tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range,
				      F_WRLCK) == 0);
		ok1(tdb_check(tdb, NULL, NULL) == 0);

		/* Test second-level expansion: should expand 0th bucket. */
		v = 0;
		ok1(find_and_lock(tdb, key, F_WRLCK, &h, &rec, NULL) == 0);
		/* Should have created correct hash. */
		ok1(h.h == tdb_hash(tdb, key.dptr, key.dsize));
		/* Should have located space in group 0, bucket 0. */
		ok1(h.group_start == subhash + sizeof(struct tdb_used_record));
		ok1(h.home_bucket == 0);
		ok1(h.found_bucket == 0);
		ok1(h.hash_used == TDB_TOPLEVEL_HASH_BITS+TDB_SUBLEVEL_HASH_BITS);

		/* Should have lock on bucket 0 */
		ok1(h.hlock_start == 0);
		ok1(h.hlock_range == 
		    1ULL << (64-(TDB_TOPLEVEL_HASH_BITS-TDB_HASH_GROUP_BITS)));
		ok1((tdb->flags & TDB_NOLOCK) || tdb->file->num_lockrecs == 1);
		ok1((tdb->flags & TDB_NOLOCK)
		    || tdb->file->lockrecs[0].off == TDB_HASH_LOCK_START);
		/* FIXME: Check lock length */

		ok1(expand_group(tdb, &h) == 0);
		/* First one should be subhash, next should be empty. */
		ok1(is_subhash(h.group[0]));
		subhash = (h.group[0] & TDB_OFF_MASK);
		for (j = 1; j < (1 << TDB_HASH_GROUP_BITS); j++)
			ok1(h.group[j] == 0);
		ok1(tdb_write_convert(tdb, h.group_start,
				      h.group, sizeof(h.group)) == 0);
		ok1(tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range,
				      F_WRLCK) == 0);

		/* Should be happy with expansion. */
		ok1(tdb_check(tdb, NULL, NULL) == 0);

		ok1(find_and_lock(tdb, key, F_WRLCK, &h, &rec, NULL) == 0);
		/* Should have created correct hash. */
		ok1(h.h == tdb_hash(tdb, key.dptr, key.dsize));
		/* Should have located space in group 0, bucket 0. */
		ok1(h.group_start == subhash + sizeof(struct tdb_used_record));
		ok1(h.home_bucket == 0);
		ok1(h.found_bucket == 0);
		ok1(h.hash_used == TDB_TOPLEVEL_HASH_BITS
		    + TDB_SUBLEVEL_HASH_BITS * 2);

		/* We should be able to add it now. */
		/* Allocate a new record. */
		new_off = alloc(tdb, key.dsize, dbuf.dsize, h.h,
				TDB_USED_MAGIC, false);
		ok1(!TDB_OFF_IS_ERR(new_off));
		ok1(add_to_hash(tdb, &h, new_off) == 0);

		/* Make sure we fill it in for later finding. */
		off = new_off + sizeof(struct tdb_used_record);
		ok1(!tdb->methods->twrite(tdb, off, key.dptr, key.dsize));
		off += key.dsize;
		ok1(!tdb->methods->twrite(tdb, off, dbuf.dptr, dbuf.dsize));

		/* We should be able to unlock that OK. */
		ok1(tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range,
				      F_WRLCK) == 0);

		/* Database should be consistent. */
		ok1(tdb_check(tdb, NULL, NULL) == 0);

		/* Should be able to find it. */
		v = 0;
		ok1(find_and_lock(tdb, key, F_WRLCK, &h, &rec, NULL)
		    == new_off);
		/* Should have created correct hash. */
		ok1(h.h == tdb_hash(tdb, key.dptr, key.dsize));
		/* Should have located space in expanded group 0, bucket 0. */
		ok1(h.group_start == subhash + sizeof(struct tdb_used_record));
		ok1(h.home_bucket == 0);
		ok1(h.found_bucket == 0);
		ok1(h.hash_used == TDB_TOPLEVEL_HASH_BITS
		    + TDB_SUBLEVEL_HASH_BITS * 2);

		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
