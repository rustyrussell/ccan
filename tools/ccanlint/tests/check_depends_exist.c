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
	char *dir;
	struct stat st;
	struct ccan_file *f;

	dir = talloc_asprintf(m, "../%s", dep);
	if (stat(dir, &st) != 0) {
		return talloc_asprintf_append(sofar,
					      "ccan/%s: expected it in"
					      " directory %s\n",
					      dep, dir);
	}

	f = new_ccan_file(m, dir);
	list_add_tail(&m->dep_dirs, &f->list);
	return sofar;
}

static void *check_depends_exist(struct manifest *m)
{
	unsigned int i;
	char *report = NULL;
	char **deps;

	if (safe_mode)
		deps = get_safe_ccan_deps(m, "..", m->basename, true);
	else
		deps = get_deps(m, "..", m->basename, true);

	for (i = 0; deps[i]; i++) {
		if (!strstarts(deps[i], "ccan/"))
			continue;

		report = add_dep(report, m, deps[i] + strlen("ccan/"));
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
	.name = "CCAN dependencies are present",
	.total_score = 1,
	.check = check_depends_exist,
	.describe = describe_depends_exist,
};

REGISTER_TEST(depends_exist, NULL);
