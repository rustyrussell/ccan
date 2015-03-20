#include "ntdb-source.h"
#include "tap-interface.h"
#include "logging.h"
#include "helprun-external-agent.h"

int main(int argc, char *argv[])
{
	unsigned int i, j;
	struct ntdb_context *ntdb;
	int flags[] = { NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_CONVERT, NTDB_NOMMAP|NTDB_CONVERT };
	NTDB_DATA key = { (unsigned char *)&j, sizeof(j) };
	NTDB_DATA data = { (unsigned char *)&j, sizeof(j) };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 8 + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		uint64_t features;
		ntdb = ntdb_open("run-features.ntdb", flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		/* Put some stuff in there. */
		for (j = 0; j < 100; j++) {
			if (ntdb_store(ntdb, key, data, NTDB_REPLACE) != 0)
				fail("Storing in ntdb");
		}

		/* Mess with features fields in hdr. */
		features = (~NTDB_FEATURE_MASK ^ 1);
		ok1(ntdb_write_convert(ntdb, offsetof(struct ntdb_header,
						    features_used),
				      &features, sizeof(features)) == 0);
		ok1(ntdb_write_convert(ntdb, offsetof(struct ntdb_header,
						    features_offered),
				      &features, sizeof(features)) == 0);
		ntdb_close(ntdb);

		ntdb = ntdb_open("run-features.ntdb", flags[i]|MAYBE_NOSYNC,
				 O_RDWR, 0, &tap_log_attr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		/* Should not have changed features offered. */
		ok1(ntdb_read_convert(ntdb, offsetof(struct ntdb_header,
						   features_offered),
				     &features, sizeof(features)) == 0);
		ok1(features == (~NTDB_FEATURE_MASK ^ 1));

		/* Should have cleared unknown bits in features_used. */
		ok1(ntdb_read_convert(ntdb, offsetof(struct ntdb_header,
						   features_used),
				     &features, sizeof(features)) == 0);
		ok1(features == (1 & NTDB_FEATURE_MASK));

		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
