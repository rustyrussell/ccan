#include <ccan/talloc/talloc.h>
#include <ccan/str/str.h>
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

	info_c_file = maybe_temp_file(ctx, ".c", false, "_info");
	fd = open(info_c_file, O_WRONLY|O_CREAT|O_EXCL, 0600);
	if (fd < 0)
		return NULL;
	if (!write_all(fd, info, len))
		return NULL;

	if (close(fd) != 0)
		return NULL;

	ccandir = talloc_dirname(ctx, dir);
	*strrchr(ccandir, '/') = '\0';

	compiled = maybe_temp_file(ctx, "", false, "info");
	if (compile_and_link(ctx, info_c_file, ccandir, "",
			     CCAN_COMPILER, CCAN_CFLAGS, "",
			     compiled, &output))
		return compiled;
	return NULL;
}

static char **get_one_deps(const void *ctx, const char *dir,
			   unsigned int *num, char **infofile)
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
	/* FIXME: Do we need num arg? */
	*num = talloc_array_length(deps) - 1;
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
				unsigned int *num,
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
	     char **infofile,
	     char **(*get_one)(const void *, const char *,
			       unsigned int *, char **))
{
	char **deps;
	unsigned int i, num;

	deps = get_one(ctx, dir, &num, infofile);
	for (i = 0; i < num; i++) {
		char **newdeps;
		unsigned int j, newnum;
		char *subinfo = NULL;
		char *subdir;

		if (!strstarts(deps[i], "ccan/"))
			continue;

		subdir = talloc_asprintf(ctx, "%s/%s",
					 talloc_dirname(ctx, dir),
					 deps[i] + strlen("ccan/"));
		newdeps = get_one(ctx, subdir, &newnum, &subinfo);

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

char **get_deps(const void *ctx, const char *dir,
		bool recurse, char **infofile)
{
	char *temp = NULL, **ret;
	if (!infofile)
		infofile = &temp;

	if (!recurse) {
		unsigned int num;
		ret = get_one_deps(ctx, dir, &num, infofile);
	} else
		ret = get_all_deps(ctx, dir, infofile, get_one_deps);

	if (infofile == &temp && temp) {
		unlink(temp);
		talloc_free(temp);
	}
	return ret;
}

char **get_safe_ccan_deps(const void *ctx, const char *dir,
			  bool recurse, char **infofile)
{
	if (!recurse) {
		unsigned int num;
		return get_one_safe_deps(ctx, dir, &num, infofile);
	}
	return get_all_deps(ctx, dir, infofile, get_one_safe_deps);
}
