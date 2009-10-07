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

static const char *can_build(struct manifest *m)
{
	if (safe_mode)
		return "Safe mode enabled";
	return NULL;
}

/* FIXME: recursive ccanlint if they ask for it. */
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

static void *check_depends_built(struct manifest *m)
{
	struct ccan_file *i;
	struct stat st;
	char *report = NULL;

	list_for_each(&m->dep_dirs, i, list) {
		char *objfile;

		if (!expect_obj_file(i->name))
			continue;

		objfile = talloc_asprintf(m, "%s.o", i->name);
		if (stat(objfile, &st) != 0) {
			report = talloc_asprintf_append(report,
							"object file %s\n",
							objfile);
		} else {
			struct ccan_file *f = new_ccan_file(m, objfile);
			list_add_tail(&m->dep_objs, &f->list);
		}
			
	}

	/* We may need libtap for testing, unless we're "tap" */
	if (!streq(m->basename, "tap")
	    && (!list_empty(&m->run_tests) || !list_empty(&m->api_tests))) {
		if (stat("../tap.o", &st) != 0) {
			report = talloc_asprintf_append(report,
							"object file ../tap.o"
							" (for tests)\n");
		}
	}

	return talloc_steal(m, report);
}

static const char *describe_depends_built(struct manifest *m,
					  void *check_result)
{
	return talloc_asprintf(check_result, 
			       "The following dependencies are not built:\n"
			       "%s", (char *)check_result);
}

struct ccanlint depends_built = {
	.name = "CCAN dependencies are built",
	.total_score = 1,
	.check = check_depends_built,
	.describe = describe_depends_built,
	.can_run = can_build,
};

REGISTER_TEST(depends_built, &depends_exist, NULL);
