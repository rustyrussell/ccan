#include <ccan/str/str.h>
#include <ccan/read_write_all/read_write_all.h>
#include <ccan/rbuf/rbuf.h>
#include <ccan/tal/path/path.h>
#include <ccan/tal/grab_file/grab_file.h>
#include <ccan/compiler/compiler.h>
#include <ccan/err/err.h>
#include "tools.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>

static char ** PRINTF_FMT(2, 3)
lines_from_cmd(const void *ctx, const char *format, ...)
{
	va_list ap;
	char *cmd;
	FILE *p;
	struct rbuf in;

	va_start(ap, format);
	cmd = tal_vfmt(ctx, format, ap);
	va_end(ap);

	p = popen(cmd, "r");
	if (!p)
		err(1, "Executing '%s'", cmd);

	/* FIXME: Use rbuf_read_str(&in, '\n') rather than strsplit! */
	rbuf_init(&in, fileno(p), tal_arr(ctx, char, 0), 0);
	if (!rbuf_read_str(&in, 0, do_tal_realloc) && errno)
		err(1, "Reading from '%s'", cmd);
	pclose(p);

	return tal_strsplit(ctx, in.buf, "\n", STR_EMPTY_OK);
}

/* Be careful about trying to compile over running programs (parallel make).
 * temp_file helps here. */
char *compile_info(const void *ctx, const char *dir)
{
	char *info_c_file, *info, *compiled, *output;
	int fd;

	/* Copy it to a file with proper .c suffix. */
	info = grab_file(ctx, tal_fmt(ctx, "%s/_info", dir));
	if (!info)
		return NULL;

	info_c_file = temp_file(ctx, ".c", "_info");
	fd = open(info_c_file, O_WRONLY|O_CREAT|O_EXCL, 0600);
	if (fd < 0)
		return NULL;
	if (!write_all(fd, info, tal_count(info)-1)) {
		close(fd);
		return NULL;
	}

	if (close(fd) != 0)
		return NULL;

	compiled = temp_file(ctx, "", "info");
	if (compile_and_link(ctx, info_c_file, find_ccan_dir(dir), "",
			     compiler, cflags, "", compiled, &output))
		return compiled;
	return NULL;
}

static char **get_one_deps(const void *ctx, const char *dir, const char *style,
			   char *(*get_info)(const void *ctx, const char *dir))
{
	char **deps, *cmd;
	char *infofile = get_info(ctx, dir);

	if (!infofile)
		return NULL;

	cmd = tal_fmt(ctx, "%s %s", infofile, style);
	deps = lines_from_cmd(cmd, "%s", cmd);
	if (!deps) {
		/* You must understand depends, maybe not testdepends. */
		if (streq(style, "depends"))
			err(1, "Could not run '%s'", cmd);
		deps = tal(ctx, char *);
		deps[0] = NULL;
	}
	return deps;
}

