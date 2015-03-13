#include "ntdb-source.h"
#include "tap-interface.h"
#include "logging.h"
#include "layout.h"

int main(int argc, char *argv[])
{
	ntdb_off_t off;
	struct ntdb_context *ntdb;
	struct ntdb_layout *layout;
	NTDB_DATA key, data;
	union ntdb_attribute seed;

	/* This seed value previously tickled a layout.c bug. */
	seed.base.attr = NTDB_ATTRIBUTE_SEED;
	seed.seed.seed = 0xb1142bc054d035b4ULL;
	seed.base.next = &tap_log_attr;

	plan_tests(11);
	key = ntdb_mkdata("Hello", 5);
	data = ntdb_mkdata("world", 5);

	/* Create a NTDB with three free tables. */
	layout = new_ntdb_layout();
	ntdb_layout_add_freetable(layout);
	ntdb_layout_add_freetable(layout);
	ntdb_layout_add_freetable(layout);
	ntdb_layout_add_free(layout, 80, 0);
	/* Used record prevent coalescing. */
	ntdb_layout_add_used(layout, key, data, 6);
	ntdb_layout_add_free(layout, 160, 1);
	key.dsize--;
	ntdb_layout_add_used(layout, key, data, 7);
	ntdb_layout_add_free(layout, 320, 2);
	key.dsize--;
	ntdb_layout_add_used(layout, key, data, 8);
	ntdb_layout_add_free(layout, 40, 0);
	ntdb = ntdb_layout_get(layout, free, &seed);
	ok1(ntdb_check(ntdb, NULL, NULL) == 0);

	off = get_free(ntdb, 0, 80 - sizeof(struct ntdb_used_record), 0,
		       NTDB_USED_MAGIC);
	ok1(off == layout->elem[3].base.off);
	ok1(ntdb->ftable_off == layout->elem[0].base.off);

	off = get_free(ntdb, 0, 160 - sizeof(struct ntdb_used_record), 0,
		       NTDB_USED_MAGIC);
	ok1(off == layout->elem[5].base.off);
	ok1(ntdb->ftable_off == layout->elem[1].base.off);

	off = get_free(ntdb, 0, 320 - sizeof(struct ntdb_used_record), 0,
		       NTDB_USED_MAGIC);
	ok1(off == layout->elem[7].base.off);
	ok1(ntdb->ftable_off == layout->elem[2].base.off);

	off = get_free(ntdb, 0, 40 - sizeof(struct ntdb_used_record), 0,
		       NTDB_USED_MAGIC);
	ok1(off == layout->elem[9].base.off);
	ok1(ntdb->ftable_off == layout->elem[0].base.off);

	/* Now we fail. */
	off = get_free(ntdb, 0, 0, 1, NTDB_USED_MAGIC);
	ok1(off == 0);

	ntdb_close(ntdb);
	ntdb_layout_free(layout);

	ok1(tap_log_messages == 0);
	return exit_status();
}
