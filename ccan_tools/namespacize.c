/* Code to move a ccan module into the ccan_ namespace. */
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>
#include "talloc/talloc.h"

#define CFLAGS "-O3 -Wall -Wundef -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wmissing-declarations -Werror -I. -Iccan_tools/libtap/src/"
#define CFLAGS_HDR "-Wall -Wundef -Wstrict-prototypes -Wold-style-definition -Werror -I."

static bool verbose = false;
static int indent = 0;
#define verbose(args...)						\
	do { if (verbose) {						\
			unsigned int _i;				\
			for (_i = 0; _i < indent; _i++) printf(" ");	\
			printf(args);					\
		}							\
	} while(0)
#define verbose_indent() (indent += 2)
#define verbose_unindent() (indent -= 2)

#define streq(a,b) (strcmp((a),(b)) == 0)

#define strstarts(str,prefix) (strncmp((str),(prefix),strlen(prefix)) == 0)

static inline bool strends(const char *str, const char *postfix)
{
	if (strlen(str) < strlen(postfix))
		return false;

	return streq(str + strlen(str) - strlen(postfix), postfix);
}

static int close_no_errno(int fd)
{
	int ret = 0, serrno = errno;
	if (close(fd) < 0)
		ret = errno;
	errno = serrno;
	return ret;
}

static int unlink_no_errno(const char *filename)
{
	int ret = 0, serrno = errno;
	if (unlink(filename) < 0)
		ret = errno;
	errno = serrno;
	return ret;
}

static void *grab_fd(const void *ctx, int fd)
{
	int ret;
	unsigned int max = 16384, size = 0;
	char *buffer;

	buffer = talloc_array(ctx, char, max+1);
	while ((ret = read(fd, buffer + size, max - size)) > 0) {
		size += ret;
		if (size == max)
			buffer = talloc_realloc(ctx, buffer, char, max*=2 + 1);
	}
	if (ret < 0) {
		talloc_free(buffer);
		buffer = NULL;
	} else
		buffer[size] = '\0';

	return buffer;
}

/* This version adds one byte (for nul term) */
static void *grab_file(const void *ctx, const char *filename)
{
	int fd;
	char *buffer;

	if (streq(filename, "-"))
		fd = dup(STDIN_FILENO);
	else
		fd = open(filename, O_RDONLY, 0);

	if (fd < 0)
		return NULL;

	buffer = grab_fd(ctx, fd);
	close_no_errno(fd);
	return buffer;
}

/* This is a dumb one which copies.  We could mangle instead. */
static char **split(const void *ctx, const char *text, const char *delims,
		    unsigned int *nump)
{
	char **lines = NULL;
	unsigned int max = 64, num = 0;

	lines = talloc_array(ctx, char *, max+1);

	while (*text != '\0') {
		unsigned int len = strcspn(text, delims);
		lines[num] = talloc_array(lines, char, len + 1);
		memcpy(lines[num], text, len);
		lines[num][len] = '\0';
		text += len;
		text += strspn(text, delims);
		if (++num == max)
			lines = talloc_realloc(ctx, lines, char *, max*=2 + 1);
	}
	lines[num] = NULL;
	if (nump)
		*nump = num;
	return lines;
}

static char **get_dir(const char *dir)
{
	DIR *d;
	struct dirent *ent;
	char **names = NULL;
	unsigned int size = 0;

	d = opendir(dir);
	if (!d)
		return NULL;

	while ((ent = readdir(d)) != NULL) {
		names = talloc_realloc(dir, names, char *, size + 2);
		names[size++]
			= talloc_asprintf(names, "%s/%s", dir, ent->d_name);
	}
	names[size++] = NULL;
	closedir(d);
	return names;
}

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

static char *build_obj(const char *cfile)
{
	char *cmd;
	char *ofile = talloc_strdup(cfile, cfile);

	ofile[strlen(ofile)-1] = 'c';

	cmd = talloc_asprintf(ofile, "gcc " CFLAGS " -o %s -c %s",
			      ofile, cfile);
	if (system(cmd) != 0)
		errx(1, "Failed to compile %s", cfile);
	return ofile;
}

