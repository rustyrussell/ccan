#include "ntdb-source.h"
#include "tap-interface.h"
#include "logging.h"

/* We rig the hash so all records clash. */
static uint32_t clash(const void *key, size_t len, uint32_t seed, void *priv)
{
	return *((const unsigned int *)key) << 20;
}

int main(int argc, char *argv[])
{
	unsigned int i;
	struct ntdb_context *ntdb;
	unsigned int v;
	struct ntdb_used_record rec;
	NTDB_DATA key = { (unsigned char *)&v, sizeof(v) };
	NTDB_DATA dbuf = { (unsigned char *)&v, sizeof(v) };
	union ntdb_attribute hattr = { .hash = { .base = { NTDB_ATTRIBUTE_HASH },
						.fn = clash } };
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT,
	};

	hattr.base.next = &tap_log_attr;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 137 + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		struct hash_info h;
		ntdb_off_t new_off, new_off2, off;

		ntdb = ntdb_open("run-04-basichash.ntdb", flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &hattr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		v = 0;
		/* Should not find it. */
		ok1(find_and_lock(ntdb, key, F_WRLCK, &h, &rec, NULL) == 0);
		/* Should have created correct hash. */
		ok1(h.h == ntdb_hash(ntdb, key.dptr, key.dsize));
		/* Should have located space in top table, bucket 0. */
		ok1(h.table == NTDB_HASH_OFFSET);
		ok1(h.table_size == (1 << ntdb->hash_bits));
		ok1(h.bucket == 0);
		ok1(h.old_val == 0);

		/* Should have lock on bucket 0 */
		ok1(h.h == 0);
		ok1((ntdb->flags & NTDB_NOLOCK) || ntdb->file->num_lockrecs == 1);
		ok1((ntdb->flags & NTDB_NOLOCK)
		    || ntdb->file->lockrecs[0].off == NTDB_HASH_LOCK_START);
		/* FIXME: Check lock length */

		/* Allocate a new record. */
		new_off = alloc(ntdb, key.dsize, dbuf.dsize,
				NTDB_USED_MAGIC, false);
		ok1(!NTDB_OFF_IS_ERR(new_off));

		/* We should be able to add it now. */
		ok1(add_to_hash(ntdb, &h, new_off) == 0);

		/* Make sure we fill it in for later finding. */
		off = new_off + sizeof(struct ntdb_used_record);
		ok1(!ntdb->io->twrite(ntdb, off, key.dptr, key.dsize));
		off += key.dsize;
		ok1(!ntdb->io->twrite(ntdb, off, dbuf.dptr, dbuf.dsize));

		/* We should be able to unlock that OK. */
		ok1(ntdb_unlock_hash(ntdb, h.h, F_WRLCK) == 0);

		/* Database should be consistent. */
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);

		/* Now, this should give a successful lookup. */
		ok1(find_and_lock(ntdb, key, F_WRLCK, &h, &rec, NULL) == new_off);
		/* Should have created correct hash. */
		ok1(h.h == ntdb_hash(ntdb, key.dptr, key.dsize));
		/* Should have located it in top table, bucket 0. */
		ok1(h.table == NTDB_HASH_OFFSET);
		ok1(h.table_size == (1 << ntdb->hash_bits));
		ok1(h.bucket == 0);

		/* Should have lock on bucket 0 */
		ok1(h.h == 0);
		ok1((ntdb->flags & NTDB_NOLOCK) || ntdb->file->num_lockrecs == 1);
		ok1((ntdb->flags & NTDB_NOLOCK)
		    || ntdb->file->lockrecs[0].off == NTDB_HASH_LOCK_START);
		/* FIXME: Check lock length */

		ok1(ntdb_unlock_hash(ntdb, h.h, F_WRLCK) == 0);

		/* Database should be consistent. */
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);

		/* Test expansion. */
		v = 1;
		ok1(find_and_lock(ntdb, key, F_WRLCK, &h, &rec, NULL) == 0);
		/* Should have created correct hash. */
		ok1(h.h == ntdb_hash(ntdb, key.dptr, key.dsize));
		/* Should have located clash in toplevel bucket 0. */
		ok1(h.table == NTDB_HASH_OFFSET);
		ok1(h.table_size == (1 << ntdb->hash_bits));
		ok1(h.bucket == 0);
		ok1((h.old_val & NTDB_OFF_MASK) == new_off);

		/* Should have lock on bucket 0 */
		ok1((h.h & ((1 << ntdb->hash_bits)-1)) == 0);
		ok1((ntdb->flags & NTDB_NOLOCK) || ntdb->file->num_lockrecs == 1);
		ok1((ntdb->flags & NTDB_NOLOCK)
		    || ntdb->file->lockrecs[0].off == NTDB_HASH_LOCK_START);
		/* FIXME: Check lock length */

		new_off2 = alloc(ntdb, key.dsize, dbuf.dsize,
				 NTDB_USED_MAGIC, false);
		ok1(!NTDB_OFF_IS_ERR(new_off2));

		off = new_off2 + sizeof(struct ntdb_used_record);
		ok1(!ntdb->io->twrite(ntdb, off, key.dptr, key.dsize));
		off += key.dsize;
		ok1(!ntdb->io->twrite(ntdb, off, dbuf.dptr, dbuf.dsize));

		/* We should be able to add it now. */
		ok1(add_to_hash(ntdb, &h, new_off2) == 0);
		ok1(ntdb_unlock_hash(ntdb, h.h, F_WRLCK) == 0);

		/* Should be happy with expansion. */
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);

		/* Should be able to find both. */
		v = 1;
		ok1(find_and_lock(ntdb, key, F_WRLCK, &h, &rec, NULL) == new_off2);
		/* Should have created correct hash. */
		ok1(h.h == ntdb_hash(ntdb, key.dptr, key.dsize));
		/* Should have located space in chain. */
		ok1(h.table > NTDB_HASH_OFFSET);
		ok1(h.table_size == 2);
		ok1(h.bucket == 1);
		/* Should have lock on bucket 0 */
		ok1((h.h & ((1 << ntdb->hash_bits)-1)) == 0);
		ok1((ntdb->flags & NTDB_NOLOCK) || ntdb->file->num_lockrecs == 1);
		ok1((ntdb->flags & NTDB_NOLOCK)
		    || ntdb->file->lockrecs[0].off == NTDB_HASH_LOCK_START);
		ok1(ntdb_unlock_hash(ntdb, h.h, F_WRLCK) == 0);

		v = 0;
		ok1(find_and_lock(ntdb, key, F_WRLCK, &h, &rec, NULL) == new_off);
		/* Should have created correct hash. */
		ok1(h.h == ntdb_hash(ntdb, key.dptr, key.dsize));
		/* Should have located space in chain. */
		ok1(h.table > NTDB_HASH_OFFSET);
		ok1(h.table_size == 2);
		ok1(h.bucket == 0);

		/* Should have lock on bucket 0 */
		ok1((h.h & ((1 << ntdb->hash_bits)-1)) == 0);
		ok1((ntdb->flags & NTDB_NOLOCK) || ntdb->file->num_lockrecs == 1);
		ok1((ntdb->flags & NTDB_NOLOCK)
		    || ntdb->file->lockrecs[0].off == NTDB_HASH_LOCK_START);
		/* FIXME: Check lock length */

		/* Simple delete should work. */
		ok1(delete_from_hash(ntdb, &h) == 0);
		ok1(add_free_record(ntdb, new_off,
				    sizeof(struct ntdb_used_record)
				    + rec_key_length(&rec)
				    + rec_data_length(&rec)
				    + rec_extra_padding(&rec),
				    NTDB_LOCK_NOWAIT, false) == 0);
		ok1(ntdb_unlock_hash(ntdb, h.h, F_WRLCK) == 0);
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);

		/* Should still be able to find other record. */
		v = 1;
		ok1(find_and_lock(ntdb, key, F_WRLCK, &h, &rec, NULL) == new_off2);
		/* Should have created correct hash. */
		ok1(h.h == ntdb_hash(ntdb, key.dptr, key.dsize));
		/* Should have located space in chain. */
		ok1(h.table > NTDB_HASH_OFFSET);
		ok1(h.table_size == 2);
		ok1(h.bucket == 1);
		/* Should have lock on bucket 0 */
		ok1((h.h & ((1 << ntdb->hash_bits)-1)) == 0);
		ok1((ntdb->flags & NTDB_NOLOCK) || ntdb->file->num_lockrecs == 1);
		ok1((ntdb->flags & NTDB_NOLOCK)
		    || ntdb->file->lockrecs[0].off == NTDB_HASH_LOCK_START);
		ok1(ntdb_unlock_hash(ntdb, h.h, F_WRLCK) == 0);

		/* Now should find empty space. */
		v = 0;
		ok1(find_and_lock(ntdb, key, F_WRLCK, &h, &rec, NULL) == 0);
		/* Should have created correct hash. */
		ok1(h.h == ntdb_hash(ntdb, key.dptr, key.dsize));
		/* Should have located space in chain, bucket 0. */
		ok1(h.table > NTDB_HASH_OFFSET);
		ok1(h.table_size == 2);
		ok1(h.bucket == 0);
		ok1(h.old_val == 0);

		/* Adding another record should work. */
		v = 2;
		ok1(find_and_lock(ntdb, key, F_WRLCK, &h, &rec, NULL) == 0);
		/* Should have created correct hash. */
		ok1(h.h == ntdb_hash(ntdb, key.dptr, key.dsize));
		/* Should have located space in chain, bucket 0. */
		ok1(h.table > NTDB_HASH_OFFSET);
		ok1(h.table_size == 2);
		ok1(h.bucket == 0);
		ok1(h.old_val == 0);

		/* Should have lock on bucket 0 */
		ok1((h.h & ((1 << ntdb->hash_bits)-1)) == 0);
		ok1((ntdb->flags & NTDB_NOLOCK) || ntdb->file->num_lockrecs == 1);
		ok1((ntdb->flags & NTDB_NOLOCK)
		    || ntdb->file->lockrecs[0].off == NTDB_HASH_LOCK_START);

		new_off = alloc(ntdb, key.dsize, dbuf.dsize,
				NTDB_USED_MAGIC, false);
		ok1(!NTDB_OFF_IS_ERR(new_off2));
		ok1(add_to_hash(ntdb, &h, new_off) == 0);
		ok1(ntdb_unlock_hash(ntdb, h.h, F_WRLCK) == 0);

		off = new_off + sizeof(struct ntdb_used_record);
		ok1(!ntdb->io->twrite(ntdb, off, key.dptr, key.dsize));
		off += key.dsize;
		ok1(!ntdb->io->twrite(ntdb, off, dbuf.dptr, dbuf.dsize));

		/* Adding another record should cause expansion. */
		v = 3;
		ok1(find_and_lock(ntdb, key, F_WRLCK, &h, &rec, NULL) == 0);
		/* Should have created correct hash. */
		ok1(h.h == ntdb_hash(ntdb, key.dptr, key.dsize));
		/* Should not have located space in chain. */
		ok1(h.table > NTDB_HASH_OFFSET);
		ok1(h.table_size == 2);
		ok1(h.bucket == 2);
		ok1(h.old_val != 0);

		/* Should have lock on bucket 0 */
		ok1((h.h & ((1 << ntdb->hash_bits)-1)) == 0);
		ok1((ntdb->flags & NTDB_NOLOCK) || ntdb->file->num_lockrecs == 1);
		ok1((ntdb->flags & NTDB_NOLOCK)
		    || ntdb->file->lockrecs[0].off == NTDB_HASH_LOCK_START);

		new_off = alloc(ntdb, key.dsize, dbuf.dsize,
				NTDB_USED_MAGIC, false);
		ok1(!NTDB_OFF_IS_ERR(new_off2));
		off = new_off + sizeof(struct ntdb_used_record);
		ok1(!ntdb->io->twrite(ntdb, off, key.dptr, key.dsize));
		off += key.dsize;
		ok1(!ntdb->io->twrite(ntdb, off, dbuf.dptr, dbuf.dsize));
		ok1(add_to_hash(ntdb, &h, new_off) == 0);
		ok1(ntdb_unlock_hash(ntdb, h.h, F_WRLCK) == 0);

		/* Retrieve it and check. */
		ok1(find_and_lock(ntdb, key, F_WRLCK, &h, &rec, NULL) == new_off);
		/* Should have created correct hash. */
		ok1(h.h == ntdb_hash(ntdb, key.dptr, key.dsize));
		/* Should have appended to chain, bucket 2. */
		ok1(h.table > NTDB_HASH_OFFSET);
		ok1(h.table_size == 3);
		ok1(h.bucket == 2);

		/* Should have lock on bucket 0 */
		ok1((h.h & ((1 << ntdb->hash_bits)-1)) == 0);
		ok1((ntdb->flags & NTDB_NOLOCK) || ntdb->file->num_lockrecs == 1);
		ok1((ntdb->flags & NTDB_NOLOCK)
		    || ntdb->file->lockrecs[0].off == NTDB_HASH_LOCK_START);
		ok1(ntdb_unlock_hash(ntdb, h.h, F_WRLCK) == 0);

		/* YA record: relocation. */
		v = 4;
		ok1(find_and_lock(ntdb, key, F_WRLCK, &h, &rec, NULL) == 0);
		/* Should have created correct hash. */
		ok1(h.h == ntdb_hash(ntdb, key.dptr, key.dsize));
		/* Should not have located space in chain. */
		ok1(h.table > NTDB_HASH_OFFSET);
		ok1(h.table_size == 3);
		ok1(h.bucket == 3);
		ok1(h.old_val != 0);

		/* Should have lock on bucket 0 */
		ok1((h.h & ((1 << ntdb->hash_bits)-1)) == 0);
		ok1((ntdb->flags & NTDB_NOLOCK) || ntdb->file->num_lockrecs == 1);
		ok1((ntdb->flags & NTDB_NOLOCK)
		    || ntdb->file->lockrecs[0].off == NTDB_HASH_LOCK_START);

		new_off = alloc(ntdb, key.dsize, dbuf.dsize,
				NTDB_USED_MAGIC, false);
		ok1(!NTDB_OFF_IS_ERR(new_off2));
		off = new_off + sizeof(struct ntdb_used_record);
		ok1(!ntdb->io->twrite(ntdb, off, key.dptr, key.dsize));
		off += key.dsize;
		ok1(!ntdb->io->twrite(ntdb, off, dbuf.dptr, dbuf.dsize));
		ok1(add_to_hash(ntdb, &h, new_off) == 0);
		ok1(ntdb_unlock_hash(ntdb, h.h, F_WRLCK) == 0);

		/* Retrieve it and check. */
		ok1(find_and_lock(ntdb, key, F_WRLCK, &h, &rec, NULL) == new_off);
		/* Should have created correct hash. */
		ok1(h.h == ntdb_hash(ntdb, key.dptr, key.dsize));
		/* Should have appended to chain, bucket 2. */
		ok1(h.table > NTDB_HASH_OFFSET);
		ok1(h.table_size == 4);
		ok1(h.bucket == 3);

		/* Should have lock on bucket 0 */
		ok1((h.h & ((1 << ntdb->hash_bits)-1)) == 0);
		ok1((ntdb->flags & NTDB_NOLOCK) || ntdb->file->num_lockrecs == 1);
		ok1((ntdb->flags & NTDB_NOLOCK)
		    || ntdb->file->lockrecs[0].off == NTDB_HASH_LOCK_START);
		ok1(ntdb_unlock_hash(ntdb, h.h, F_WRLCK) == 0);

		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
