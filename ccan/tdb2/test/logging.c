#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
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
		enum tdb_debug_level level, void *priv,
		const char *fmt, ...)
{
	va_list ap;
	char *p;

	if (suppress_logging)
		return;

	va_start(ap, fmt);
	if (vasprintf(&p, fmt, ap) == -1)
		abort();
	/* Strip trailing \n: diag adds it. */
	if (p[strlen(p)-1] == '\n')
		p[strlen(p)-1] = '\0';
	diag("tdb log level %u: %s%s", level, log_prefix, p);
	free(p);
	if (level != TDB_DEBUG_TRACE)
		tap_log_messages++;
	va_end(ap);
}

