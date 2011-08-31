#include "tdb1-logging.h"
#include <ccan/tap/tap.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Turn log messages into tap diag messages. */
static void taplog(struct tdb_context *tdb,
		   enum tdb_log_level level,
		   enum TDB_ERROR ecode,
		   const char *message,
		   void *data)
{
	if (suppress_logging)
		return;

	/* Strip trailing \n: diag adds it. */
	if (message[0] && message[strlen(message)-1] == '\n')
		diag("%s%.*s", log_prefix, (unsigned)strlen(message)-1, message);
	else
		diag("%s%s", log_prefix, message);
}

struct tdb1_logging_context taplogctx = { taplog, NULL };
