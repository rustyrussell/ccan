#include "talloc/talloc.h"
#include "string/string.h"
#include "tools.h"
#include <err.h>
#include <stdbool.h>

static char ** __attribute__((format(printf, 3, 4)))
lines_from_cmd(const void *ctx, unsigned int *num, char *format, ...)
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

	return split(ctx, buffer, "\n", num);
}

static char **get_one_deps(const void *ctx, const char *dir, unsigned int *num)
{
	char **deps, *cmd;

	cmd = talloc_asprintf(ctx, "%s/_info depends", dir);
	deps = lines_from_cmd(cmd, num, "%s", cmd);
	if (!deps)
		err(1, "Could not run '%s'", cmd);
	return deps;
}

static bool have_dep(char **deps, unsigned int num, const char *dep)
{
	unsigned int i;

	for (i = 0; i < num; i++)
		if (streq(deps[i], dep))
			return true;
	return false;
}

/* Gets all the dependencies, recursively. */
char **get_deps(const void *ctx, const char *dir)
{
	char **deps;
	unsigned int i, num;

	deps = get_one_deps(ctx, dir, &num);
	for (i = 0; i < num; i++) {
		char **newdeps;
		unsigned int j, newnum;

		if (!strstarts(deps[i], "ccan/"))
			continue;

		newdeps = get_one_deps(ctx, deps[i], &newnum);

		/* Should be short, so brute-force out dups. */
		for (j = 0; j < newnum; j++) {
			if (have_dep(deps, num, newdeps[j]))
				continue;

			deps = talloc_realloc(NULL, deps, char *, num + 2);
			deps[num++] = newdeps[j];
			deps[num] = NULL;
		}
	}
	return deps;
}