/* Make copy of src, replacing "from" with "to". */
static char *replace(const void *ctx, const char *src,
		     const char *from, const char *to)
{
	char *ret = tal_strdup(ctx, "");
	unsigned int rlen, len, add;

	rlen = len = 0;
	for (;;) {
		const char *next = strstr(src+len, from);
		if (!next)
			add = strlen(src+len) + 1;
		else
			add = next - (src+len);

		tal_resize(&ret, rlen + add + strlen(to)+1);
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
				const char *style,
				char *(*unused)(const void *, const char *))
{
	char **deps, **lines, *raw, *fname;
	unsigned int i, n;
	bool correct_style = false;

	fname = path_join(ctx, dir, "_info");
	raw = grab_file(fname, fname);
	if (!raw)
		errx(1, "Could not open %s", fname);

	/* Replace \n by actual line breaks, and split it. */
	lines = tal_strsplit(raw, replace(raw, raw, "\\n", "\n"), "\n",
			     STR_EMPTY_OK);

	deps = tal_arr(ctx, char *, tal_count(lines));

	for (n = i = 0; lines[i]; i++) {
		char *str;
		unsigned int len;

		/* Ignore lines starting with # (e.g. #include) */
		if (lines[i][0] == '#')
			continue;

		if (strstr(lines[i], "\"testdepends\""))
			correct_style = streq(style, "testdepends");
		else if (strstr(lines[i], "\"depends\""))
			correct_style = streq(style, "depends");

		if (!correct_style)
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
		deps[n++] = tal_strndup(deps, str, len);
	}
	deps[n] = NULL;
	tal_free(fname);

	/* Make sure tal_array_length() works */
	tal_resize(&deps, n + 1);
	return deps;
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
get_all_deps(const void *ctx, const char *dir, const char *style,
	     char *(*get_info)(const void *ctx, const char *dir),
	     char **(*get_one)(const void *, const char *, const char *,
			       char *(*get_info)(const void *, const char *)))
{
	char **deps;
	unsigned int i;

	deps = get_one(ctx, dir, style, get_info);
	if (!deps)
		return NULL;
	for (i = 0; i < tal_count(deps)-1; i++) {
		char **newdeps;
		unsigned int j;
		char *subdir;

		if (!strstarts(deps[i], "ccan/"))
			continue;

		subdir = path_join(ctx, find_ccan_dir(dir), deps[i]);
		newdeps = get_one(ctx, subdir, "depends", get_info);

		/* Should be short, so brute-force out dups. */
		for (j = 0; j < tal_count(newdeps)-1; j++) {
			unsigned int num;

			if (have_dep(deps, newdeps[j]))
				continue;

			num = tal_count(deps)-1;
			tal_resize(&deps, num + 2);
			deps[num] = newdeps[j];
			deps[num+1] = NULL;
		}
	}
	return deps;
}

/* Can return NULL: _info may not support prop. */
static char **get_one_prop(const void *ctx, const char *dir, const char *prop,
			   char *(*get_info)(const void *ctx, const char *dir))
{
	char *cmd, **lines;

	cmd = tal_fmt(ctx, "%s %s", get_info(ctx, dir), prop);
	lines = lines_from_cmd(cmd, "%s", cmd);
	/* Strip final NULL. */
	if (lines)
		tal_resize(&lines, tal_count(lines)-1);
	return lines;
}

static char **get_one_libs(const void *ctx, const char *dir,
			   char *(*get_info)(const void *ctx, const char *dir))
{
	return get_one_prop(ctx, dir, "libs", get_info);
}

static char **get_one_cflags(const void *ctx, const char *dir,
			   char *(*get_info)(const void *ctx, const char *dir))
{
	return get_one_prop(ctx, dir, "cflags", get_info);
}

static char **get_one_ccanlint(const void *ctx, const char *dir,
			       char *(*get_info)(const void *ctx, const char *dir))
{
	return get_one_prop(ctx, dir, "ccanlint", get_info);
}

/* O(n^2) but n is small. */
static char **add_deps(char **deps1, char **deps2)
{
	unsigned int i, len;

	len = tal_count(deps1);

	for (i = 0; deps2[i]; i++) {
		if (have_dep(deps1, deps2[i]))
			continue;
		tal_resize(&deps1, len + 1);
		deps1[len-1] = tal_strdup(deps1, deps2[i]);
		deps1[len++] = NULL;
	}
	return deps1;
}

char **get_cflags(const void *ctx, const char *dir,
        char *(*get_info)(const void *ctx, const char *dir))
{
	char **flags;
	unsigned int len;
	flags = get_one_cflags(ctx, dir, get_info);
	len = tal_count(flags);
	tal_resize(&flags, len + 1);
	flags[len] = NULL;
	return flags;
}

char **get_ccanlint(const void *ctx, const char *dir,
		    char *(*get_info)(const void *ctx, const char *dir))
{
	char **ccanlint;
	unsigned int len;
	ccanlint = get_one_ccanlint(ctx, dir, get_info);
	len = tal_count(ccanlint);
	tal_resize(&ccanlint, len + 1);
	ccanlint[len] = NULL;
	return ccanlint;
}

static char *get_one_ported(const void *ctx, const char *dir,
			    char *(*get_info)(const void *ctx, const char *dir))
{
	char **ported = get_one_prop(ctx, dir, "ported", get_info);

	/* No news is good news. */
	if (tal_count(ported) == 0)
		return NULL;

	if (tal_count(ported) != 1)
		errx(1, "%s/_info ported gave %zu lines, not one",
		     dir, tal_count(ported));

	if (streq(ported[0], ""))
		return NULL;
	else
		return ported[0];
}

char *get_ported(const void *ctx, const char *dir, bool recurse,
		char *(*get_info)(const void *ctx, const char *dir))
{
	char *msg;

	msg = get_one_ported(ctx, dir, get_info);
	if (msg)
		return msg;

	if (recurse) {
		size_t i;
		char **deps = get_deps(ctx, dir, "depends", true, get_info);
		for (i = 0; deps[i]; i++) {
			char *subdir;
			if (!strstarts(deps[i], "ccan/"))
				continue;

			subdir = path_join(ctx, find_ccan_dir(dir), deps[i]);
			msg = get_one_ported(ctx, subdir, get_info);
			if (msg)
				return msg;
		}
	}
	return NULL;
}

char **get_libs(const void *ctx, const char *dir, const char *style,
		char *(*get_info)(const void *ctx, const char *dir))
{
	char **deps, **libs;
	unsigned int i, len;

	libs = get_one_libs(ctx, dir, get_info);
	len = tal_count(libs);

	if (style) {
		deps = get_deps(ctx, dir, style, true, get_info);
		if (streq(style, "testdepends"))
			deps = add_deps(deps,
					get_deps(ctx, dir, "depends", true,
						 get_info));

		for (i = 0; deps[i]; i++) {
			char **newlibs, *subdir;
			size_t newlen;

			if (!strstarts(deps[i], "ccan/"))
				continue;

			subdir = path_join(ctx, find_ccan_dir(dir), deps[i]);

			newlibs = get_one_libs(ctx, subdir, get_info);
			newlen = tal_count(newlibs);
			tal_resize(&libs, len + newlen);
			memcpy(&libs[len], newlibs,
			       sizeof(newlibs[0])*newlen);
			len += newlen;
		}
	}

	/* Append NULL entry. */
	tal_resize(&libs, len + 1);
	libs[len] = NULL;
	return libs;
}

/* FIXME: This is O(n^2), which is dumb. */
static char **uniquify_deps(char **deps)
{
	unsigned int i, j, num;

	if (!deps)
		return NULL;

	num = tal_count(deps) - 1;
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
	/* Make sure tal_count() works */
	tal_resize(&deps, num + 1);
	return deps;
}

char **get_deps(const void *ctx, const char *dir, const char *style,
		bool recurse,
		char *(*get_info)(const void *ctx, const char *dir))
{
	char **ret;

	if (!recurse) {
		ret = get_one_deps(ctx, dir, style, get_info);
	} else
		ret = get_all_deps(ctx, dir, style, get_info, get_one_deps);

	return uniquify_deps(ret);
}

char **get_safe_ccan_deps(const void *ctx, const char *dir, const char *style,
			  bool recurse)
{
	char **ret;
	if (!recurse) {
		ret = get_one_safe_deps(ctx, dir, style, NULL);
	} else {
		ret = get_all_deps(ctx, dir, style, NULL, get_one_safe_deps);
	}
	return uniquify_deps(ret);
}
