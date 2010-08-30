#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/check.c>
#include <ccan/tap/tap.h>
#include "logging.h"
#include "layout.h"

static tdb_len_t free_record_length(struct tdb_context *tdb, tdb_off_t off)
{
	struct tdb_free_record f;

	if (tdb_read_convert(tdb, off, &f, sizeof(f)) != 0)
		return TDB_OFF_ERR;
	if (f.magic != TDB_FREE_MAGIC)
		return TDB_OFF_ERR;
	return f.data_len;
}

int main(int argc, char *argv[])
{
	tdb_off_t list;
	struct tdb_context *tdb;
	struct tdb_layout *layout;
	struct tdb_data data, key;
	tdb_len_t total;
	unsigned int i;

	/* FIXME: Test TDB_CONVERT */

	plan_tests(62);
	data.dptr = (void *)"world";
	data.dsize = 5;
	key.dptr = (void *)"hello";
	key.dsize = 5;

	/* No coalescing can be done due to EOF */
	layout = new_tdb_layout();
	tdb_layout_add_hashtable(layout, 12, 0);
	tdb_layout_add_freetable(layout, 1, 16, 12, 0);
	tdb_layout_add_free(layout, 1024);
	tdb = tdb_layout_get(layout);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	ok1(free_record_length(tdb, layout->elem[2].base.off) == 1024);

	/* Figure out which list free entry is. */
	list = size_to_bucket(tdb, 1024);
	/* Lock and fail to coalesce. */
	ok1(tdb_lock_list(tdb, 0, F_WRLCK, TDB_LOCK_WAIT) == 0);
	ok1(tdb_lock_free_list(tdb, list, TDB_LOCK_WAIT) == 0);
	ok1(coalesce(tdb, layout->elem[2].base.off, list, 1024) == 0);
	tdb_unlock_free_list(tdb, list);
	tdb_unlock_list(tdb, 0, F_WRLCK);
	ok1(free_record_length(tdb, layout->elem[2].base.off) == 1024);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	/* No coalescing can be done due to used record */
	layout = new_tdb_layout();
	tdb_layout_add_hashtable(layout, 12, 0);
	tdb_layout_add_freetable(layout, 1, 16, 12, 0);
	tdb_layout_add_free(layout, 1024);
	tdb_layout_add_used(layout, key, data, 6);
	tdb = tdb_layout_get(layout);
	ok1(free_record_length(tdb, layout->elem[2].base.off) == 1024);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* Figure out which list free entry is. */
	list = size_to_bucket(tdb, 1024);
	/* Lock and fail to coalesce. */
	ok1(tdb_lock_list(tdb, 0, F_WRLCK, TDB_LOCK_WAIT) == 0);
	ok1(tdb_lock_free_list(tdb, list, TDB_LOCK_WAIT) == 0);
	ok1(coalesce(tdb, layout->elem[2].base.off, list, 1024) == 0);
	tdb_unlock_free_list(tdb, list);
	tdb_unlock_list(tdb, 0, F_WRLCK);
	ok1(free_record_length(tdb, layout->elem[2].base.off) == 1024);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	/* Coalescing can be done due to two free records, then EOF */
	layout = new_tdb_layout();
	tdb_layout_add_hashtable(layout, 12, 0);
	tdb_layout_add_freetable(layout, 1, 16, 12, 0);
	tdb_layout_add_free(layout, 1024);
	tdb_layout_add_free(layout, 512);
	tdb = tdb_layout_get(layout);
	ok1(free_record_length(tdb, layout->elem[2].base.off) == 1024);
	ok1(free_record_length(tdb, layout->elem[3].base.off) == 512);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* Figure out which list free entry is. */
	list = size_to_bucket(tdb, 1024);
	/* Lock and coalesce. */
	ok1(tdb_lock_list(tdb, 0, F_WRLCK, TDB_LOCK_WAIT) == 0);
	ok1(tdb_lock_free_list(tdb, list, TDB_LOCK_WAIT) == 0);
	ok1(coalesce(tdb, layout->elem[2].base.off, list, 1024) == 1);
	tdb_unlock_list(tdb, 0, F_WRLCK);
	ok1(!tdb_has_locks(tdb));
	ok1(free_record_length(tdb, layout->elem[2].base.off)
	    == 1024 + sizeof(struct tdb_used_record) + 512);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	/* Coalescing can be done due to two free records, then data */
	layout = new_tdb_layout();
	tdb_layout_add_hashtable(layout, 12, 0);
	tdb_layout_add_freetable(layout, 1, 16, 12, 0);
	tdb_layout_add_free(layout, 1024);
	tdb_layout_add_free(layout, 512);
	tdb_layout_add_used(layout, key, data, 6);
	tdb = tdb_layout_get(layout);
	ok1(free_record_length(tdb, layout->elem[2].base.off) == 1024);
	ok1(free_record_length(tdb, layout->elem[3].base.off) == 512);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* Figure out which list free entry is. */
	list = size_to_bucket(tdb, 1024);
	/* Lock and coalesce. */
	ok1(tdb_lock_list(tdb, 0, F_WRLCK, TDB_LOCK_WAIT) == 0);
	ok1(tdb_lock_free_list(tdb, list, TDB_LOCK_WAIT) == 0);
	ok1(coalesce(tdb, layout->elem[2].base.off, list, 1024) == 1);
	tdb_unlock_list(tdb, 0, F_WRLCK);
	ok1(!tdb_has_locks(tdb));
	ok1(free_record_length(tdb, layout->elem[2].base.off)
	    == 1024 + sizeof(struct tdb_used_record) + 512);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	/* Coalescing can be done due to three free records, then EOF */
	layout = new_tdb_layout();
	tdb_layout_add_hashtable(layout, 12, 0);
	tdb_layout_add_freetable(layout, 1, 16, 12, 0);
	tdb_layout_add_free(layout, 1024);
	tdb_layout_add_free(layout, 512);
	tdb_layout_add_free(layout, 32);
	tdb = tdb_layout_get(layout);
	ok1(free_record_length(tdb, layout->elem[2].base.off) == 1024);
	ok1(free_record_length(tdb, layout->elem[3].base.off) == 512);
	ok1(free_record_length(tdb, layout->elem[4].base.off) == 32);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* Figure out which list free entry is. */
	list = size_to_bucket(tdb, 1024);
	/* Lock and coalesce. */
	ok1(tdb_lock_list(tdb, 0, F_WRLCK, TDB_LOCK_WAIT) == 0);
	ok1(tdb_lock_free_list(tdb, list, TDB_LOCK_WAIT) == 0);
	ok1(coalesce(tdb, layout->elem[2].base.off, list, 1024) == 1);
	tdb_unlock_list(tdb, 0, F_WRLCK);
	ok1(!tdb_has_locks(tdb));
	ok1(free_record_length(tdb, layout->elem[2].base.off)
	    == 1024 + sizeof(struct tdb_used_record) + 512
	    + sizeof(struct tdb_used_record) + 32);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	/* Coalescing across two zones. */
	layout = new_tdb_layout();
	tdb_layout_add_hashtable(layout, 12, 0);
	tdb_layout_add_freetable(layout, 2, 16, 12, 0);
	tdb_layout_add_free(layout, 32768);
	tdb_layout_add_free(layout, 30000);
	tdb = tdb_layout_get(layout);
	ok1(free_record_length(tdb, layout->elem[2].base.off) == 32768);
	ok1(zone_of(tdb, layout->elem[2].base.off) == 0);
	ok1(free_record_length(tdb, layout->elem[3].base.off) == 30000);
	ok1(zone_of(tdb, layout->elem[3].base.off) == 1);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* Figure out which list free entry is. */
	list = size_to_bucket(tdb, 32768);
	/* Lock and coalesce. */
	ok1(tdb_lock_list(tdb, 0, F_WRLCK, TDB_LOCK_WAIT) == 0);
	ok1(tdb_lock_free_list(tdb, list, TDB_LOCK_WAIT) == 0);
	ok1(coalesce(tdb, layout->elem[2].base.off, list, 32768) == 1);
	tdb_unlock_list(tdb, 0, F_WRLCK);
	ok1(!tdb_has_locks(tdb));
	ok1(free_record_length(tdb, layout->elem[2].base.off)
	    == 32768 + sizeof(struct tdb_used_record) + 30000);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	/* Coalescing many across many zones. */
	layout = new_tdb_layout();
	tdb_layout_add_hashtable(layout, 12, 0);
	tdb_layout_add_freetable(layout, 8, 16, 12, 0);
	total = 0;
	for (i = 4; i < 16; i++) {
		tdb_layout_add_free(layout, 1 << i);
		total += sizeof(struct tdb_used_record) + (1 << i);
	}
	total -= sizeof(struct tdb_used_record);
	tdb = tdb_layout_get(layout);
	ok1(free_record_length(tdb, layout->elem[2].base.off) == 1 << 4);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* Figure out which list free entry is. */
	list = size_to_bucket(tdb, 1 << 4);
	/* Lock and coalesce. */
	ok1(tdb_lock_list(tdb, 0, F_WRLCK, TDB_LOCK_WAIT) == 0);
	ok1(tdb_lock_free_list(tdb, list, TDB_LOCK_WAIT) == 0);
	ok1(coalesce(tdb, layout->elem[2].base.off, list, 1 << 4) == 1);
	tdb_unlock_list(tdb, 0, F_WRLCK);
	ok1(!tdb_has_locks(tdb));
	ok1(free_record_length(tdb, layout->elem[2].base.off) == total);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	ok1(tap_log_messages == 0);
	return exit_status();
}
