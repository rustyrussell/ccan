#include "tools.h"
#include <stdlib.h>
#include <stdarg.h>

bool run_gcov(const void *ctx, unsigned int *time_ms, char **output,
	      const char *fmt, ...)
{
	char *args;
	va_list ap;
	bool rc;

	va_start(ap, fmt);
	args = tal_vfmt(ctx, fmt, ap);
	rc = run_command(ctx, time_ms, output, "gcov %s", args);
	tal_free(args);
	return rc;
}

const char *gcov_unavailable(void *ctx)
{
	const char *err = NULL;

#ifdef __GNUC__
	unsigned int timeleft = default_timeout_ms;
	char *output;

	if (!run_gcov(ctx, &timeleft, &output, "-h")) {
		err = tal_fmt(ctx, "No gcov support: %s", output);
		tal_free(output);
	}
#else
	err = "No coverage support for this compiler";
#endif

	return err;
}
