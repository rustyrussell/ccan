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

	/* This CCAN module. */
	list = talloc_asprintf(m, " %s.o", m->dir);

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

static char *mangle(struct manifest *m, struct ccan_file *example)
{
	char **lines = get_ccan_file_lines(example);
	char *ret;
	bool in_function = false, fake_function = false;
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

	/* Starts indented?  Wrap it in a main() function. */
	if (lines[0] && isblank(lines[0][0])) {
		ret = start_main(ret);
		fake_function = true;
		in_function = true;
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
			    && !strchr(lines[i], ';'))
				in_function = true;
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