struct replace
{
	struct replace *next;
	char *string;
};

static void __attribute__((noreturn)) usage(void)
{
	errx(1, "Usage:\n"
	     "namespacize [--verbose] <dir>\n"
	     "namespacize [--verbose] --adjust <dir>...\n"
	     "The first form converts dir/ to insert 'ccan_' prefixes, and\n"
	     "then adjusts any other ccan directories at the same level which\n"
	     "are effected.\n"
	     "--adjust does an adjustment for each directory, in case a\n"
	     "dependency has been namespacized\n");
}

static void add_replace(struct replace **repl, const char *str)
{
	struct replace *new;

	/* Don't replace things already CCAN-ized (eg. idempotent wrappers) */
	if (strstarts(str, "CCAN_") || strstarts(str, "ccan_"))
		return;

	new = talloc(*repl, struct replace);
	new->next = *repl;
	new->string = talloc_strdup(new, str);
	*repl = new;
}

static char *basename(const void *ctx, const char *dir)
{
	char *p = strrchr(dir, '/');

	if (!p)
		return (char *)dir;
	return talloc_strdup(ctx, p+1);
}

/* FIXME: Only does main header, should chase local includes. */ 
static void analyze_headers(const char *dir, struct replace **repl)
{
	char *hdr, *contents, *p;
	enum { LINESTART, HASH, DEFINE, NONE } state = LINESTART;

	/* Get hold of header, assume that's it. */
	hdr = talloc_asprintf(dir, "%s/%s.h", dir, basename(dir, dir));
	contents = grab_file(dir, hdr);
	if (!contents)
		err(1, "Reading %s", hdr);

	verbose("Looking in %s\n", hdr);
	verbose_indent();
	/* Look for lines of form #define X */
	for (p = contents; *p; p++) {
		if (*p == '\n')
			state = LINESTART;
		else if (!isspace(*p)) {
			if (state == LINESTART && *p == '#')
				state = HASH;
			else if (state==HASH && !strncmp(p, "define", 6)) {
				state = DEFINE;
				p += 5;
			} else if (state == DEFINE) {
				unsigned int len;

				len = strspn(p, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
					     "abcdefghijklmnopqrstuvwxyz"
					     "01234567889_");
				if (len) {
					char *s;
					s = talloc_strndup(contents, p, len);
					verbose("Found %s\n", s);
					add_replace(repl, s);
				}
				state = NONE;
			} else
				state = NONE;
		}
	}
	verbose_unindent();
}

static void add_extern_symbols(const char *ofile, struct replace **repl)
{
	/* Should actually read the elf: this is a hack. */
	char **line;

	line = lines_from_cmd(ofile, "nm --defined-only --extern %s", ofile);

	/* nm output is of form [hexaddr] [char] [name]\n */
	for (; *line; line++) {
		unsigned int cols;
		char **names = split(ofile, *line, " \t", &cols);
		if (cols != 3)
			errx(1, "Unexpected nm line '%s' (%i cols)", *line, cols);

		verbose("Found %s\n", names[2]);
		add_replace(repl, names[2]);
	}
}

static void get_header_symbols(const char *dir, struct replace **repl)
{
	char *cmd;
	char *hfile = talloc_asprintf(dir, "%s/%s.h", dir, basename(dir, dir));
	char *ofile = talloc_asprintf(dir, "%s.o", hfile);

	/* Horrible hack to get static inlines. */
	cmd = talloc_asprintf(dir, "gcc " CFLAGS_HDR
			      " -Dstatic= -include %s -o %s -c -x c /dev/null",
			      hfile, ofile);
	if (system(cmd) != 0)
		errx(1, "Failed to compile %s", hfile);

	add_extern_symbols(ofile, repl);
}

