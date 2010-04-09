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
	struct manifest *dep_man;
	bool has_c_files;

	dep_man = get_manifest(dir, dir);

	/* If it has C files, we expect an object file built from them. */
	has_c_files = !list_empty(&dep_man->c_files);
	talloc_free(dep_man);
	return has_c_files;
}

static void *check_depends_built(struct manifest *m, unsigned int *timeleft)
{
	struct ccan_file *i;
	struct stat st;
	char *report = NULL;

	list_for_each(&m->dep_dirs, i, list) {
		char *objfile;

		if (!expect_obj_file(i->fullname))
			continue;

		objfile = talloc_asprintf(m, "%s.o", i->fullname);
		if (stat(objfile, &st) != 0) {
			report = talloc_asprintf_append(report,
							"object file %s\n",
							objfile);
		} else {
			struct ccan_file *f = new_ccan_file(m, "", objfile);
			list_add_tail(&m->dep_objs, &f->list);
		}
			
	}

	/* We may need libtap for testing, unless we're "tap" */
	if (!streq(m->basename, "tap")
	    && (!list_empty(&m->run_tests) || !list_empty(&m->api_tests))) {
		char *tapobj = talloc_asprintf(m, "%s/ccan/tap.o", ccan_dir);
		if (stat(tapobj, &st) != 0) {
			report = talloc_asprintf_append(report,
							"object file %s"
							" (for tests)\n",
							tapobj);
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
	.key = "depends-built",
	.name = "Module's CCAN dependencies are already built",
	.total_score = 1,
	.check = check_depends_built,
	.describe = describe_depends_built,
	.can_run = can_build,
};

REGISTER_TEST(depends_built, &depends_exist, NULL);
