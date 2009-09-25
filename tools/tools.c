#include <ccan/talloc/talloc.h>
#include <ccan/grab_file/grab_file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <err.h>
#include "tools.h"

static char *tmpdir = NULL;
static unsigned int count;

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

/* Returns output if command fails. */
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

static int unlink_all(char *dir)
{
	char cmd[strlen(dir) + sizeof("rm -rf ")];
	sprintf(cmd, "rm -rf %s", dir);
//	system(cmd);
	return 0;
}

char *temp_file(const void *ctx, const char *extension)
{
	/* For first call, create dir. */
	while (!tmpdir) {
		tmpdir = getenv("TMPDIR");
		if (!tmpdir)
			tmpdir = "/tmp";
		tmpdir = talloc_asprintf(talloc_autofree_context(),
					 "%s/ccanlint-%u.%lu",
					 tmpdir, getpid(), random());
		if (mkdir(tmpdir, 0700) != 0) {
			if (errno == EEXIST) {
				talloc_free(tmpdir);
				tmpdir = NULL;
				continue;
			}
			err(1, "mkdir %s failed", tmpdir);
		}
		talloc_set_destructor(tmpdir, unlink_all);
	}

	return talloc_asprintf(ctx, "%s/%u%s", tmpdir, count++, extension);
}
