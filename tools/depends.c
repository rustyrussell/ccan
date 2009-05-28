#include <ccan/talloc/talloc.h>
#include <ccan/str/str.h>
#include <ccan/grab_file/grab_file.h>
#include <ccan/str_talloc/str_talloc.h>
#include "tools.h"
#include <err.h>
#include <stdbool.h>
#include <unistd.h>

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

	buffer = grab_fd(ctx, fileno(p), NULL);
	if (!buffer)
		err(1, "Reading from '%s'", cmd);
	pclose(p);

	return strsplit(ctx, buffer, "\n", num);
}

static int unlink_info(char *infofile)
{
	unlink(infofile);
	return 0;
}

/* Be careful about trying to compile over running programs (parallel make) */
static char *compile_info(const void *ctx, const char *dir)
{
	char *infofile = talloc_asprintf(ctx, "%s/_info.%u", dir, getpid());
	char *cmd = talloc_asprintf(ctx, "cc " CFLAGS " -o %s %s/_info.c",
				    infofile, dir);
	talloc_set_destructor(infofile, unlink_info);
	if (system(cmd) != 0)
		return NULL;

	return infofile;
}

static char **get_one_deps(const void *ctx, const char *dir, unsigned int *num)
{
	char **deps, *cmd, *infofile;

	infofile = compile_info(ctx, dir);
	if (!infofile)
		errx(1, "Could not compile _info for '%s'", dir);

	cmd = talloc_asprintf(ctx, "%s depends", infofile);
	deps = lines_from_cmd(cmd, num, "%s", cmd);
	if (!deps)
		err(1, "Could not run '%s'", cmd);
	return deps;
}

/* Make copy of src, replacing "from" with "to". */
static char *replace(const void *ctx, const char *src,
		     const char *from, const char *to)
{
	char *ret = talloc_strdup(ctx, "");
	unsigned int rlen, len, add;

	rlen = len = 0;
	for (;;) {
		const char *next = strstr(src+len, from);
		if (!next)
			add = strlen(src+len) + 1;
		else
			add = next - (src+len);

		ret = talloc_realloc(ctx, ret, char, rlen + add + strlen(to)+1);
		memcpy(ret+rlen, src+len, add);
		if (!next)
			return ret;
		len += add;
		rlen += add;
		strcpy(ret+rlen, to);
		rlen += strlen(to);
		len += strlen(from);
	}
}

/* This is a terrible hack.  We scan for ccan/ strings. */
static char **get_one_safe_deps(const void *ctx,
				const char *dir, unsigned int *num)
{
	char **deps, **lines, *raw, *fname;
	unsigned int i, n = 0;

	fname = talloc_asprintf(ctx, "%s/_info.c", dir);
	raw = grab_file(fname, fname, NULL);
	if (!raw)
		errx(1, "Could not open %s", fname);

	/* Replace \n by actual line breaks, and split it. */
	lines = strsplit(raw, replace(raw, raw, "\\n", "\n"), "\n", &n);

	deps = talloc_array(ctx, char *, n+1);

	for (n = i = 0; lines[i]; i++) {
		char *str;
		unsigned int len;

		/* Ignore lines starting with # (e.g. #include) */
		if (lines[i][0] == '#')
			continue;

		/* Start of line, or after ". */
		if (strstarts(lines[i], "ccan/"))
			str = lines[i];
		else {
			str = strstr(lines[i], "\"ccan/");
			if (!str)
				continue;
			str++;
		}
		
		len = strspn(str, "/abcdefghijklmnopqrstuvxwyz12345678980_");
		if (len == 5)
			continue;
		deps[n++] = talloc_strndup(deps, str, len);
	}
	deps[n] = NULL;
	talloc_free(fname);
	if (num)
		*num = n;
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
static char **
get_all_deps(const void *ctx, const char *dir,
	     char **(*get_one)(const void *, const char *, unsigned int *))
{
	char **deps;
	unsigned int i, num;

	deps = get_one(ctx, dir, &num);
	for (i = 0; i < num; i++) {
		char **newdeps;
		unsigned int j, newnum;

		if (!strstarts(deps[i], "ccan/"))
			continue;

		newdeps = get_one(ctx, deps[i], &newnum);

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

char **get_deps(const void *ctx, const char *dir, bool recurse)
{
	if (!recurse) {
		unsigned int num;
		return get_one_deps(ctx, dir, &num);
	}
	return get_all_deps(ctx, dir, get_one_deps);
}

char **get_safe_ccan_deps(const void *ctx, const char *dir, bool recurse)
{
	if (!recurse) {
		unsigned int num;
		return get_one_safe_deps(ctx, dir, &num);
	}
	return get_all_deps(ctx, dir, get_one_safe_deps);
}
	
