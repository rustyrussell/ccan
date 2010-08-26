#ifndef TDB2_TEST_LOGGING_H
#define TDB2_TEST_LOGGING_H
#include <ccan/tdb2/tdb2.h>
unsigned tap_log_messages;

void tap_log_fn(struct tdb_context *tdb,
		enum tdb_debug_level level, void *priv,
		const char *fmt, ...);

#endif /* TDB2_TEST_LOGGING_H */
