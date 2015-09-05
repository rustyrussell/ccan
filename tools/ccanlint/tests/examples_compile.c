#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/tal/tal.h>
#include <ccan/tal/str/str.h>
#include <ccan/take/take.h>
#include <ccan/cast/cast.h>
#include <ccan/tal/path/path.h>
#include <ccan/str/str.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <err.h>
#include "build.h"

static const char *can_run(struct manifest *m)
{
	if (safe_mode)
		return "Safe mode enabled";
	if (list_empty(&m->examples))
		return "No examples to compile";
	return NULL;
}

static void add_mod(struct manifest ***deps, struct manifest *m)
{
	unsigned int num = tal_count(*deps);
	tal_resize(deps, num + 1);
	(*deps)[num] = m;
}

static bool have_mod(struct manifest *deps[], const char *modname)
{
	unsigned int i;

	for (i = 0; i < tal_count(deps); i++)
		if (strcmp(deps[i]->modname, modname) == 0)
			return true;
	return false;
}

static void add_dep(struct manifest ***deps, const char *modname)
{
	unsigned int i;
	struct manifest *m;
	char *errstr;

	if (have_mod(*deps, modname))
		return;

	m = get_manifest(*deps,
			 tal_fmt(*deps, "%s/ccan/%s", ccan_dir, modname));
	errstr = build_submodule(m, cflags, COMPILE_NORMAL);
	if (errstr)
		errx(1, "%s", errstr);

	add_mod(deps, m);

	/* Get that modules depends as well... */
	assert(!safe_mode);
	if (m->info_file) {
		char **infodeps;

		infodeps = get_deps(m, m->dir, "depends", false,
				    get_or_compile_info);

		for (i = 0; infodeps[i]; i++) {
			if (strstarts(infodeps[i], "ccan/"))
				add_dep(deps, infodeps[i] + strlen("ccan/"));
		}
	}
}

static struct manifest **get_example_deps(struct manifest *m,
					  struct ccan_file *f)
{
	char **lines;
	struct manifest **deps = tal_arr(f, struct manifest *, 0);

	/* This one for a start. */
	add_dep(&deps, m->modname);

	/* Other modules implied by includes. */
	for (lines = get_ccan_file_lines(f); *lines; lines++) {
		char *modname;
		if (tal_strreg(f, *lines,
			       "^[ \t]*#[ \t]*include[ \t]*[<\"]"
			       "ccan/+(.+)/+[^/]+\\.h", &modname)) {
			if (!have_mod(deps, modname))
				add_dep(&deps, modname);
		}
	}

	return deps;
}

static char *example_obj_list(const void *ctx, struct manifest **deps)
{
	char *list = tal_strdup(ctx, "");
	unsigned int i;

	for (i = 0; i < tal_count(deps); i++) {
		if (deps[i]->compiled[COMPILE_NORMAL])
			tal_append_fmt(&list, " %s",
				       deps[i]->compiled[COMPILE_NORMAL]);
	}
	return list;
}

static char *example_lib_list(const void *ctx, struct manifest **deps)
{
	char *list = tal_strdup(ctx, "");
	char **libs;
	unsigned int i, j;

	/* FIXME: This doesn't uniquify. */
	for (i = 0; i < tal_count(deps); i++) {
		libs = get_libs(ctx, deps[i]->dir, NULL, get_or_compile_info);
		for (j = 0; libs[j]; j++)
			tal_append_fmt(&list, "-l%s ", libs[j]);
	}
	return list;
}

static char *cflags_list(const struct manifest *m)
{
	unsigned int i;
	char *ret = tal_strdup(m, cflags);

	char **flags = get_cflags(m, m->dir, get_or_compile_info);
	for (i = 0; flags[i]; i++)
		tal_append_fmt(&ret, " %s", flags[i]);
	return ret;
}

/* FIXME: Test with reduced features! */
static bool compile(const void *ctx,
		    struct manifest *m,
		    struct ccan_file *file,
		    char **output)
{
	struct manifest **deps = get_example_deps(m, file);
	const char *flags = cflags_list(m);

	file->compiled[COMPILE_NORMAL] = temp_file(ctx, "", file->fullname);
	if (!compile_and_link(ctx, file->fullname, ccan_dir,
			      example_obj_list(file, deps),
			      compiler, flags,
			      example_lib_list(file, deps),
			      file->compiled[COMPILE_NORMAL],
			      output)) {
		/* Don't keep failures, even with --keep */
		unlink(file->compiled[COMPILE_NORMAL]);
		file->compiled[COMPILE_NORMAL] = NULL;
		return false;
	}
	return true;
}

static void start_main(char **ret, const char *why)
{
	tal_append_fmt(ret,
	      "/* The example %s, so fake function wrapper inserted */\n"
	      "int main(int argc, char *argv[])\n"
	       "{\n", why);
}

