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

static int cleanup_testfile(char *testfile)
{
	unlink(testfile);
	return 0;
}

static char *obj_list(const struct manifest *m)
{
	char *list = talloc_strdup(m, "");
	struct ccan_file *i;

	/* Other CCAN deps. */
	list_for_each(&m->dep_objs, i, list)
		list = talloc_asprintf_append(list, "%s ", i->name);

	return list;
}

static char *lib_list(const struct manifest *m)
{
	unsigned int i, num;
	char **libs = get_libs(m, ".", ".", &num);
	char *ret = talloc_strdup(m, "");

	for (i = 0; i < num; i++)
		ret = talloc_asprintf_append(ret, "-l%s ", libs[i]);
	return ret;
}

static void *check_use_build(struct manifest *m)
{
	char *contents;
	char *tmpfile, *outfile;
	int fd;

	tmpfile = talloc_strdup(m, tempnam("/tmp", "ccanlint"));
	talloc_set_destructor(tmpfile, cleanup_testfile);
	outfile = talloc_strdup(m, tempnam("/tmp", "ccanlint"));
	talloc_set_destructor(outfile, cleanup_testfile);

	fd = open(tmpfile, O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (fd < 0)
		return talloc_asprintf(m, "Creating temporary file: %s",
				       strerror(errno));

	contents = talloc_asprintf(tmpfile,
				   "#include <ccan/%s/%s.h>\n"
				   "int main(void)\n"
				   "{\n"
				   "	return 0;\n"
				   "}\n",
				   m->basename, m->basename);
	if (write(fd, contents, strlen(contents)) != strlen(contents)) {
		close(fd);
		return "Failure writing to temporary file";
	}
	close(fd);

	return run_command(m, "cc " CFLAGS " -o %s -x c %s -x none %s %s",
			   outfile, tmpfile, obj_list(m), lib_list(m));
}

static const char *describe_use_build(struct manifest *m, void *check_result)
{
	return talloc_asprintf(check_result, 
			       "Linking against module:\n"
			       "%s", (char *)check_result);
}

struct ccanlint check_build = {
	.name = "Module can be used",
	.total_score = 1,
	.check = check_use_build,
	.describe = describe_use_build,
	.can_run = can_build,
};

REGISTER_TEST(check_build, &build, NULL);
