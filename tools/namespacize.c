/* Code to move a ccan module into the ccan_ namespace. */
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "string/string.h"
#include "talloc/talloc.h"
#include "tools.h"

#define IDENT_CHARS	"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
			"abcdefghijklmnopqrstuvwxyz" \
			"01234567889_"

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

#define strstarts(str,prefix) (strncmp((str),(prefix),strlen(prefix)) == 0)

static inline bool strends(const char *str, const char *postfix)
{
	if (strlen(str) < strlen(postfix))
		return false;

	return streq(str + strlen(str) - strlen(postfix), postfix);
}

static int unlink_no_errno(const char *filename)
{
	int ret = 0, serrno = errno;
	if (unlink(filename) < 0)
		ret = errno;
	errno = serrno;
	return ret;
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
	struct replace *new, *i;

	/* Avoid duplicates. */
	for (i = *repl; i; i = i->next)
		if (streq(i->string, str))
			return;

	new = talloc(*repl, struct replace);
	new->next = *repl;
	new->string = talloc_strdup(new, str);
	*repl = new;
}

static void add_replace_tok(struct replace **repl, const char *s)
{
	struct replace *new;
	unsigned int len = strspn(s, IDENT_CHARS);

	new = talloc(*repl, struct replace);
	new->next = *repl;
	new->string = talloc_strndup(new, s, len);
	*repl = new;
}

static char *basename(const void *ctx, const char *dir)
{
	char *p = strrchr(dir, '/');

	if (!p)
		return (char *)dir;
	return talloc_strdup(ctx, p+1);
}

static void look_for_macros(char *contents, struct replace **repl)
{
	char *p;
	enum { LINESTART, HASH, DEFINE, NONE } state = LINESTART;

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

				len = strspn(p, IDENT_CHARS);
				if (len) {
					char *s;
					s = talloc_strndup(contents, p, len);
					/* Don't wrap idempotent wrappers */
					if (!strstarts(s, "CCAN_")) {
						verbose("Found %s\n", s);
						add_replace(repl, s);
					}
				}
				state = NONE;
			} else
				state = NONE;
		}
	}
}

/* Blank out preprocessor lines, and eliminate \ */
static void preprocess(char *p)
{
	char *s;

	/* We assume backslashes are only used for macros. */
	while ((s = strstr(p, "\\\n")) != NULL)
		s[0] = s[1] = ' ';

	/* Now eliminate # lines. */
	if (p[0] == '#') {
		unsigned int i;
		for (i = 0; p[i] != '\n'; i++)
			p[i] = ' ';
	}
	while ((s = strstr(p, "\n#")) != NULL) {
		unsigned int i;
		for (i = 1; s[i] != '\n'; i++)
			s[i] = ' ';
	}
}

static char *get_statement(const void *ctx, char **p)
{
	unsigned brackets = 0;
	bool seen_brackets = false;
	char *answer = talloc_strdup(ctx, "");

	for (;;) {
		if ((*p)[0] == '/' && (*p)[1] == '/')
			*p += strcspn(*p, "\n");
		else if ((*p)[0] == '/' && (*p)[1] == '*')
			*p = strstr(*p, "*/") + 1;
		else {
			char c = **p;
			if (c == ';' && !brackets) {
				(*p)++;
				return answer;
			}
			/* Compress whitespace into a single ' ' */
			if (isspace(c)) {
				c = ' ';
				while (isspace((*p)[1]))
					(*p)++;
			} else if (c == '{' || c == '(' || c == '[') {
				if (c == '(')
					seen_brackets = true;
				brackets++;
			} else if (c == '}' || c == ')' || c == ']')
				brackets--;

			if (answer[0] != '\0' || c != ' ') {
				answer = talloc_realloc(NULL, answer, char,
							strlen(answer) + 2);
				answer[strlen(answer)+1] = '\0';
				answer[strlen(answer)] = c;
			}
			if (c == '}' && seen_brackets && brackets == 0) {
				(*p)++;
				return answer;
			}
		}
		(*p)++;
		if (**p == '\0')
			return NULL;
	}
}

/* This hack should handle well-formatted code. */
static void look_for_definitions(char *contents, struct replace **repl)
{
	char *stmt, *p = contents;

	preprocess(contents);

	while ((stmt = get_statement(contents, &p)) != NULL) {
		int i, len;

		/* Definition of struct/union? */
		if ((strncmp(stmt, "struct", 5) == 0
		     || strncmp(stmt, "union", 5) == 0)
		    && strchr(stmt, '{') && stmt[7] != '{')
			add_replace_tok(repl, stmt+7);

		/* Definition of var or typedef? */
		for (i = strlen(stmt)-1; i >= 0; i--)
			if (strspn(stmt+i, IDENT_CHARS) == 0)
				break;

		if (i != strlen(stmt)-1) {
			add_replace_tok(repl, stmt+i+1);
			continue;
		}

		/* function or array declaration? */
		len = strspn(stmt, IDENT_CHARS "* ");
		if (len > 0 && (stmt[len] == '(' || stmt[len] == '[')) {
			if (strspn(stmt + len + 1, IDENT_CHARS) != 0) {
				for (i = len-1; i >= 0; i--)
					if (strspn(stmt+i, IDENT_CHARS) == 0)
						break;
				if (i != len-1) {
					add_replace_tok(repl, stmt+i+1);
					continue;
				}
			} else {
				/* Pointer to function? */
				len++;
				len += strspn(stmt + len, " *");
				i = strspn(stmt + len, IDENT_CHARS);
				if (i > 0 && stmt[len + i] == ')')
					add_replace_tok(repl, stmt+len);
			}
		}
	}
}

/* FIXME: Only does main header, should chase local includes. */ 
static void analyze_headers(const char *dir, struct replace **repl)
{
	char *hdr, *contents;

	/* Get hold of header, assume that's it. */
	hdr = talloc_asprintf(dir, "%s/%s.h", dir, basename(dir, dir));
	contents = grab_file(dir, hdr);
	if (!contents)
		err(1, "Reading %s", hdr);

	verbose("Looking in %s for macros\n", hdr);
	verbose_indent();
	look_for_macros(contents, repl);
	verbose_unindent();

	verbose("Looking in %s for symbols\n", hdr);
	verbose_indent();
	look_for_definitions(contents, repl);
	verbose_unindent();
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
