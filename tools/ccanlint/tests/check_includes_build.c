#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/talloc/talloc.h>
#include <ccan/grab_file/grab_file.h>
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

static void *check_includes_build(struct manifest *m)
{
	char *contents;
	char *tmpfile, *objfile;
	int fd;

	tmpfile = temp_file(m, ".c");
	objfile = temp_file(m, ".o");

	fd = open(tmpfile, O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (fd < 0)
		return talloc_asprintf(m, "Creating temporary file: %s",
				       strerror(errno));

	contents = talloc_asprintf(tmpfile, "#include <ccan/%s/%s.h>\n",
				   m->basename, m->basename);
	if (write(fd, contents, strlen(contents)) != strlen(contents)) {
		close(fd);
		return "Failure writing to temporary file";
	}
	close(fd);

	return compile_object(m, objfile, tmpfile);
}

static const char *describe_includes_build(struct manifest *m,
					   void *check_result)
{
	return talloc_asprintf(check_result, 
			       "#include of the main header file:\n"
			       "%s", (char *)check_result);
}

struct ccanlint includes_build = {
	.name = "Can compile against main header",
	.total_score = 1,
	.check = check_includes_build,
	.describe = describe_includes_build,
	.can_run = can_build,
};

REGISTER_TEST(includes_build, &depends_exist, NULL);
