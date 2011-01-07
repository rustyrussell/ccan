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

static struct ccan_file *main_header(struct manifest *m)
{
	struct ccan_file *f;

	list_for_each(&m->h_files, f, list) {
		if (strstarts(f->name, m->basename)
		    && strlen(f->name) == strlen(m->basename) + 2)
			return f;
	}
	/* Should not happen: we depend on has_main_header */
	abort();
}

static void check_includes_build(struct manifest *m,
				 bool keep,
				 unsigned int *timeleft, struct score *score)
{
	char *contents;
	char *tmpsrc, *tmpobj, *cmdout;
	int fd;
	struct ccan_file *mainh = main_header(m);

	tmpsrc = maybe_temp_file(m, "-included.c", keep, mainh->fullname);
	tmpobj = maybe_temp_file(m, ".o", keep, tmpsrc);

	fd = open(tmpsrc, O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (fd < 0)
		err(1, "Creating temporary file %s", tmpsrc);

	contents = talloc_asprintf(tmpsrc, "#include <ccan/%s/%s.h>\n",
				   m->basename, m->basename);
	if (write(fd, contents, strlen(contents)) != strlen(contents))
		err(1, "writing to temporary file %s", tmpsrc);
	close(fd);

	if (compile_object(score, tmpsrc, ccan_dir, "", tmpobj, &cmdout)) {
		score->pass = true;
		score->score = score->total;
	} else {
		score->error = talloc_asprintf(score,
				       "#include of the main header file:\n%s",
				       cmdout);
	}
}

struct ccanlint includes_build = {
	.key = "main_header_compiles",
	.name = "Modules main header compiles",
	.check = check_includes_build,
	.can_run = can_build,
	.needs = "depends_exist main_header_exists"
};

REGISTER_TEST(includes_build);
