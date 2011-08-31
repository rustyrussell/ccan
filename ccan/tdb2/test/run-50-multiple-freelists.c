#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include "logging.h"
#include "layout.h"

int main(int argc, char *argv[])
{
	tdb_off_t off;
	struct tdb_context *tdb;
	struct tdb_layout *layout;
	TDB_DATA key, data;
	union tdb_attribute seed;

	/* This seed value previously tickled a layout.c bug. */
	seed.base.attr = TDB_ATTRIBUTE_SEED;
	seed.seed.seed = 0xb1142bc054d035b4ULL;
	seed.base.next = &tap_log_attr;

	plan_tests(11);
	key = tdb_mkdata("Hello", 5);
	data = tdb_mkdata("world", 5);

	/* Create a TDB with three free tables. */
	layout = new_tdb_layout(NULL);
	tdb_layout_add_freetable(layout);
	tdb_layout_add_freetable(layout);
	tdb_layout_add_freetable(layout);
	tdb_layout_add_free(layout, 80, 0);
	/* Used record prevent coalescing. */
	tdb_layout_add_used(layout, key, data, 6);
	tdb_layout_add_free(layout, 160, 1);
	key.dsize--;
	tdb_layout_add_used(layout, key, data, 7);
	tdb_layout_add_free(layout, 320, 2);
	key.dsize--;
	tdb_layout_add_used(layout, key, data, 8);
	tdb_layout_add_free(layout, 40, 0);
	tdb = tdb_layout_get(layout, &seed);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	off = get_free(tdb, 0, 80 - sizeof(struct tdb_used_record), 0,
		       TDB_USED_MAGIC, 0);
	ok1(off == layout->elem[3].base.off);
	ok1(tdb->tdb2.ftable_off == layout->elem[0].base.off);

	off = get_free(tdb, 0, 160 - sizeof(struct tdb_used_record), 0,
		       TDB_USED_MAGIC, 0);
	ok1(off == layout->elem[5].base.off);
	ok1(tdb->tdb2.ftable_off == layout->elem[1].base.off);

	off = get_free(tdb, 0, 320 - sizeof(struct tdb_used_record), 0,
		       TDB_USED_MAGIC, 0);
	ok1(off == layout->elem[7].base.off);
	ok1(tdb->tdb2.ftable_off == layout->elem[2].base.off);

	off = get_free(tdb, 0, 40 - sizeof(struct tdb_used_record), 0,
		       TDB_USED_MAGIC, 0);
	ok1(off == layout->elem[9].base.off);
	ok1(tdb->tdb2.ftable_off == layout->elem[0].base.off);

	/* Now we fail. */
	off = get_free(tdb, 0, 0, 1, TDB_USED_MAGIC, 0);
	ok1(off == 0);

	tdb_close(tdb);
	tdb_layout_free(layout);

	ok1(tap_log_messages == 0);
	return exit_status();
}
