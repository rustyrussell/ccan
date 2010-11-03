#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/talloc/talloc.h>
#include <ccan/str/str.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>

static const char *can_run(struct manifest *m)
{
	if (safe_mode)
		return "Safe mode enabled";
	return NULL;
}

/* FIXME: We should build if it doesn't exist... */
static bool expect_obj_file(const char *dir)
{
	struct manifest *dep_man;
	bool has_c_files;

	dep_man = get_manifest(dir, dir);

	/* If it has C files, we expect an object file built from them. */
	has_c_files = !list_empty(&dep_man->c_files);
	talloc_free(dep_man);
	return has_c_files;
}

static char *add_dep(const struct manifest *m, char *list, const char *mod)
{
	char **deps, *obj;
	unsigned int i;

	/* Not ourselves. */
	if (streq(m->basename, mod))
		return list;

	/* Not if there's no object file for that module */
	if (!expect_obj_file(talloc_asprintf(list, "%s/ccan/%s", ccan_dir,mod)))
		return list;

	obj = talloc_asprintf(list, "%s/ccan/%s.o", ccan_dir, mod);

	/* Not anyone we've already included. */
	if (strstr(list, obj))
		return list;

	list = talloc_asprintf_append(list, " %s", obj);

	/* Get that modules depends as well... */
	assert(!safe_mode);
	deps = get_deps(m, talloc_asprintf(list, "%s/ccan/%s", ccan_dir, mod),
			false, NULL);

	for (i = 0; deps[i]; i++) {
		if (strstarts(deps[i], "ccan/"))
			list = add_dep(m, list, deps[i] + strlen("ccan/"));
	}
	return list;
}

static char *obj_list(const struct manifest *m, struct ccan_file *f)
{
	char *list = talloc_strdup(m, "");
	struct ccan_file *i;
	char **lines;

	/* Object files for this module. */
	list_for_each(&m->c_files, i, list)
		list = talloc_asprintf_append(list, " %s", i->compiled);

	/* Other ccan modules we depend on. */
	list_for_each(&m->dep_dirs, i, list) {
		if (i->compiled)
			list = talloc_asprintf_append(list, " %s", i->compiled);
	}

	/* Other modules implied by includes. */
	for (lines = get_ccan_file_lines(f); *lines; lines++) {
		unsigned preflen = strspn(*lines, " \t");
		if (strstarts(*lines + preflen, "#include <ccan/")) {
			const char *mod;
			unsigned modlen;

			mod = *lines + preflen + strlen("#include <ccan/");
			modlen = strcspn(mod, "/");
			mod = talloc_strndup(f, mod, modlen);
			list = add_dep(m, list, mod);
		}
	}

	return list;
}

static char *lib_list(const struct manifest *m)
{
	unsigned int i, num;
	char **libs = get_libs(m, ".", &num, &m->info_file->compiled);
	char *ret = talloc_strdup(m, "");

	for (i = 0; i < num; i++)
		ret = talloc_asprintf_append(ret, "-l%s ", libs[i]);
	return ret;
}

static char *compile(const void *ctx,
		     struct manifest *m,
		     struct ccan_file *file,
		     bool keep)
{
	char *errmsg;

	file->compiled = maybe_temp_file(ctx, "", keep, file->fullname);
	errmsg = compile_and_link(ctx, file->fullname, ccan_dir,
				  obj_list(m, file),
				  "", lib_list(m), file->compiled);
	if (errmsg) {
		talloc_free(file->compiled);
		return errmsg;
	}
	return NULL;
}

struct score {
	unsigned int score;
	char *errors;
};

static char *start_main(char *ret, const char *why)
{
	return talloc_asprintf_append(ret,
	      "/* The example %s, so fake function wrapper inserted */\n"
	      "int main(int argc, char *argv[])\n"
	      "{\n", why);
}

