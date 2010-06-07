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

static char *add_dep(char *sofar, struct manifest *m, const char *dep)
{
	struct stat st;
	struct ccan_file *f;

	f = new_ccan_file(m, ccan_dir, talloc_strdup(m, dep));
	if (stat(f->fullname, &st) != 0) {
		return talloc_asprintf_append(sofar,
					      "ccan/%s: expected it in"
					      " directory %s\n",
					      dep, f->fullname);
	}

	list_add_tail(&m->dep_dirs, &f->list);
	return sofar;
}

static void *check_depends_exist(struct manifest *m, unsigned int *timeleft)
{
	unsigned int i;
	char *report = NULL;
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

		report = add_dep(report, m, deps[i]);
	}
	return report;
}

static const char *describe_depends_exist(struct manifest *m,
					  void *check_result)
{
	return talloc_asprintf(check_result,
			       "The following dependencies are are expected:\n"
			       "%s", (char *)check_result);
}

struct ccanlint depends_exist = {
	.key = "depends-exist",
	.name = "Module's CCAN dependencies are present",
	.total_score = 1,
	.check = check_depends_exist,
	.describe = describe_depends_exist,
};

REGISTER_TEST(depends_exist, NULL);
