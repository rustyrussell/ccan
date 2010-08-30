#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ccan/tap/tap.h>
#include "logging.h"

unsigned tap_log_messages;

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

	va_start(ap, fmt);
	if (vasprintf(&p, fmt, ap) == -1)
		abort();
	diag("tdb log level %u: %s", level, p);
	free(p);
	if (level != TDB_DEBUG_TRACE)
		tap_log_messages++;
	va_end(ap);
}

