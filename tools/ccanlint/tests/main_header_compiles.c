#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
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
				 unsigned int *timeleft, struct score *score)
{
	char *contents;
	char *tmpsrc, *tmpobj, *cmdout;
	int fd;
	struct ccan_file *mainh = main_header(m);

	tmpsrc = temp_file(m, "-included.c", mainh->fullname);
	tmpobj = temp_file(m, ".o", tmpsrc);

	fd = open(tmpsrc, O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (fd < 0)
		err(1, "Creating temporary file %s", tmpsrc);

	contents = tal_fmt(tmpsrc, "#include <ccan/%s/%s.h>\n",
			   m->modname, m->basename);
	if (write(fd, contents, strlen(contents)) != strlen(contents))
		err(1, "writing to temporary file %s", tmpsrc);
	close(fd);

	if (compile_object(score, tmpsrc, ccan_dir, compiler, cflags,
			   tmpobj, &cmdout)) {
		score->pass = true;
		score->score = score->total;
	} else {
		score->error = tal_fmt(score,
				       "#include of the main header file:\n%s",
				       cmdout);
	}
}

struct ccanlint main_header_compiles = {
	.key = "main_header_compiles",
	.name = "Modules main header compiles",
	.check = check_includes_build,
	.can_run = can_build,
	.needs = "depends_exist main_header_exists info_ported"
};

REGISTER_TEST(main_header_compiles);
