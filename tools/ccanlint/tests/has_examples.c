#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/talloc/talloc.h>
#include <ccan/str/str.h>
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
			 bool keep,
			 struct doc_section *example)
{
	char *name;
	unsigned int i;
	int fd;
	struct ccan_file *f;

	name = talloc_asprintf(m, "%s/example-%s-%s.c",
			       talloc_dirname(m,
					      source->fullname),
			       source->name,
			       example->function);
	/* example->function == 'struct foo' */
	while (strchr(name, ' '))
		*strchr(name, ' ') = '_';

	name = maybe_temp_file(m, ".c", keep, name);
	f = new_ccan_file(m, talloc_dirname(m, name), talloc_basename(m, name));
	talloc_steal(f, name);
	list_add_tail(&m->examples, &f->list);

	fd = open(f->fullname, O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (fd < 0)
		return talloc_asprintf(m, "Creating temporary file %s: %s",
				       f->fullname, strerror(errno));

	for (i = 0; i < example->num_lines; i++) {
		if (write(fd, example->lines[i], strlen(example->lines[i]))
		    != strlen(example->lines[i])
		    || write(fd, "\n", 1) != 1) {
			close(fd);
			return "Failure writing to temporary file";
		}
	}
	close(fd);
	return NULL;
}

/* FIXME: We should have one example per function in header. */
static void extract_examples(struct manifest *m,
			     bool keep,
			     unsigned int *timeleft,
			     struct score *score)
{
	struct ccan_file *f, *mainh = NULL; /* gcc complains uninitialized */
	struct doc_section *d;
	bool have_info_example = false, have_header_example = false;

	score->total = 2;
	list_for_each(get_ccan_file_docs(m->info_file), d, list) {
		if (streq(d->type, "example")) {
			score->error = add_example(m, m->info_file, keep, d);
			if (score->error)
				return;
			have_info_example = true;
		}
	}

	/* Check main header. */
	list_for_each(&m->h_files, f, list) {
		if (!strstarts(f->name, m->basename)
		    || strlen(f->name) != strlen(m->basename) + 2)
			continue;

		mainh = f;
		list_for_each(get_ccan_file_docs(f), d, list) {
			if (streq(d->type, "example")) {
				score->error = add_example(m, f, keep, d);
				if (score->error)
					return;
				have_header_example = true;
			}
		}
	}

	if (have_info_example && have_header_example) {
		score->score = score->total;
		score->pass = true;
		return;
	}

	score->error = "Expect examples in header and _info";
	if (!have_info_example)
		score_file_error(score, m->info_file, 0, "No Example: section");
	if (!have_header_example)
		score_file_error(score, mainh, 0, "No Example: section");

	score->score = have_info_example + have_header_example;
	/* We pass if we find any example. */
	score->pass = score->score != 0;
}

struct ccanlint has_examples = {
	.key = "examples_exist",
	.name = "_info and main header file have Example: sections",
	.check = extract_examples,
};

REGISTER_TEST(has_examples, &has_info, NULL);
