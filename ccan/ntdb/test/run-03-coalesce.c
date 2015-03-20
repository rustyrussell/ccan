#include "ntdb-source.h"
#include "tap-interface.h"
#include "logging.h"
#include "layout.h"
#include "helprun-external-agent.h"

static ntdb_len_t free_record_length(struct ntdb_context *ntdb, ntdb_off_t off)
{
	struct ntdb_free_record f;
	enum NTDB_ERROR ecode;

	ecode = ntdb_read_convert(ntdb, off, &f, sizeof(f));
	if (ecode != NTDB_SUCCESS)
		return ecode;
	if (frec_magic(&f) != NTDB_FREE_MAGIC)
		return NTDB_ERR_CORRUPT;
	return frec_len(&f);
}

int main(int argc, char *argv[])
{
	ntdb_off_t b_off, test;
	struct ntdb_context *ntdb;
	struct ntdb_layout *layout;
	NTDB_DATA data, key;
	ntdb_len_t len;

	/* FIXME: Test NTDB_CONVERT */
	/* FIXME: Test lock order fail. */

	plan_tests(42);
	data = ntdb_mkdata("world", 5);
	key = ntdb_mkdata("hello", 5);

	/* No coalescing can be done due to EOF */
	layout = new_ntdb_layout();
	ntdb_layout_add_freetable(layout);
	len = 15560;
	ntdb_layout_add_free(layout, len, 0);
	ntdb_layout_write(layout, free, &tap_log_attr, "run-03-coalesce.ntdb");
	/* NOMMAP is for lockcheck. */
	ntdb = ntdb_open("run-03-coalesce.ntdb", NTDB_NOMMAP|MAYBE_NOSYNC,
			 O_RDWR, 0, &tap_log_attr);
	ok1(ntdb_check(ntdb, NULL, NULL) == 0);
	ok1(free_record_length(ntdb, layout->elem[1].base.off) == len);

	/* Figure out which bucket free entry is. */
	b_off = bucket_off(ntdb->ftable_off, size_to_bucket(len));
	/* Lock and fail to coalesce. */
	ok1(ntdb_lock_free_bucket(ntdb, b_off, NTDB_LOCK_WAIT) == 0);
	test = layout->elem[1].base.off;
	ok1(coalesce(ntdb, layout->elem[1].base.off, b_off, len, &test)
	    == 0);
	ntdb_unlock_free_bucket(ntdb, b_off);
	ok1(free_record_length(ntdb, layout->elem[1].base.off) == len);
	ok1(test == layout->elem[1].base.off);
	ok1(ntdb_check(ntdb, NULL, NULL) == 0);
	ntdb_close(ntdb);
	ntdb_layout_free(layout);

	/* No coalescing can be done due to used record */
	layout = new_ntdb_layout();
	ntdb_layout_add_freetable(layout);
	ntdb_layout_add_free(layout, 15528, 0);
	ntdb_layout_add_used(layout, key, data, 6);
	ntdb_layout_write(layout, free, &tap_log_attr, "run-03-coalesce.ntdb");
	/* NOMMAP is for lockcheck. */
	ntdb = ntdb_open("run-03-coalesce.ntdb", NTDB_NOMMAP|MAYBE_NOSYNC,
			 O_RDWR, 0, &tap_log_attr);
	ok1(free_record_length(ntdb, layout->elem[1].base.off) == 15528);
	ok1(ntdb_check(ntdb, NULL, NULL) == 0);

	/* Figure out which bucket free entry is. */
	b_off = bucket_off(ntdb->ftable_off, size_to_bucket(15528));
	/* Lock and fail to coalesce. */
	ok1(ntdb_lock_free_bucket(ntdb, b_off, NTDB_LOCK_WAIT) == 0);
	test = layout->elem[1].base.off;
	ok1(coalesce(ntdb, layout->elem[1].base.off, b_off, 15528, &test)
	    == 0);
	ntdb_unlock_free_bucket(ntdb, b_off);
	ok1(free_record_length(ntdb, layout->elem[1].base.off) == 15528);
	ok1(test == layout->elem[1].base.off);
	ok1(ntdb_check(ntdb, NULL, NULL) == 0);
	ntdb_close(ntdb);
	ntdb_layout_free(layout);

	/* Coalescing can be done due to two free records, then EOF */
	layout = new_ntdb_layout();
	ntdb_layout_add_freetable(layout);
	ntdb_layout_add_free(layout, 1024, 0);
	ntdb_layout_add_free(layout, 14520, 0);
	ntdb_layout_write(layout, free, &tap_log_attr, "run-03-coalesce.ntdb");
	/* NOMMAP is for lockcheck. */
	ntdb = ntdb_open("run-03-coalesce.ntdb", NTDB_NOMMAP|MAYBE_NOSYNC,
			 O_RDWR, 0, &tap_log_attr);
	ok1(free_record_length(ntdb, layout->elem[1].base.off) == 1024);
	ok1(free_record_length(ntdb, layout->elem[2].base.off) == 14520);
	ok1(ntdb_check(ntdb, NULL, NULL) == 0);

	/* Figure out which bucket (first) free entry is. */
	b_off = bucket_off(ntdb->ftable_off, size_to_bucket(1024));
	/* Lock and coalesce. */
	ok1(ntdb_lock_free_bucket(ntdb, b_off, NTDB_LOCK_WAIT) == 0);
	test = layout->elem[2].base.off;
	ok1(coalesce(ntdb, layout->elem[1].base.off, b_off, 1024, &test)
	    == 1024 + sizeof(struct ntdb_used_record) + 14520);
	/* Should tell us it's erased this one... */
	ok1(test == NTDB_ERR_NOEXIST);
	ok1(ntdb->file->allrecord_lock.count == 0 && ntdb->file->num_lockrecs == 0);
	ok1(free_record_length(ntdb, layout->elem[1].base.off)
	    == 1024 + sizeof(struct ntdb_used_record) + 14520);
	ok1(ntdb_check(ntdb, NULL, NULL) == 0);
	ntdb_close(ntdb);
	ntdb_layout_free(layout);

	/* Coalescing can be done due to two free records, then data */
	layout = new_ntdb_layout();
	ntdb_layout_add_freetable(layout);
	ntdb_layout_add_free(layout, 1024, 0);
	ntdb_layout_add_free(layout, 14488, 0);
	ntdb_layout_add_used(layout, key, data, 6);
	ntdb_layout_write(layout, free, &tap_log_attr, "run-03-coalesce.ntdb");
	/* NOMMAP is for lockcheck. */
	ntdb = ntdb_open("run-03-coalesce.ntdb", NTDB_NOMMAP|MAYBE_NOSYNC,
			 O_RDWR, 0, &tap_log_attr);
	ok1(free_record_length(ntdb, layout->elem[1].base.off) == 1024);
	ok1(free_record_length(ntdb, layout->elem[2].base.off) == 14488);
	ok1(ntdb_check(ntdb, NULL, NULL) == 0);

	/* Figure out which bucket free entry is. */
	b_off = bucket_off(ntdb->ftable_off, size_to_bucket(1024));
	/* Lock and coalesce. */
	ok1(ntdb_lock_free_bucket(ntdb, b_off, NTDB_LOCK_WAIT) == 0);
	test = layout->elem[2].base.off;
	ok1(coalesce(ntdb, layout->elem[1].base.off, b_off, 1024, &test)
	    == 1024 + sizeof(struct ntdb_used_record) + 14488);
	ok1(ntdb->file->allrecord_lock.count == 0 && ntdb->file->num_lockrecs == 0);
	ok1(free_record_length(ntdb, layout->elem[1].base.off)
	    == 1024 + sizeof(struct ntdb_used_record) + 14488);
	ok1(test == NTDB_ERR_NOEXIST);
	ok1(ntdb_check(ntdb, NULL, NULL) == 0);
	ntdb_close(ntdb);
	ntdb_layout_free(layout);

	/* Coalescing can be done due to three free records, then EOF */
	layout = new_ntdb_layout();
	ntdb_layout_add_freetable(layout);
	ntdb_layout_add_free(layout, 1024, 0);
	ntdb_layout_add_free(layout, 512, 0);
	ntdb_layout_add_free(layout, 13992, 0);
	ntdb_layout_write(layout, free, &tap_log_attr, "run-03-coalesce.ntdb");
	/* NOMMAP is for lockcheck. */
	ntdb = ntdb_open("run-03-coalesce.ntdb", NTDB_NOMMAP|MAYBE_NOSYNC,
			 O_RDWR, 0, &tap_log_attr);
	ok1(free_record_length(ntdb, layout->elem[1].base.off) == 1024);
	ok1(free_record_length(ntdb, layout->elem[2].base.off) == 512);
	ok1(free_record_length(ntdb, layout->elem[3].base.off) == 13992);
	ok1(ntdb_check(ntdb, NULL, NULL) == 0);

	/* Figure out which bucket free entry is. */
	b_off = bucket_off(ntdb->ftable_off, size_to_bucket(1024));
	/* Lock and coalesce. */
	ok1(ntdb_lock_free_bucket(ntdb, b_off, NTDB_LOCK_WAIT) == 0);
	test = layout->elem[2].base.off;
	ok1(coalesce(ntdb, layout->elem[1].base.off, b_off, 1024, &test)
	    == 1024 + sizeof(struct ntdb_used_record) + 512
	    + sizeof(struct ntdb_used_record) + 13992);
	ok1(ntdb->file->allrecord_lock.count == 0
	    && ntdb->file->num_lockrecs == 0);
	ok1(free_record_length(ntdb, layout->elem[1].base.off)
	    == 1024 + sizeof(struct ntdb_used_record) + 512
	    + sizeof(struct ntdb_used_record) + 13992);
	ok1(ntdb_check(ntdb, NULL, NULL) == 0);
	ntdb_close(ntdb);
	ntdb_layout_free(layout);

	ok1(tap_log_messages == 0);
	return exit_status();
}
