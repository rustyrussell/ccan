#include <ccan/str/str.h>
#include <ccan/talloc/talloc.h>
#include <ccan/grab_file/grab_file.h>
#include <ccan/str_talloc/str_talloc.h>
#include <ccan/read_write_all/read_write_all.h>
#include "tools.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <err.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>

static char ** __attribute__((format(printf, 2, 3)))
lines_from_cmd(const void *ctx, const char *format, ...)
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

	return strsplit(ctx, buffer, "\n");
}

/* Be careful about trying to compile over running programs (parallel make).
 * temp_file helps here. */
static char *compile_info(const void *ctx, const char *dir)
{
	char *info_c_file, *info, *ccandir, *compiled, *output;
	size_t len;
	int fd;

	/* Copy it to a file with proper .c suffix. */
	info = grab_file(ctx, talloc_asprintf(ctx, "%s/_info", dir), &len);
	if (!info)
		return NULL;

	info_c_file = temp_file(ctx, ".c", "_info");
	fd = open(info_c_file, O_WRONLY|O_CREAT|O_EXCL, 0600);
	if (fd < 0)
		return NULL;
	if (!write_all(fd, info, len))
		return NULL;

	if (close(fd) != 0)
		return NULL;

	ccandir = talloc_dirname(ctx, dir);
	if (strrchr(ccandir, '/'))
		*strrchr(ccandir, '/') = '\0';

	compiled = temp_file(ctx, "", "info");
	if (compile_and_link(ctx, info_c_file, ccandir, "",
			     CCAN_COMPILER, CCAN_CFLAGS " -I.", "",
			     compiled, &output))
		return compiled;
	return NULL;
}

static char **get_one_deps(const void *ctx, const char *dir, char **infofile)
{
	char **deps, *cmd;

	if (!*infofile) {
		*infofile = compile_info(ctx, dir);
		if (!*infofile)
			errx(1, "Could not compile _info for '%s'", dir);
	}

	cmd = talloc_asprintf(ctx, "%s depends", *infofile);
	deps = lines_from_cmd(cmd, "%s", cmd);
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
				const char *dir,
				char **infofile)
{
	char **deps, **lines, *raw, *fname;
	unsigned int i, n;

	fname = talloc_asprintf(ctx, "%s/_info", dir);
	raw = grab_file(fname, fname, NULL);
	if (!raw)
		errx(1, "Could not open %s", fname);

	/* Replace \n by actual line breaks, and split it. */
	lines = strsplit(raw, replace(raw, raw, "\\n", "\n"), "\n");

	deps = talloc_array(ctx, char *, talloc_array_length(lines));

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

	/* Make sure talloc_array_length() works */
	return talloc_realloc(NULL, deps, char *, n + 1);
}

static bool have_dep(char **deps, const char *dep)
{
	unsigned int i;

	for (i = 0; deps[i]; i++)
		if (streq(deps[i], dep))
			return true;
	return false;
}



/* Gets all the dependencies, recursively. */
static char **
get_all_deps(const void *ctx, const char *dir,
	     char **infofile,
	     char **(*get_one)(const void *, const char *, char **))
{
	char **deps;
	unsigned int i;

	deps = get_one(ctx, dir, infofile);
	for (i = 0; i < talloc_array_length(deps)-1; i++) {
		char **newdeps;
		unsigned int j;
		char *subinfo = NULL;
		char *subdir;

		if (!strstarts(deps[i], "ccan/"))
			continue;

		subdir = talloc_asprintf(ctx, "%s/%s",
					 talloc_dirname(ctx, dir),
					 deps[i] + strlen("ccan/"));
		newdeps = get_one(ctx, subdir, &subinfo);

		/* Should be short, so brute-force out dups. */
		for (j = 0; j < talloc_array_length(newdeps)-1; j++) {
			unsigned int num;

			if (have_dep(deps, newdeps[j]))
				continue;

			num = talloc_array_length(deps)-1;
			deps = talloc_realloc(NULL, deps, char *, num + 2);
			deps[num] = newdeps[j];
			deps[num+1] = NULL;
		}
	}
	return deps;
}

char **get_libs(const void *ctx, const char *dir,
		unsigned int *num, char **infofile)
{
	char **libs, *cmd;

	if (!*infofile) {
		*infofile = compile_info(ctx, dir);
		if (!*infofile)
			errx(1, "Could not compile _info for '%s'", dir);
	}

	cmd = talloc_asprintf(ctx, "%s libs", *infofile);
	libs = lines_from_cmd(cmd, "%s", cmd);
	if (!libs)
		err(1, "Could not run '%s'", cmd);
	/* FIXME: Do we need num arg? */
	*num = talloc_array_length(libs) - 1;
	return libs;
}

/* FIXME: This is O(n^2), which is dumb. */
static char **uniquify_deps(char **deps)
{
	unsigned int i, j, num;

	num = talloc_array_length(deps) - 1;
	for (i = 0; i < num; i++) {
		for (j = i + 1; j < num; j++) {
			if (streq(deps[i], deps[j])) {
				memmove(&deps[j], &deps[j+1],
					(num - j - 1) * sizeof(char *));
				num--;
			}
		}
	}
	deps[num] = NULL;
	/* Make sure talloc_array_length() works */
	return talloc_realloc(NULL, deps, char *, num + 1);
}

char **get_deps(const void *ctx, const char *dir,
		bool recurse, char **infofile)
{
	char *temp = NULL, **ret;
	if (!infofile)
		infofile = &temp;

	if (!recurse) {
		ret = get_one_deps(ctx, dir, infofile);
	} else
		ret = get_all_deps(ctx, dir, infofile, get_one_deps);

	if (infofile == &temp && temp) {
		unlink(temp);
		talloc_free(temp);
	}
	return uniquify_deps(ret);
}

char **get_safe_ccan_deps(const void *ctx, const char *dir,
			  bool recurse)
{
	char **ret;
	if (!recurse) {
		ret = get_one_safe_deps(ctx, dir, NULL);
	} else {
		ret = get_all_deps(ctx, dir, NULL, get_one_safe_deps);
	}
	return uniquify_deps(ret);
}
