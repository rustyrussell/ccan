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

static char *obj_list(const struct manifest *m)
{
	char *list = talloc_strdup(m, "");
	struct ccan_file *i;

	/* Other CCAN deps. */
	list_for_each(&m->dep_dirs, i, list) {
		if (i->compiled)
			list = talloc_asprintf_append(list, "%s ", i->compiled);
	}
	return list;
}

static char *lib_list(const struct manifest *m)
{
	unsigned int i, num;
	char **libs = get_libs(m, ".", &num, &m->info_file->compiled);
	char *ret = talloc_strdup(m, "");

	for (i = 0; i < num; i++)
		ret = talloc_asprintf_append(ret, "-l%s ", libs[i]);
	return ret;
}

static void check_use_build(struct manifest *m,
			    bool keep,
			    unsigned int *timeleft, struct score *score)
{
	char *contents;
	char *tmpfile;
	char *basename = talloc_asprintf(m, "%s/example.c", m->dir);
	int fd;

	tmpfile = maybe_temp_file(m, ".c", keep, basename);

	fd = open(tmpfile, O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (fd < 0)
		err(1, "Creating temporary file %s", tmpfile);

	contents = talloc_asprintf(tmpfile,
				   "#include <ccan/%s/%s.h>\n"
				   "int main(void)\n"
				   "{\n"
				   "	return 0;\n"
				   "}\n",
				   m->basename, m->basename);
	if (write(fd, contents, strlen(contents)) != strlen(contents))
		err(1, "Failure writing to temporary file %s", tmpfile);
	close(fd);

	score->error = compile_and_link(m, tmpfile, ccan_dir, obj_list(m), "",
					lib_list(m),
					maybe_temp_file(m, "", keep, tmpfile));
	if (!score->error) {
		score->pass = true;
		score->score = score->total;
	}
}

struct ccanlint check_build = {
	.key = "check-link",
	.name = "Module can be linked against trivial program",
	.check = check_use_build,
	.can_run = can_build,
};

REGISTER_TEST(check_build, &build, NULL);
