#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/talloc/talloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

static const char *can_run(struct manifest *m)
{
	if (safe_mode)
		return "Safe mode enabled";
	return NULL;
}

static char *obj_list(const struct manifest *m)
{
	char *list;
	struct ccan_file *i;

	/* Object files for this module. */
	list_for_each(&m->c_files, i, list)
		list = talloc_asprintf_append(list, " %s", i->compiled);

	/* Other ccan modules we depend on. */
	list_for_each(&m->dep_dirs, i, list) {
		if (i->compiled)
			list = talloc_asprintf_append(list, " %s", i->compiled);
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
				  obj_list(m), "", lib_list(m), file->compiled);
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

static char *start_main(char *ret)
{
	return talloc_asprintf_append(ret,
				      "/* Fake function wrapper inserted */\n"
				      "int main(int argc, char *argv[])\n"
				      "{\n");
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
				      end - p + 1, p);
}

static bool looks_internal(const char *p)
{
	return (strncmp(p, "#", 1) != 0
		&& strncmp(p, "static", 6) != 0
		&& strncmp(p, "struct", 6) != 0
		&& strncmp(p, "union", 5) != 0);
}

static void strip_leading_whitespace(char **lines, unsigned prefix_len)
{
	unsigned int i;

	for (i = 0; lines[i]; i++)
		lines[i] += prefix_len;
}

static char *mangle(struct manifest *m, struct ccan_file *example)
{
	char **lines = get_ccan_file_lines(example);
	char *ret, *use_funcs = NULL;
	bool in_function = false, fake_function = false, has_main = false;
	unsigned int i;

	ret = talloc_strdup(m, "/* Prepend a heap of headers. */\n"
			    "#include <assert.h>\n"
			    "#include <err.h>\n"
			    "#include <fcntl.h>\n"
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

	ret = talloc_asprintf_append(ret, "/* Useful dummmy functions. */\n"
				     "int somefunc(void);\n"
				     "int somefunc(void) { return 0; }\n");

	/* Starts indented? */
	if (lines[0] && isblank(lines[0][0])) {
		unsigned prefix = strspn(lines[0], " \t");
		if (looks_internal(lines[0] + prefix)) {
			/* Wrap it all in main(). */
			ret = start_main(ret);
			fake_function = true;
			in_function = true;
			has_main = true;
		} else
			strip_leading_whitespace(lines, prefix);
	}

	/* Primitive, very primitive. */
	for (i = 0; lines[i]; i++) {
		/* } at start of line ends a function. */
		if (in_function) {
			if (lines[i][0] == '}')
				in_function = false;
		} else {
			/* Character at start of line, with ( and no ;
			 * == function start. */
			if (!isblank(lines[i][0])
			    && strchr(lines[i], '(')
			    && !strchr(lines[i], ';')) {
				in_function = true;
				if (strncmp(lines[i], "int main", 8) == 0)
					has_main = true;
				if (strncmp(lines[i], "static", 6) == 0) {
					use_funcs = add_func(use_funcs,
							     lines[i]);
				}
			}
		}
		/* ... means elided code.  If followed by spaced line, means
		 * next part is supposed to be inside a function. */
		if (strcmp(lines[i], "...") == 0) {
			if (!in_function
			    && lines[i+1]
			    && isblank(lines[i+1][0])) {
				/* This implies we start a function here. */
				ret = start_main(ret);
				fake_function = true;
				in_function = true;
			}
			ret = talloc_asprintf_append(ret,
						     "/* ... removed */\n");
			continue;
		}
		ret = talloc_asprintf_append(ret, "%s\n", lines[i]);
	}

	/* Need a main to link successfully. */
	if (!has_main) {
		ret = talloc_asprintf_append(ret, "int main(void)\n{\n");
		fake_function = true;
	}

	/* Get rid of unused warnings by printing addresses of static funcs. */
	if (use_funcs) {
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
					struct ccan_file *example, bool keep)
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

	contents = mangle(m, example);
	if (write(fd, contents, strlen(contents)) != strlen(contents)) {
		close(fd);
		return NULL;
	}
	close(fd);
	return f;
}

static void *build_examples(struct manifest *m, bool keep,
			    unsigned int *timeleft)
{
	struct ccan_file *i;
	struct score *score = talloc(m, struct score);

	score->score = 0;
	score->errors = NULL;

	list_for_each(&m->examples, i, list) {
		char *ret;

		examples_compile.total_score++;
		ret = compile(score, m, i, keep);
		if (!ret)
			score->score++;
		else {
			struct ccan_file *mangle = mangle_example(m, i, keep);

			talloc_free(ret);
			ret = compile(score, m, mangle, keep);
			if (!ret)
				score->score++;
			else {
				if (!score->errors)
					score->errors = ret;
				else {
					score->errors
					= talloc_append_string(score->errors,
							       ret);
					talloc_free(ret);
				}
			}
		}
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
	.check = build_examples,
	.describe = describe,
	.can_run = can_run,
};

REGISTER_TEST(examples_compile, &has_examples, NULL);
