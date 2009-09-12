#include <ccan/talloc/talloc.h>
#include <ccan/grab_file/grab_file.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include "tools.h"

char *talloc_basename(const void *ctx, const char *dir)
{
	char *p = strrchr(dir, '/');

	if (!p)
		return (char *)dir;
	return talloc_strdup(ctx, p+1);
}

char *talloc_dirname(const void *ctx, const char *dir)
{
	char *p = strrchr(dir, '/');

	if (!p)
		return talloc_strdup(ctx, ".");
	return talloc_strndup(ctx, dir, p - dir);
}

char *talloc_getcwd(const void *ctx)
{
	unsigned int len;
	char *cwd;

	/* *This* is why people hate C. */
	len = 32;
	cwd = talloc_array(ctx, char, len);
	while (!getcwd(cwd, len)) {
		if (errno != ERANGE) {
			talloc_free(cwd);
			return NULL;
		}
		cwd = talloc_realloc(ctx, cwd, char, len *= 2);
	}
	return cwd;
}

char *run_command(const void *ctx, const char *fmt, ...)
{
	va_list ap;
	char *cmd, *contents;
	FILE *pipe;

	va_start(ap, fmt);
	cmd = talloc_vasprintf(ctx, fmt, ap);
	va_end(ap);

	/* Ensure stderr gets to us too. */
	cmd = talloc_asprintf_append(cmd, " 2>&1");
	
	pipe = popen(cmd, "r");
	if (!pipe)
		return talloc_asprintf(ctx, "Failed to run '%s'", cmd);

	contents = grab_fd(cmd, fileno(pipe), NULL);
	if (pclose(pipe) != 0)
		return talloc_asprintf(ctx, "Running '%s':\n%s",
				       cmd, contents);

	talloc_free(cmd);
	return NULL;
}
