#include <stdio.h>
#include <stdlib.h>
#include "tap-interface.h"
#include "logging.h"

unsigned tap_log_messages;
const char *log_prefix = "";
char *log_last = NULL;
bool suppress_logging;

union ntdb_attribute tap_log_attr = {
	.log = { .base = { .attr = NTDB_ATTRIBUTE_LOG },
		 .fn = tap_log_fn }
};

void tap_log_fn(struct ntdb_context *ntdb,
		enum ntdb_log_level level,
		enum NTDB_ERROR ecode,
		const char *message, void *priv)
{
	if (suppress_logging)
		return;

	diag("ntdb log level %u: %s: %s%s",
	     level, ntdb_errorstr(ecode), log_prefix, message);
	if (log_last)
		free(log_last);
	log_last = strdup(message);
	tap_log_messages++;
}
