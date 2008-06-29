#include "talloc/talloc.h"
#include "tools.h"
#include <err.h>

static char ** __attribute__((format(printf, 2, 3)))
lines_from_cmd(const void *ctx, char *format, ...)
{
	va_list ap;
	char *cmd, *buffer;
	FILE *p;

	va_start(ap, format);
	cmd = talloc_vasprintf(ctx, format, ap);
	va_end(ap);

	p = popen(cmd, "r");
	if (!p)
		err(1, "Executing '%s'", cmd);

	buffer = grab_fd(ctx, fileno(p));
	if (!buffer)
		err(1, "Reading from '%s'", cmd);
	pclose(p);

	return split(ctx, buffer, "\n", NULL);
}

static char *build_info(const void *ctx, const char *dir)
{
	char *file, *cfile, *cmd;

	cfile = talloc_asprintf(ctx, "%s/%s", dir, "_info.c");
	file = talloc_asprintf(cfile, "%s/%s", dir, "_info");
	cmd = talloc_asprintf(file, "gcc " CFLAGS " -o %s %s", file, cfile);
	if (system(cmd) != 0)
		errx(1, "Failed to compile %s", file);

	return file;
}

char **get_deps(const void *ctx, const char *dir)
{
	char **deps, *cmd;

	cmd = talloc_asprintf(ctx, "%s depends", build_info(ctx, dir));
	deps = lines_from_cmd(cmd, cmd);
	if (!deps)
		err(1, "Could not run '%s'", cmd);
	return deps;
}