/* We only handle simple function definitions here. */
static char *add_func(char *others, const char *line)
{
	const char *p, *end = strchr(line, '(') - 1;
	while (isblank(*end)) {
		end--;
		if (end == line)
			return others;
	}

	for (p = end; isalnum(*p) || *p == '_'; p--) {
		if (p == line)
			return others;
	}

	return talloc_asprintf_append(others, "printf(\"%%p\", %.*s);\n",
				      (unsigned)(end - p + 1), p);
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

		if (!line[0] || isblank(line[0]) || strstarts(line, "//"))
			continue;

		/* The winners. */
		if (strstarts(line, "if") && len == 2) {
			*why = "starts with if";
			return true;
		}
		if (strstarts(line, "for") && len == 3) {
			*why = "starts with for";
			return true;
		}
		if (strstarts(line, "while") && len == 5) {
			*why = "starts with while";
			return true;
		}
		if (strstarts(line, "do") && len == 2) {
			*why = "starts with do";
			return true;
		}

		/* The losers. */
		if (strstarts(line, "#include")) {
			*why = "starts with #include";
			return false;
		}

		if (last_ended && strchr(line, '(')) {
			if (strstarts(line, "static")) {
				*why = "starts with static and contains (";
				return false;
			}
			if (strends(line, ")")) {
				*why = "contains ( and ends with )";
				return false;
			}
		}

		/* Single identifier then operator == inside function. */
		if (last_ended && len
		    && ispunct(line[len+strspn(line+len, " ")])) {
			*why = "starts with identifier then punctuation";
			return true;
		}

		last_ended = (strends(line, "}")
			      || strends(line, ";")
			      || streq(line, "..."));
	}

	/* No idea... Say yes? */
	*why = "gave no clues";
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

	ret = talloc_array(ctx, char *, 1 +lines_total + prev_total + 1);
	ret[0] = talloc_asprintf(ret, "/* The example %s, thus %s */\n",
				 why, reasoning);
	memcpy(ret+1, lines, count * sizeof(ret[0]));
	memcpy(ret+1 + count, prev, prev_total * sizeof(ret[0]));
	memcpy(ret+1 + count + prev_total, lines + count,
	       (lines_total - count + 1) * sizeof(ret[0]));
	return ret;
}

static char *mangle(struct manifest *m, char **lines)
{
	char *ret, *use_funcs = NULL, *why;
	bool in_function = false, fake_function = false, has_main = false;
	unsigned int i;

	ret = talloc_strdup(m, "/* Prepend a heap of headers. */\n"
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
			    "#include <unistd.h>\n");
	ret = talloc_asprintf_append(ret, "/* Include header from module. */\n"
				     "#include <ccan/%s/%s.h>\n",
				     m->basename, m->basename);

	ret = talloc_asprintf_append(ret, "/* Useful dummy functions. */\n"
				     "extern int somefunc(void);\n"
				     "int somefunc(void) { return 0; }\n"
				     "extern char somestring[];\n"
				     "char somestring[] = \"hello world\";\n");

	if (looks_internal(lines, &why)) {
		/* Wrap it all in main(). */
		ret = start_main(ret, why);
		fake_function = true;
		in_function = true;
		has_main = true;
	} else
		ret = talloc_asprintf_append(ret,
			     "/* The example %s, so didn't wrap in main() */\n",
				     why);

	/* Primitive, very primitive. */
	for (i = 0; lines[i]; i++) {
		/* } at start of line ends a function. */
		if (in_function) {
			if (lines[i][0] == '}')
				in_function = false;
		} else {
			/* Character at start of line, with ( and no ;
			 * == function start.  Ignore comments. */
			if (!isblank(lines[i][0])
			    && strchr(lines[i], '(')
			    && !strchr(lines[i], ';')
			    && !strstr(lines[i], "//")) {
				in_function = true;
				if (strncmp(lines[i], "int main", 8) == 0)
					has_main = true;
				if (strncmp(lines[i], "static", 6) == 0) {
					use_funcs = add_func(use_funcs,
							     lines[i]);
				}
			}
		}
		/* ... means elided code. */
		if (strcmp(lines[i], "...") == 0) {
			if (!in_function && !has_main
			    && looks_internal(lines + i + 1, &why)) {
				/* This implies we start a function here. */
				ret = start_main(ret, why);
				has_main = true;
				fake_function = true;
				in_function = true;
			}
			ret = talloc_asprintf_append(ret,
						     "/* ... removed */\n");
			continue;
		}
		ret = talloc_asprintf_append(ret, "%s\n", lines[i]);
	}

	if (!has_main) {
		ret = talloc_asprintf_append(ret,
			     "/* Need a main to link successfully. */\n"
			     "int main(void)\n{\n");
		fake_function = true;
	}

	if (use_funcs) {
		ret = talloc_asprintf_append(ret,
					     "/* Get rid of unused warnings"
					     " by printing addresses of"
					     " static funcs. */");
		if (!fake_function) {
			ret = talloc_asprintf_append(ret,
						     "int use_funcs(void);\n"
						     "int use_funcs(void) {\n");
			fake_function = true;
		}
		ret = talloc_asprintf_append(ret, "	%s\n", use_funcs);
	}

	if (fake_function)
		ret = talloc_asprintf_append(ret, "return 0;\n"
					     "}\n");
	return ret;
}

