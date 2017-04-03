#ifndef NTDB_TEST_LOGGING_H
#define NTDB_TEST_LOGGING_H
#include "../ntdb.h"
#include <stdbool.h>
#include <string.h>

extern bool suppress_logging;
extern const char *log_prefix;
extern unsigned tap_log_messages;
extern union ntdb_attribute tap_log_attr;
extern char *log_last;

void tap_log_fn(struct ntdb_context *ntdb,
		enum ntdb_log_level level,
		enum NTDB_ERROR ecode,
		const char *message, void *priv);
#endif /* NTDB_TEST_LOGGING_H */