/* We only handle simple function definitions here. */
static char *add_func(const tal_t *ctx, char *others, const char *line)
{
	const char *p, *end = strchr(line, '(') - 1;
	char *use;

	while (cisspace(*end)) {
		end--;
		if (end == line)
			return others;
	}

	for (p = end; cisalnum(*p) || *p == '_'; p--) {
		if (p == line)
			return others;
	}

	use = tal_fmt(ctx, "printf(\"%%p\", %.*s);\n",
		      (unsigned)(end - p), p+1);
	if (others)
		use = tal_strcat(ctx, take(others), take(use));

	return use;
}

static void strip_leading_whitespace(char **lines)
{
	unsigned int i, min_span = -1U;

	for (i = 0; lines[i]; i++) {
		unsigned int span = strspn(lines[i], " \t");
		/* All whitespace?  Ignore */
		if (!lines[i][span])
			continue;
		if (span < min_span)
			min_span = span;
	}

	for (i = 0; lines[i]; i++)
		if (strlen(lines[i]) >= min_span)
			lines[i] += min_span;
}

static bool looks_internal(char **lines, char **why)
{
	unsigned int i;
	bool last_ended = true; /* Did last line finish a statement? */

	for (i = 0; lines[i]; i++) {
		/* Skip leading whitespace. */
		const char *line = lines[i] + strspn(lines[i], " \t");
		unsigned len = strspn(line, IDENT_CHARS);

		if (!line[0] || cisspace(line[0]) || strstarts(line, "//")
		    || strstarts(line, "#line"))
			continue;

		assert(line[strlen(line)-1] != '\n');

		/* The winners. */
		if (strstarts(line, "if") && len == 2) {
			*why = cast_const(char *, "starts with if");
			return true;
		}
		if (strstarts(line, "for") && len == 3) {
			*why = cast_const(char *, "starts with for");
			return true;
		}
		if (strstarts(line, "while") && len == 5) {
			*why = cast_const(char *, "starts with while");
			return true;
		}
		if (strstarts(line, "do") && len == 2) {
			*why = cast_const(char *, "starts with do");
			return true;
		}

		/* The losers. */
		if (strstarts(line, "#include")) {
			*why = cast_const(char *, "starts with #include");
			return false;
		}

		if (last_ended && strchr(line, '(')) {
			if (strstarts(line, "static")) {
				*why = cast_const(char *,
						  "starts with static"
						  " and contains (");
				return false;
			}
			if (strends(line, ")")) {
				*why = cast_const(char *,
						  "contains ( and ends with )");
				return false;
			}
		}

		/* Previously prepended. */
		if (strstr(line, "didn't seem to belong inside a function")) {
			*why = cast_const(char *, "Comment said so");
			return false;
		}

		/* Single identifier then operator == inside function. */
		if (last_ended && len
		    && cispunct(line[len+strspn(line+len, " ")])) {
			*why = cast_const(char *, "starts with identifier"
					  " then punctuation");
			return true;
		}

		last_ended = (strends(line, "}")
			      || strends(line, ";")
			      || strends(line, "*/")
			      || streq(line, "..."));
	}

	/* No idea... Say yes? */
	*why = cast_const(char *, "gave no clues");
	return true;
}

/* Examples will often build on prior ones.  Try combining them. */
static char **combine(const void *ctx, char **lines, char **prev)
{
	unsigned int i, lines_total, prev_total, count;
	char **ret;
	const char *reasoning;
	char *why = NULL;

	if (!prev)
		return NULL;

	/* If it looks internal, put prev at start. */
	if (looks_internal(lines, &why)) {
		count = 0;
		reasoning = "seemed to belong inside a function";
	} else {
		/* Try inserting in first elided position */
		for (count = 0; lines[count]; count++) {
			if (strcmp(lines[count], "...") == 0)
				break;
		}
		if (!lines[count]) {
			/* Try at start anyway? */
			count = 0;
			reasoning = "didn't seem to belong inside"
				" a function, so we prepended the previous"
				" example";
		} else {
			reasoning = "didn't seem to belong inside"
				" a function, so we put the previous example"
				" at the first ...";

			count++;
		}
	}

	for (i = 0; lines[i]; i++);
	lines_total = i;

	for (i = 0; prev[i]; i++);
	prev_total = i;

	ret = tal_arr(ctx, char *, 1 + lines_total + prev_total + 1);
	ret[0] = tal_fmt(ret, "/* The example %s, thus %s */",
			 why, reasoning);
	memcpy(ret+1, lines, count * sizeof(ret[0]));
	memcpy(ret+1 + count, prev, prev_total * sizeof(ret[0]));
	memcpy(ret+1 + count + prev_total, lines + count,
	       (lines_total - count + 1) * sizeof(ret[0]));
	return ret;
}

