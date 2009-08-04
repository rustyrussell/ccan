/* We need this otherwise fcntl locking fails. */
#define _FILE_OFFSET_BITS 64
#define _XOPEN_SOURCE 500
#include "tdb/tdb.h"
#include "tdb/io.c"
#include "tdb/tdb.c"
#include "tdb/lock.c"
#include "tdb/freelist.c"
#include "tdb/traverse.c"
#include "tdb/transaction.c"
#include "tdb/error.c"
#include "tdb/open.c"
#include "tap/tap.h"
#include <stdlib.h>
#include <err.h>

static int tdb_expand_file_sparse(struct tdb_context *tdb,
				  tdb_off_t size,
				  tdb_off_t addition)
{
	if (tdb->read_only || tdb->traverse_read) {
		tdb->ecode = TDB_ERR_RDONLY;
		return -1;
	}

	if (ftruncate(tdb->fd, size+addition) == -1) {
		char b = 0;
		ssize_t written = pwrite(tdb->fd,  &b, 1, (size+addition) - 1);
		if (written == 0) {
			/* try once more, potentially revealing errno */
			written = pwrite(tdb->fd,  &b, 1, (size+addition) - 1);
		}
		if (written == 0) {
			/* again - give up, guessing errno */
			errno = ENOSPC;
		}
		if (written != 1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "expand_file to %d failed (%s)\n", 
				 size+addition, strerror(errno)));
			return -1;
		}
	}

	return 0;
}

static const struct tdb_methods large_io_methods = {
	tdb_read,
	tdb_write,
	tdb_next_hash_chain,
	tdb_oob,
	tdb_expand_file_sparse,
	tdb_brlock
};

static int test_traverse(struct tdb_context *tdb, TDB_DATA key, TDB_DATA data,
			 void *_data)
{
	TDB_DATA *expect = _data;
	ok1(key.dsize == strlen("hi"));
	ok1(memcmp(key.dptr, "hi", strlen("hi")) == 0);
	ok1(data.dsize == expect->dsize);
	ok1(memcmp(data.dptr, expect->dptr, data.dsize) == 0);
	return 0;
}

int main(int argc, char *argv[])
{
	struct tdb_context *tdb;
	TDB_DATA key, orig_data, data;
	uint32_t hash;
	tdb_off_t rec_ptr;
	struct list_struct rec;

	plan_tests(24);
	tdb = tdb_open("/tmp/test.tdb", 1024, TDB_CLEAR_IF_FIRST,
		       O_CREAT|O_TRUNC|O_RDWR, 0600);

	ok1(tdb);
	tdb->methods = &large_io_methods;

	/* Enlarge the file (internally multiplies by 100). */
	ok1(tdb_expand(tdb, 30000000) == 0);

	/* Put an entry in, and check it. */
	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";
	orig_data.dsize = strlen("world");
	orig_data.dptr = (void *)"world";

	ok1(tdb_store(tdb, key, orig_data, TDB_INSERT) == 0);

	data = tdb_fetch(tdb, key);
	ok1(data.dsize == strlen("world"));
	ok1(memcmp(data.dptr, "world", strlen("world")) == 0);
	free(data.dptr);

	/* That currently fills at the end, make sure that's true. */
	hash = tdb->hash_fn(&key);
	rec_ptr = tdb_find_lock_hash(tdb, key, hash, F_RDLCK, &rec);
	ok1(rec_ptr);
	ok1(rec_ptr > 2U*1024*1024*1024);
	tdb_unlock(tdb, BUCKET(rec.full_hash), F_RDLCK);

	/* Traverse must work. */
	ok1(tdb_traverse(tdb, test_traverse, &orig_data) == 1);

	/* Delete should work. */
	ok1(tdb_delete(tdb, key) == 0);

	ok1(tdb_traverse(tdb, test_traverse, NULL) == 0);

	/* Transactions should work. */
	ok1(tdb_transaction_start(tdb) == 0);
	ok1(tdb_store(tdb, key, orig_data, TDB_INSERT) == 0);

	data = tdb_fetch(tdb, key);
	ok1(data.dsize == strlen("world"));
	ok1(memcmp(data.dptr, "world", strlen("world")) == 0);
	free(data.dptr);
	ok1(tdb_transaction_commit(tdb) == 0);

	ok1(tdb_traverse(tdb, test_traverse, &orig_data) == 1);
	tdb_close(tdb);

	return exit_status();
}