static struct ccan_file *mangle_example(struct manifest *m,
					struct ccan_file *example,
					char **lines,
					bool keep)
{
	char *name, *contents;
	int fd;
	struct ccan_file *f;

	name = maybe_temp_file(example, ".c", keep, 
			       talloc_asprintf(m, "%s/mangled-%s",
					       m->dir, example->name));
	f = new_ccan_file(example,
			  talloc_dirname(example, name),
			  talloc_basename(example, name));
	talloc_steal(f, name);

	fd = open(f->fullname, O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (fd < 0)
		return NULL;

	contents = mangle(m, lines);
	if (write(fd, contents, strlen(contents)) != strlen(contents)) {
		close(fd);
		return NULL;
	}
	close(fd);
	f->contents = talloc_steal(f, contents);
	return f;
}

static void *build_examples(struct manifest *m, bool keep,
			    unsigned int *timeleft)
{
	struct ccan_file *i;
	struct score *score = talloc(m, struct score);
	char **prev = NULL;

	score->score = 0;
	score->errors = talloc_strdup(score, "");

	examples_compile.total_score = 0;
	list_for_each(&m->examples, i, list) {
		char *ret, *ret1 = NULL, *ret2;
		struct ccan_file *mangle1 = NULL, *mangle2;

		examples_compile.total_score++;
		/* Simplify our dumb parsing. */
		strip_leading_whitespace(get_ccan_file_lines(i));
		ret = compile(score, m, i, keep);
		if (!ret) {
			prev = get_ccan_file_lines(i);
			score->score++;
			continue;
		}

		/* Try combining with previous (successful) example... */
		if (prev) {
			char **new = combine(i, get_ccan_file_lines(i), prev);

			mangle1 = mangle_example(m, i, new, keep);
			ret1 = compile(score, m, mangle1, keep);
			if (!ret1) {
				prev = new;
				score->score++;
				continue;
			}
		}

		/* Try standalone. */
		mangle2 = mangle_example(m, i, get_ccan_file_lines(i), keep);
		ret2 = compile(score, m, mangle2, keep);
		if (!ret2) {
			prev = get_ccan_file_lines(i);
			score->score++;
			continue;
		}

		score->errors = talloc_asprintf_append(score->errors,
				       "%s: tried standalone example:\n"
				       "%s\n"
				       "Errors: %s\n\n",
				       i->name,
				       get_ccan_file_contents(i),
				       ret);
		if (mangle1) {
			score->errors = talloc_asprintf_append(score->errors,
					       "%s: tried combining with"
					       " previous example:\n"
					       "%s\n"
					       "Errors: %s\n\n",
					       i->name,
					       get_ccan_file_contents(mangle1),
					       ret1);
		}
		score->errors = talloc_asprintf_append(score->errors,
				       "%s: tried adding headers, wrappers:\n"
				       "%s\n"
				       "Errors: %s\n\n",
				       i->name,
				       get_ccan_file_contents(mangle2),
				       ret2);
		/* This didn't work, so not a candidate for combining. */
		prev = NULL;
	}

	if (strcmp(score->errors, "") == 0) {
		talloc_free(score);
		return NULL;
	}
	return score;
}

static unsigned int score_examples(struct manifest *m, void *check_result)
{
	struct score *score = check_result;
	return score->score;
}

static const char *describe(struct manifest *m, void *check_result)
{
	struct score *score = check_result;
	if (verbose >= 2 && score->errors)
		return talloc_asprintf(m, "Compile errors building examples:\n"
				       "%s", score->errors);
	return NULL;
}

struct ccanlint examples_compile = {
	.key = "examples-compile",
	.name = "Module examples compile",
	.score = score_examples,
	.total_score = 3, /* This gets changed to # examples, if they exist */
	.check = build_examples,
	.describe = describe,
	.can_run = can_run,
};

REGISTER_TEST(examples_compile, &has_examples, NULL);
