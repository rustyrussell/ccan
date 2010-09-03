#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tap/tap.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_used_record rec;
	struct tdb_context tdb = { .log = tap_log_fn, .log_priv = NULL };

	plan_tests(64 + 32 + 48*7 + 1);

	/* We should be able to encode any data value. */
	for (i = 0; i < 64; i++)
		ok1(set_header(&tdb, &rec, 0, 1ULL << i, 1ULL << i, 0, 0)
		    == 0);

	/* And any key and data with < 64 bits between them. */
	for (i = 0; i < 32; i++) {
		tdb_len_t dlen = 1ULL >> (63 - i), klen = 1ULL << i;
		ok1(set_header(&tdb, &rec, klen, dlen, klen + dlen, 0, 0)
		    == 0);
	}

	/* We should neatly encode all values. */
	for (i = 0; i < 48; i++) {
		uint64_t h = 1ULL << (i < 5 ? 63 - i : 63 - 4);
		uint64_t klen = 1ULL << (i < 16 ? i : 15);
		uint64_t dlen = 1ULL << i;
		uint64_t xlen = 1ULL << (i < 32 ? i : 31);
		uint64_t zbits = 1ULL << (i < 6 ? i : 5);
		ok1(set_header(&tdb, &rec, klen, dlen, klen + dlen + xlen, h,
			       zbits)
		    == 0);
		ok1(rec_key_length(&rec) == klen);
		ok1(rec_data_length(&rec) == dlen);
		ok1(rec_extra_padding(&rec) == xlen);
		ok1((uint64_t)rec_hash(&rec) << (64 - 5) == h);
		ok1(rec_zone_bits(&rec) == zbits);
		ok1(rec_magic(&rec) == TDB_MAGIC);
	}
	ok1(tap_log_messages == 0);
	return exit_status();
}
