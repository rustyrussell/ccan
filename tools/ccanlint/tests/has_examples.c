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

	name = maybe_temp_file(m, ".c", keep, 
			       talloc_asprintf(m, "%s/example-%s-%s",
					       talloc_dirname(m,
							      source->fullname),
					       source->name,
					       example->function));
	f = new_ccan_file(m, talloc_dirname(m, name), talloc_basename(m, name));
	talloc_steal(f, name);
	list_add(&m->examples, &f->list);

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
struct score {
	bool info_example, header_example;
	char *error;
};

static void *extract_examples(struct manifest *m,
			      bool keep,
			      unsigned int *timeleft)
{
	struct ccan_file *f;
	struct doc_section *d;
	struct score *score = talloc(m, struct score);

	score->info_example = score->header_example = false;
	score->error = NULL;

	list_for_each(get_ccan_file_docs(m->info_file), d, list) {
		if (streq(d->type, "example")) {
			score->error = add_example(m, m->info_file, keep, d);;
			if (score->error)
				return score;
			score->info_example = true;
		}
	}

	/* Check main header. */
	list_for_each(&m->h_files, f, list) {
		if (!strstarts(f->name, m->basename)
		    || strlen(f->name) != strlen(m->basename) + 2)
			continue;

		list_for_each(get_ccan_file_docs(f), d, list) {
			if (streq(d->type, "example")) {
				score->error = add_example(m, f, keep, d);
				if (score->error)
					return score;
				score->header_example = true;
			}
		}
	}
	return score;
}

static unsigned int score_examples(struct manifest *m, void *check_result)
{
	struct score *score = check_result;
	int total = 0;

	if (score->error)
		return 0;
	total += score->info_example;
	total += score->header_example;
	return total;
}

static const char *describe_examples(struct manifest *m,
				     void *check_result)
{
	struct score *score = check_result;
	char *descrip = NULL;

	if (score->error)
		return score->error;

	if (!score->info_example)
		descrip = talloc_asprintf(score,
		"Your _info file has no module example.\n\n"
		"There should be an Example: section of the _info documentation\n"
		"which provides a concise toy program which uses your module\n");

	if (!score->header_example)
		descrip = talloc_asprintf(score,
		 "%sMain header file file has no examples\n\n"
		 "There should be an Example: section for each public function\n"
		  "demonstrating its use\n", descrip ? descrip : "");

	return descrip;
}

struct ccanlint has_examples = {
	.key = "has-examples",
	.name = "_info and header files have examples",
	.score = score_examples,
	.check = extract_examples,
	.describe = describe_examples,
	.total_score = 2,
};

REGISTER_TEST(has_examples, &has_info, NULL);