/* FIXME: Better to analyse headers in more depth, rather than recompile. */
static void get_exposed_symbols(const char *dir, struct replace **repl)
{
	char **files;
	unsigned int i;

	files = get_dir(dir);
	for (i = 0; files[i]; i++) {
		char *ofile;
		if (!strends(files[i], ".c") || strends(files[i], "/_info.c"))
			continue;

		/* This produces file.c -> file.o */
		ofile = build_obj(files[i]);
		verbose("Looking in %s\n", ofile);
		verbose_indent();
		add_extern_symbols(ofile, repl);
		unlink(ofile);
		verbose_unindent();
	}
	get_header_symbols(dir, repl);
}

static void write_replacement_file(const char *dir, struct replace **repl)
{
	char *replname = talloc_asprintf(dir, "%s/.namespacize", dir);
	int fd;
	struct replace *r;

	fd = open(replname, O_WRONLY|O_CREAT|O_EXCL, 0644);
	if (fd < 0) {
		if (errno == EEXIST)
			errx(1, "%s already exists: can't namespacize twice",
			     replname);
		err(1, "Opening %s", replname);
	}

	for (r = *repl; r; r = r->next) {
		if (write(fd,r->string,strlen(r->string)) != strlen(r->string)
		    || write(fd, "\n", 1) != 1) {
			unlink_no_errno(replname);
			if (errno == 0)
				errx(1, "Short write to %s: disk full?",
				     replname);
			errx(1, "Writing to %s", replname);
		}
	}

	close(fd);
}

static int unlink_destroy(char *name)
{
	unlink(name);
	return 0;
}

static char *find_word(char *f, const char *str)
{
	char *p = f;

	while ((p = strstr(p, str)) != NULL) {
		/* Check it's not in the middle of a word. */
		if (p > f && (isalnum(p[-1]) || p[-1] == '_')) {
			p++;
			continue;
		}
		if (isalnum(p[strlen(str)]) || p[strlen(str)] == '_') {
			p++;
			continue;
		}
		return p;
	}
	return NULL;
}

/* This is horribly inefficient but simple. */
static const char *rewrite_file(const char *filename,
				const struct replace *repl)
{
	char *newname, *file;
	int fd;

	verbose("Rewriting %s\n", filename);
	file = grab_file(filename, filename);
	if (!file)
		err(1, "Reading file %s", filename);

	for (; repl; repl = repl->next) {
		char *p;

		while ((p = find_word(file, repl->string)) != NULL) {
			unsigned int off;
			char *new = talloc_array(file, char, strlen(file)+6);

			off = p - file;
			memcpy(new, file, off);
			if (isupper(repl->string[0]))
				memcpy(new + off, "CCAN_", 5);
			else
				memcpy(new + off, "ccan_", 5);
			strcpy(new + off + 5, file + off);
			file = new;
		}
	}

	/* If we exit for some reason, we want this erased. */
	newname = talloc_asprintf(talloc_autofree_context(), "%s.tmp",
				  filename);
	fd = open(newname, O_WRONLY|O_CREAT|O_EXCL, 0644);
	if (fd < 0)
		err(1, "Creating %s", newname);

	talloc_set_destructor(newname, unlink_destroy);
	if (write(fd, file, strlen(file)) != strlen(file)) {
		if (errno == 0)
			errx(1, "Short write to %s: disk full?", newname);
		errx(1, "Writing to %s", newname);
	}
	close(fd);
	return newname;
}

struct adjusted
{
	struct adjusted *next;
	const char *file;
	const char *tmpfile;
};

static void setup_adjust_files(const char *dir,
			       const struct replace *repl,
			       struct adjusted **adj)
{
	char **files;

	for (files = get_dir(dir); *files; files++) {
		if (strends(*files, "/test"))
			setup_adjust_files(*files, repl, adj);
		else if (strends(*files, ".c") || strends(*files, ".h")) {
			struct adjusted *a = talloc(dir, struct adjusted);
			a->next = *adj;
			a->file = *files;
			a->tmpfile = rewrite_file(a->file, repl);
			*adj = a;
		}
	}
}

