#include "tools.h"
#include <stdlib.h>
#include <stdarg.h>

const char *gcov; /* = NULL */

bool run_gcov(const void *ctx, unsigned int *time_ms, char **output,
	      const char *fmt, ...)
{
	const char *cmd = gcov;
	char *args;
	va_list ap;
	bool rc;

	if (!gcov) {
#if defined(__clang__)
		cmd = "llvm-cov gcov";
#elif defined(__GNUC__)
		cmd = "gcov";
#endif
	}

	if (!cmd)
		return false;

	va_start(ap, fmt);
	args = tal_vfmt(ctx, fmt, ap);
	rc = run_command(ctx, time_ms, output, "%s %s", cmd, args);
	tal_free(args);
	va_end(ap);
	return rc;
}

const char *gcov_unavailable(void *ctx)
{
	const char *err = NULL;

	/* 
	 * If the user has specified a path, assume they know what
	 * they're doing
	 */
	if (gcov)
		return NULL;

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
