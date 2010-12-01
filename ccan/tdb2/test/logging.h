#ifndef TDB2_TEST_LOGGING_H
#define TDB2_TEST_LOGGING_H
#include <ccan/tdb2/tdb2.h>
#include <stdbool.h>
#include <string.h>

extern bool suppress_logging;
extern const char *log_prefix;
extern unsigned tap_log_messages;
extern union tdb_attribute tap_log_attr;

void tap_log_fn(struct tdb_context *tdb,
		enum tdb_debug_level level, void *priv,
		const char *message);

static inline bool data_equal(struct tdb_data a, struct tdb_data b)
{
	if (a.dsize != b.dsize)
		return false;
	return memcmp(a.dptr, b.dptr, a.dsize) == 0;
}
#endif /* TDB2_TEST_LOGGING_H */
