#include "logging.h"
#include <ccan/tap/tap.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

bool suppress_logging = false;
const char *log_prefix = "";

/* Turn log messages into tap diag messages. */
static void taplog(struct tdb_context *tdb,
		   enum tdb_debug_level level,
		   const char *fmt, ...)
{
	va_list ap;
	char line[200];

	if (suppress_logging)
		return;

	va_start(ap, fmt);
	vsprintf(line, fmt, ap);
	va_end(ap);

	/* Strip trailing \n: diag adds it. */
	if (line[0] && line[strlen(line)-1] == '\n')
		diag("%s%.*s", log_prefix, (unsigned)strlen(line)-1, line);
	else
		diag("%s%s", log_prefix, line);
}

struct tdb_logging_context taplogctx = { taplog, NULL };