/* Only handles very simple comments. */
static char *strip_comment(const void *ctx, const char *orig_line)
{
	char *p, *ret = tal_strdup(ctx, orig_line);

	p = strstr(ret, "/*");
	if (!p)
		p = strstr(ret, "//");
	if (p)
		*p = '\0';
	return ret;
}

static char *mangle(struct manifest *m, char **lines)
{
	char *ret, *use_funcs = NULL, *why;
	bool in_function = false, fake_function = false, has_main = false;
	unsigned int i;

	ret = tal_fmt(m,
		      "/* Include header from module. */\n"
		      "#include <ccan/%s/%s.h>\n"
		      "/* Prepend a heap of headers. */\n"
		      "#include <assert.h>\n"
		      "#include <err.h>\n"
		      "#include <errno.h>\n"
		      "#include <fcntl.h>\n"
		      "#include <limits.h>\n"
		      "#include <stdbool.h>\n"
		      "#include <stdint.h>\n"
		      "#include <stdio.h>\n"
		      "#include <stdlib.h>\n"
		      "#include <string.h>\n"
		      "#include <sys/stat.h>\n"
		      "#include <sys/types.h>\n"
		      "#include <unistd.h>\n",
		      m->modname, m->basename);

	ret = tal_strcat(m, take(ret), "/* Useful dummy functions. */\n"
				     "extern int somefunc(void);\n"
				     "int somefunc(void) { return 0; }\n"
				     "extern char somestring[];\n"
				     "char somestring[] = \"hello world\";\n");

	if (looks_internal(lines, &why)) {
		/* Wrap it all in main(). */
		start_main(&ret, why);
		fake_function = true;
		in_function = true;
		has_main = true;
	} else
		tal_append_fmt(&ret,
			     "/* The example %s, so didn't wrap in main() */\n",
			       why);

	/* Primitive, very primitive. */
	for (i = 0; lines[i]; i++) {
		char *line = strip_comment(ret, lines[i]);

		/* } at start of line ends a function. */
		if (in_function) {
			if (line[0] == '}')
				in_function = false;
		} else {
			/* Character at start of line, with ( and no ;
			 * == function start.  Ignore comments. */
			if (!cisspace(line[0])
			    && strchr(line, '(')
			    && !strchr(line, ';')
			    && !strstr(line, "//")) {
				in_function = true;
				if (strncmp(line, "int main", 8) == 0)
					has_main = true;
				if (strncmp(line, "static", 6) == 0) {
					use_funcs = add_func(m, use_funcs,
							     line);
				}
			}
		}
		/* ... means elided code. */
		if (strcmp(line, "...") == 0) {
			if (!in_function && !has_main
			    && looks_internal(lines + i + 1, &why)) {
				/* This implies we start a function here. */
				start_main(&ret, why);
				has_main = true;
				fake_function = true;
				in_function = true;
			}
			ret = tal_strcat(m, take(ret), "/* ... removed */\n");
			continue;
		}
		ret = tal_strcat(m, take(ret), lines[i]);
		ret = tal_strcat(m, take(ret), "\n");
	}

	if (!has_main) {
		ret = tal_strcat(m, take(ret),
			     "/* Need a main to link successfully. */\n"
			     "int main(void)\n{\n");
		fake_function = true;
	}

	if (use_funcs) {
		ret = tal_strcat(m, take(ret),
				 "/* Get rid of unused warnings"
				 " by printing addresses of"
				 " static funcs. */\n");
		if (!fake_function) {
			ret = tal_strcat(m, take(ret),
					 "int use_funcs(void);\n"
					 "int use_funcs(void) {\n");
			fake_function = true;
		}
		tal_append_fmt(&ret, "	%s\n", use_funcs);
	}

	if (fake_function)
		ret = tal_strcat(m, take(ret), "return 0;\n}\n");
	return ret;
}

