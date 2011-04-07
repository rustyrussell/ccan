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
		enum tdb_log_level level,
		const char *message, void *priv);
#endif /* TDB2_TEST_LOGGING_H */
