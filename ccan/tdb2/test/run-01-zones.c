#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/check.c>
#include <ccan/tap/tap.h>
#include "logging.h"
#include "layout.h"

/* Calculate start of zone offset from layout directly. */
static tdb_off_t layout_zone_off(tdb_off_t off, struct tdb_layout *layout)
{
	unsigned int i;

	/* Every second one is a free entry, so divide by 2 to get zone */
	for (i = 0; i < layout->num_elems; i++) {
		if (layout->elem[i].base.type != ZONE)
			continue;
		if (layout->elem[i].base.off
		    + (1ULL << layout->elem[i].zone.zone_bits) > off)
			return layout->elem[i].base.off;
	}
	abort();
}

int main(int argc, char *argv[])
{
	struct tdb_context *tdb;
	struct tdb_layout *layout;
	struct free_zone_header zhdr;
	tdb_off_t off, step;
	unsigned int i;

	/* FIXME: Test TDB_CONVERT */

	plan_tests(3 + 100);

	/* No coalescing can be done due to EOF */
	layout = new_tdb_layout(NULL);
	tdb_layout_add_zone(layout, INITIAL_ZONE_BITS, false);
	tdb_layout_add_zone(layout, INITIAL_ZONE_BITS, true);
	tdb_layout_add_zone(layout, INITIAL_ZONE_BITS+1, true);
	tdb_layout_add_zone(layout, INITIAL_ZONE_BITS+2, true);
	tdb_layout_add_zone(layout, INITIAL_ZONE_BITS+2, true);
	tdb = tdb_layout_get(layout);

	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* Last zone should get right zone. */
	ok1(last_zone(tdb, &zhdr)
	    == layout->elem[layout->num_elems-1].base.off);
	ok1(zhdr.zone_bits == INITIAL_ZONE_BITS+2);

	off = sizeof(struct tdb_header);
	step = (tdb->map_size - 1 - off) / 100;
	for (i = 0; i < 100; i++, off += step) {
		ok1(off_to_zone(tdb, off, &zhdr) == layout_zone_off(off, layout));
	}

	return exit_status();
}