static struct ccan_file *mangle_example(struct manifest *m,
					struct ccan_file *example,
					char **lines)
{
	char *name, *contents;
	int fd;
	struct ccan_file *f;

	name = temp_file(example, ".c",
			 take(tal_fmt(NULL, "mangled-%s", example->name)));
	f = new_ccan_file(example,
			  take(path_dirname(example, name)),
			  take(path_basename(example, name)));
	tal_steal(f, name);

	fd = open(f->fullname, O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (fd < 0)
		return NULL;

	contents = mangle(m, lines);
	if (write(fd, contents, strlen(contents)) != strlen(contents)) {
		close(fd);
		return NULL;
	}
	close(fd);
	f->contents = tal_steal(f, contents);
	list_add(&m->mangled_examples, &f->list);
	return f;
}

/* If an example has expected output, it's complete and should not be
 * included in future examples. */
static bool has_expected_output(char **lines)
{
	unsigned int i;

	for (i = 0; lines[i]; i++) {
		char *p = lines[i] + strspn(lines[i], " \t");
		if (!strstarts(p, "//"))
			continue;
		p += strspn(p, "/ ");
		if (strncasecmp(p, "given", strlen("given")) == 0)
			return true;
	}
	return false;
}

static unsigned int try_compiling(struct manifest *m,
				  struct ccan_file *i,
				  char **prev,
				  struct ccan_file *mangled[3],
				  bool res[3],
				  char *err[3],
				  char **lines[3])
{
	unsigned int num;

	/* Try standalone. */
	mangled[0] = i;
	res[0] = compile(i, m, mangled[0], &err[0]);
	lines[0] = get_ccan_file_lines(i);
	if (res[0] && streq(err[0], ""))
		return 1;

	if (prev) {
		lines[1] = combine(i, get_ccan_file_lines(i), prev);

		mangled[1] = mangle_example(m, i, lines[1]);
		res[1] = compile(i, m, mangled[1], &err[1]);
		if (res[1] && streq(err[1], "")) {
			return 2;
		}
		num = 2;
	} else
		num = 1;

	/* Try standalone. */
	lines[num] = get_ccan_file_lines(i);
	mangled[num] = mangle_example(m, i, lines[num]);
	res[num] = compile(i, m, mangled[num], &err[num]);

	return num+1;
}

static void build_examples(struct manifest *m,
			   unsigned int *timeleft, struct score *score)
{
	struct ccan_file *i;
	char **prev = NULL;
	bool warnings = false;

	score->total = 0;
	score->pass = true;

	list_for_each(&m->examples, i, list) {
		char *err[3];
		struct ccan_file *file[3] = { NULL, NULL, NULL };
		bool res[3];
		unsigned num, j;
		char **lines[3];
		const char *error;

		score->total++;

		/* Simplify our dumb parsing. */
		strip_leading_whitespace(get_ccan_file_lines(i));

		num = try_compiling(m, i, prev, file, res, err, lines);

		/* First look for a compile without any warnings. */
		for (j = 0; j < num; j++) {
			if (res[j] && streq(err[j], "")) {
				if (!has_expected_output(lines[j]))
					prev = lines[j];
				score->score++;
				goto next;
			}
		}

		/* Now accept anything which succeeded. */
		for (j = 0; j < num; j++) {
			if (res[j]) {
				if (!has_expected_output(lines[j]))
					prev = lines[j];
				score->score++;
				warnings = true;
				score_file_error(score, file[j], 0,
						 "Compiling extracted example"
						 " gave warnings:\n"
						 "Example:\n"
						 "%s\n"
						 "Compiler:\n"
						 "%s",
						 get_ccan_file_contents(file[j]),
						 err[j]);
				goto next;
			}
		}

		score->pass = false;
		if (!verbose) {
			if (num == 3)
				error = "Compiling standalone, adding headers, "
					"and including previous "
					"example all failed";
			else
				error = "Standalone compile and"
					" adding headers both failed";
		} else {
			if (num == 3) {
				error = tal_fmt(score,
				      "Standalone example:\n"
				      "%s\n"
				      "Errors: %s\n\n"
				      "Combining with previous example:\n"
				      "%s\n"
				      "Errors: %s\n\n"
				      "Adding headers, wrappers:\n"
				      "%s\n"
				      "Errors: %s\n\n",
				      get_ccan_file_contents(file[0]),
				      err[0],
				      get_ccan_file_contents(file[1]),
				      err[1],
				      get_ccan_file_contents(file[2]),
				      err[2]);
			} else {
				error = tal_fmt(score,
				      "Standalone example:\n"
				      "%s\n"
				      "Errors: %s\n\n"
				      "Adding headers, wrappers:\n"
				      "%s\n"
				      "Errors: %s\n\n",
				      get_ccan_file_contents(file[0]),
				      err[0],
				      get_ccan_file_contents(file[1]),
				      err[1]);
			}
		}
		score_file_error(score, i, 0, "%s", error);
		/* This didn't work, so not a candidate for combining. */
		prev = NULL;

	next:
		;
	}

	/* An extra point if they all compiled without warnings. */
	if (!list_empty(&m->examples)) {
		score->total++;
		if (!warnings)
			score->score++;
	}
}

struct ccanlint examples_compile = {
	.key = "examples_compile",
	.name = "Module examples compile",
	.check = build_examples,
	.can_run = can_run,
	.needs = "examples_exist module_builds"
};

REGISTER_TEST(examples_compile);
