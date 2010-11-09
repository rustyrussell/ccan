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

static void add_dep(struct manifest *m, const char *dep, struct score *score)
{
	struct stat st;
	struct ccan_file *f;

	f = new_ccan_file(m, ccan_dir, talloc_strdup(m, dep));
	if (stat(f->fullname, &st) != 0) {
		score->error = "Depends don't exist";
		score_file_error(score, f, 0, "could not stat");
	} else
		list_add_tail(&m->dep_dirs, &f->list);
}

static void check_depends_exist(struct manifest *m,
				bool keep,
				unsigned int *timeleft, struct score *score)
{
	unsigned int i;
	char **deps;
	char *updir = talloc_strdup(m, m->dir);

	*strrchr(updir, '/') = '\0';

	if (safe_mode)
		deps = get_safe_ccan_deps(m, m->dir, true,
					  &m->info_file->compiled);
	else
		deps = get_deps(m, m->dir, true, &m->info_file->compiled);

	for (i = 0; deps[i]; i++) {
		if (!strstarts(deps[i], "ccan/"))
			continue;

		add_dep(m, deps[i], score);
	}
	if (!score->error) {
		score->pass = true;
		score->score = score->total;
	}
}

struct ccanlint depends_exist = {
	.key = "depends-exist",
	.name = "Module's CCAN dependencies are present",
	.check = check_depends_exist,
};

REGISTER_TEST(depends_exist, NULL);
