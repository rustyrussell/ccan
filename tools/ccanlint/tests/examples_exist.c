#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/str/str.h>
#include <ccan/tal/path/path.h>
#include <ccan/take/take.h>
#include <ccan/cast/cast.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <string.h>
#include <ctype.h>

/* Creates and adds an example file. */
static char *add_example(struct manifest *m, struct ccan_file *source,
			 struct doc_section *example)
{
	char *name, *linemarker;
	unsigned int i;
	int fd;
	struct ccan_file *f;

	name = tal_fmt(m, "example-%s-%s",
		       source->name, example->function);
	/* example->function == 'struct foo' */
	while (strchr(name, ' '))
		*strchr(name, ' ') = '_';

	name = temp_file(m, ".c", take(name));
	f = new_ccan_file(m, take(path_dirname(m, name)),
			  take(path_basename(m, name)));
	tal_steal(f, name);
	list_add_tail(&m->examples, &f->list);

	fd = open(f->fullname, O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (fd < 0)
		return tal_fmt(m, "Creating temporary file %s: %s",
			       f->fullname, strerror(errno));

	/* Add #line to demark where we are from, so errors are correct! */
	linemarker = tal_fmt(f, "#line %i \"%s\"\n",
			     example->srcline+2, source->fullname);
	write(fd, linemarker, strlen(linemarker));

	for (i = 0; i < example->num_lines; i++) {
		if (write(fd, example->lines[i], strlen(example->lines[i]))
		    != strlen(example->lines[i])
		    || write(fd, "\n", 1) != 1) {
			close(fd);
			return cast_const(char *,
					  "Failure writing to temporary file");
		}
	}
	close(fd);
	return NULL;
}

/* FIXME: We should have one example per function in header. */
static void extract_examples(struct manifest *m,
			     unsigned int *timeleft,
			     struct score *score)
{
	struct ccan_file *f, *mainh = NULL; /* gcc complains uninitialized */
	struct doc_section *d;
	bool have_info_example = false, have_header_example = false;

	score->total = 2;
	list_for_each(get_ccan_file_docs(m->info_file), d, list) {
		if (streq(d->type, "example")) {
			score->error = add_example(m, m->info_file, d);
			if (score->error)
				return;
			have_info_example = true;
		}
	}

	/* Check all headers for examples. */
	list_for_each(&m->h_files, f, list) {
		if (strstarts(f->name, m->basename)
		    && strlen(f->name) == strlen(m->basename) + 2)
			mainh = f;

		list_for_each(get_ccan_file_docs(f), d, list) {
			if (streq(d->type, "example")) {
				score->error = add_example(m, f, d);
				if (score->error)
					return;
				have_header_example = true;
			}
		}
	}

	/* We don't fail ccanlint for this. */
	score->pass = true;
	if (have_info_example && have_header_example) {
		score->score = score->total;
		return;
	}

	if (!have_info_example)
		score_file_error(score, m->info_file, 0, "No Example: section");
	if (!have_header_example)
		score_file_error(score, mainh, 0, "No Example: section");

	score->score = have_info_example + have_header_example;
}

struct ccanlint examples_exist = {
	.key = "examples_exist",
	.name = "_info and main header file have Example: sections",
	.check = extract_examples,
	.needs = "info_exists main_header_exists"
};

REGISTER_TEST(examples_exist);
