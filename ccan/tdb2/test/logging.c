#include <stdio.h>
#include <stdlib.h>
#include <ccan/tap/tap.h>
#include "logging.h"

unsigned tap_log_messages;
const char *log_prefix = "";
bool suppress_logging;

union tdb_attribute tap_log_attr = {
	.log = { .base = { .attr = TDB_ATTRIBUTE_LOG },
		 .log_fn = tap_log_fn }
};

void tap_log_fn(struct tdb_context *tdb,
		enum tdb_log_level level, void *priv,
		const char *message)
{
	if (suppress_logging)
		return;

	diag("tdb log level %u: %s%s", level, log_prefix, message);
	tap_log_messages++;
}

