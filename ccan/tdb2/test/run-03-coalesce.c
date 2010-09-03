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
	if (frec_magic(&f) != TDB_FREE_MAGIC)
		return TDB_OFF_ERR;
	return f.data_len;
}

int main(int argc, char *argv[])
{
	tdb_off_t b_off, zone_off;
	struct tdb_context *tdb;
	struct tdb_layout *layout;
	struct tdb_data data, key;
	tdb_len_t len;
	unsigned int zone_bits = 16;

	/* FIXME: Test TDB_CONVERT */

	plan_tests(45);
	data.dptr = (void *)"world";
	data.dsize = 5;
	key.dptr = (void *)"hello";
	key.dsize = 5;

	/* No coalescing can be done due to EOF */
	layout = new_tdb_layout();
	tdb_layout_add_zone(layout, zone_bits, false);
	tdb_layout_add_hashtable(layout, 12, 0);
	tdb = tdb_layout_get(layout);
	len = layout->elem[2].free.len;
	zone_off = layout->elem[0].base.off;
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	ok1(free_record_length(tdb, layout->elem[2].base.off) == len);

	/* Figure out which bucket free entry is. */
	b_off = bucket_off(zone_off, size_to_bucket(zone_bits, len));
	/* Lock and fail to coalesce. */
	ok1(tdb_lock_free_bucket(tdb, b_off, TDB_LOCK_WAIT) == 0);
	ok1(coalesce(tdb, zone_off, zone_bits, layout->elem[2].base.off,
		     b_off, len) == 0);
	tdb_unlock_free_bucket(tdb, b_off);
	tdb_unlock_list(tdb, 0, F_WRLCK);
	ok1(free_record_length(tdb, layout->elem[2].base.off) == len);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	/* No coalescing can be done due to used record */
	layout = new_tdb_layout();
	tdb_layout_add_zone(layout, zone_bits, false);
	tdb_layout_add_hashtable(layout, 12, 0);
	tdb_layout_add_free(layout, 1024);
	tdb_layout_add_used(layout, key, data, 6);
	tdb = tdb_layout_get(layout);
	zone_off = layout->elem[0].base.off;
	ok1(free_record_length(tdb, layout->elem[2].base.off) == 1024);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* Figure out which bucket free entry is. */
	b_off = bucket_off(zone_off, size_to_bucket(zone_bits, 1024));
	/* Lock and fail to coalesce. */
	ok1(tdb_lock_free_bucket(tdb, b_off, TDB_LOCK_WAIT) == 0);
	ok1(coalesce(tdb, zone_off, zone_bits, layout->elem[2].base.off,
		     b_off, 1024) == 0);
	tdb_unlock_free_bucket(tdb, b_off);
	ok1(free_record_length(tdb, layout->elem[2].base.off) == 1024);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	/* Coalescing can be done due to two free records, then EOF */
	layout = new_tdb_layout();
	tdb_layout_add_zone(layout, zone_bits, false);
	tdb_layout_add_hashtable(layout, 12, 0);
	tdb_layout_add_free(layout, 1024);
	tdb = tdb_layout_get(layout);
	zone_off = layout->elem[0].base.off;
	len = layout->elem[3].free.len;
	ok1(free_record_length(tdb, layout->elem[2].base.off) == 1024);
	ok1(free_record_length(tdb, layout->elem[3].base.off) == len);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* Figure out which bucket (first) free entry is. */
	b_off = bucket_off(zone_off, size_to_bucket(zone_bits, 1024));
	/* Lock and coalesce. */
	ok1(tdb_lock_free_bucket(tdb, b_off, TDB_LOCK_WAIT) == 0);
	ok1(coalesce(tdb, zone_off, zone_bits, layout->elem[2].base.off,
		     b_off, 1024) == 1);
	ok1(!tdb_has_locks(tdb));
	ok1(free_record_length(tdb, layout->elem[2].base.off)
	    == 1024 + sizeof(struct tdb_used_record) + len);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	/* Coalescing can be done due to two free records, then data */
	layout = new_tdb_layout();
	tdb_layout_add_zone(layout, zone_bits, false);
	tdb_layout_add_hashtable(layout, 12, 0);
	tdb_layout_add_free(layout, 1024);
	tdb_layout_add_free(layout, 512);
	tdb_layout_add_used(layout, key, data, 6);
	tdb = tdb_layout_get(layout);
	zone_off = layout->elem[0].base.off;
	ok1(free_record_length(tdb, layout->elem[2].base.off) == 1024);
	ok1(free_record_length(tdb, layout->elem[3].base.off) == 512);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* Figure out which bucket free entry is. */
	b_off = bucket_off(zone_off, size_to_bucket(zone_bits, 1024));
	/* Lock and coalesce. */
	ok1(tdb_lock_free_bucket(tdb, b_off, TDB_LOCK_WAIT) == 0);
	ok1(coalesce(tdb, zone_off, zone_bits, layout->elem[2].base.off,
		     b_off, 1024) == 1);
	ok1(!tdb_has_locks(tdb));
	ok1(free_record_length(tdb, layout->elem[2].base.off)
	    == 1024 + sizeof(struct tdb_used_record) + 512);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	/* Coalescing can be done due to three free records, then EOF */
	layout = new_tdb_layout();
	tdb_layout_add_zone(layout, zone_bits, false);
	tdb_layout_add_hashtable(layout, 12, 0);
	tdb_layout_add_free(layout, 1024);
	tdb_layout_add_free(layout, 512);
	tdb = tdb_layout_get(layout);
	zone_off = layout->elem[0].base.off;
	len = layout->elem[4].free.len;
	ok1(free_record_length(tdb, layout->elem[2].base.off) == 1024);
	ok1(free_record_length(tdb, layout->elem[3].base.off) == 512);
	ok1(free_record_length(tdb, layout->elem[4].base.off) == len);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* Figure out which bucket free entry is. */
	b_off = bucket_off(zone_off, size_to_bucket(zone_bits, 1024));
	/* Lock and coalesce. */
	ok1(tdb_lock_free_bucket(tdb, b_off, TDB_LOCK_WAIT) == 0);
	ok1(coalesce(tdb, zone_off, zone_bits, layout->elem[2].base.off,
		     b_off, 1024) == 1);
	ok1(!tdb_has_locks(tdb));
	ok1(free_record_length(tdb, layout->elem[2].base.off)
	    == 1024 + sizeof(struct tdb_used_record) + 512
	    + sizeof(struct tdb_used_record) + len);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	/* Coalescing across two zones isn't possible. */
	layout = new_tdb_layout();
	tdb_layout_add_zone(layout, zone_bits, false);
	tdb_layout_add_hashtable(layout, 12, 0);
	tdb_layout_add_zone(layout, zone_bits, true);
	tdb = tdb_layout_get(layout);
	zone_off = layout->elem[0].base.off;
	len = layout->elem[2].free.len;
	ok1(free_record_length(tdb, layout->elem[2].base.off) == len);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* Figure out which list free entry is. */
	b_off = bucket_off(zone_off, size_to_bucket(zone_bits, len));
	/* Lock and coalesce. */
	ok1(tdb_lock_free_bucket(tdb, b_off, TDB_LOCK_WAIT) == 0);
	ok1(coalesce(tdb, zone_off, zone_bits, layout->elem[2].base.off,
		     b_off, len) == 0);
	tdb_unlock_free_bucket(tdb, b_off);
	ok1(!tdb_has_locks(tdb));
	ok1(free_record_length(tdb, layout->elem[2].base.off) == len);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	ok1(tap_log_messages == 0);
	return exit_status();
}
