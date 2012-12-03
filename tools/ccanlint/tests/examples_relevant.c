#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
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

static void examples_relevant_check(struct manifest *m,
				    unsigned int *timeleft,
				    struct score *score)
{
	struct ccan_file *f;
	struct doc_section *d;

	list_for_each(&m->h_files, f, list) {
		list_for_each(get_ccan_file_docs(f), d, list) {
			unsigned int i;
			bool found = false;

			if (!streq(d->type, "example"))
				continue;

			if (!d->function) {
				score_file_error(score, f, d->srcline+1,
						 "Function name not found in summary line");
				continue;
			}
			for (i = 0; i < d->num_lines; i++) {
				if (strstr(d->lines[i], d->function))
					found = true;
			}

			if (!found) {
				score_file_error(score, f, d->srcline+1,
						 "Example for %s doesn't"
						 " mention it", d->function);
			}
		}
	}

	if (!score->error) {
		score->score = score->total;
		score->pass = true;
		return;
	}
}

struct ccanlint examples_relevant = {
	.key = "examples_relevant",
	.name = "Example: sections demonstrate appropriate function",
	.check = examples_relevant_check,
	.needs = "examples_exist"
};

REGISTER_TEST(examples_relevant);