/* This is the "commit" stage, so we hope it won't fail. */
static void rename_files(const struct adjusted *adj)
{
	while (adj) {
		if (rename(adj->tmpfile, adj->file) != 0)
			warn("Could not rename over '%s', we're in trouble",
			     adj->file);
		adj = adj->next;
	}
}

static void convert_dir(const char *dir)
{
	char *name;
	struct replace *replace = NULL;
	struct adjusted *adj = NULL;

	/* Remove any ugly trailing slashes. */
	name = talloc_strdup(NULL, dir);
	while (strends(name, "/"))
		name[strlen(name)-1] = '\0';

	analyze_headers(name, &replace);
	get_exposed_symbols(name, &replace);
	write_replacement_file(name, &replace);
	setup_adjust_files(name, replace, &adj);
	rename_files(adj);
	talloc_free(name);
	talloc_free(replace);
}

static struct replace *read_replacement_file(const char *depdir)
{
	struct replace *repl = NULL;
	char *replname = talloc_asprintf(depdir, "%s/.namespacize", depdir);
	char *file, **line;

	file = grab_file(replname, replname);
	if (!file) {
		if (errno != ENOENT)
			err(1, "Opening %s", replname);
		return NULL;
	}

	for (line = split(file, file, "\n", NULL); *line; line++)
		add_replace(&repl, *line);
	return repl;
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

static char **get_deps(const void *ctx, const char *dir)
{
	char **deps, *cmd;

	cmd = talloc_asprintf(ctx, "%s depends", build_info(ctx, dir));
	deps = lines_from_cmd(cmd, cmd);
	if (!deps)
		err(1, "Could not run '%s'", cmd);
	return deps;
}

static char *parent_dir(const void *ctx, const char *dir)
{
	char *parent, *slash;

	parent = talloc_strdup(ctx, dir);
	slash = strrchr(parent, '/');
	if (slash)
		*slash = '\0';
	else
		parent = talloc_strdup(ctx, ".");
	return parent;
}

static void adjust_dir(const char *dir)
{
	char *parent = parent_dir(NULL, dir);
	char **deps;

	verbose("Adjusting %s\n", dir);
	verbose_indent();
	for (deps = get_deps(parent, dir); *deps; deps++) {
		char *depdir;
		struct adjusted *adj = NULL;
		struct replace *repl;

		depdir = talloc_asprintf(parent, "%s/%s", parent, *deps);
		repl = read_replacement_file(depdir);
		if (repl) {
			verbose("%s has been namespacized\n", depdir);
			setup_adjust_files(parent, repl, &adj);
			rename_files(adj);
		} else
			verbose("%s has not been namespacized\n", depdir);
		talloc_free(depdir);
	}
	verbose_unindent();
}

static void adjust_dependents(const char *dir)
{
	char *parent = parent_dir(NULL, dir);
	char *base = basename(parent, dir);
	char **file;

	verbose("Looking for dependents in %s\n", parent);
	verbose_indent();
	for (file = get_dir(parent); *file; file++) {
		char *infoc, **deps;
		bool isdep = false;

		if (basename(*file, *file)[0] == '.')
			continue;

		infoc = talloc_asprintf(*file, "%s/_info.c", *file);
		if (access(infoc, R_OK) != 0)
			continue;

		for (deps = get_deps(*file, *file); *deps; deps++) {
			if (streq(*deps, base))
				isdep = true;
		}
		if (isdep)
			adjust_dir(*file);
		else
			verbose("%s is not dependent\n", *file);
	}
	verbose_unindent();
}

int main(int argc, char *argv[])
{
	if (argv[1] && streq(argv[1], "--verbose")) {
		verbose = true;
		argv++;
		argc--;
	}

	if (argc == 2) {
		verbose("Namespacizing %s\n", argv[1]);
		verbose_indent();
		convert_dir(argv[1]);
		adjust_dependents(argv[1]);
		verbose_unindent();
		return 0;
	}

	if (argc > 2 && streq(argv[1], "--adjust")) {
		unsigned int i;

		for (i = 2; i < argc; i++)
			adjust_dir(argv[i]);
		return 0;
	}
	usage();
}
