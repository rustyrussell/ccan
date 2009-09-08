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

static bool expect_obj_file(const char *dir)
{
	char *olddir;
	struct manifest *dep_man;
	bool has_c_files;

	olddir = talloc_getcwd(dir);
	if (!olddir)
		err(1, "Getting current directory");

	/* We will fail below if this doesn't exist. */
	if (chdir(dir) != 0)
		return false;

	dep_man = get_manifest(dir);
	if (chdir(olddir) != 0)
		err(1, "Returning to original directory '%s'", olddir);
	talloc_free(olddir);

	/* If it has C files, we expect an object file built from them. */
	has_c_files = !list_empty(&dep_man->c_files);
	talloc_free(dep_man);
	return has_c_files;
}

/* FIXME: recursive ccanlint if they ask for it. */
static char *add_dep(char *sofar, struct manifest *m, const char *dep)
{
	char *file, *dir;
	struct stat st;
	bool need_obj;

	dir = talloc_asprintf(m, "../%s", dep);
	need_obj = expect_obj_file(dir);

	if (need_obj) {
		file = talloc_asprintf(m, "../%s.o", dep);
		if (stat(file, &st) == 0) {
			struct ccan_file *f = new_ccan_file(m, file);
			list_add_tail(&m->dep_obj_files, &f->list);
			return sofar;
		}
	}

	if (stat(dir, &st) == 0) {
		if (!need_obj)
			return sofar;

		return talloc_asprintf_append(sofar,
					      "ccan/%s: isn't built (no %s)\n",
					      dep, file);
	}

	return talloc_asprintf_append(sofar,
				      "ccan/%s: could not find directory %s\n",
				      dep, dir);
}

static void *check_depends(struct manifest *m)
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

static const char *describe_depends(struct manifest *m, void *check_result)
{
	return talloc_asprintf(check_result, 
			       "The following dependencies are needed:\n"
			       "%s\n", (char *)check_result);
}

struct ccanlint depends = {
	.name = "CCAN dependencies are built",
	.total_score = 1,
	.check = check_depends,
	.describe = describe_depends,
};

REGISTER_TEST(depends, NULL);
