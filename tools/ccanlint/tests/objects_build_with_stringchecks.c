#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/str/str.h>
#include <ccan/foreach/foreach.h>
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
#include "reduce_features.h"

static const char *uses_stringfuncs(struct manifest *m)
{
	struct list_head *list;

	foreach_ptr(list, &m->c_files, &m->h_files) {
		struct ccan_file *i;
		char *match;

		list_for_each(list, i, list) {
			if (tal_strreg(m, get_ccan_file_contents(i),
				   "(isalnum|isalpha|isascii|isblank|iscntrl"
				   "|isdigit|isgraph|islower|isprint|ispunct"
				   "|isspace|isupper|isxdigit"
				   "|strstr|strchr|strrchr)", &match)) {
				if (verbose > 2)
					printf("Matched '%s' in %s\n",
					       match, i->fullname);
				return NULL;
			}
		}
	}
	return "No ctype.h or string functions found";
}

static void write_str(int fd, const char *str)
{
	if (write(fd, str, strlen(str)) != strlen(str))
		err(1, "Writing to temporary file");
}

static int start_file(const char *filename)
{
	int fd;
	fd = open(filename, O_WRONLY|O_CREAT, 0600);
	write_str(fd, "#define CCAN_STR_DEBUG 1\n#include <ccan/str/str.h>\n");
	return fd;
}

static void test_compile(struct score *score,
			 struct ccan_file *file,
			 const char *filename,
			 const char *flags,
			 bool *errors,
			 bool *warnings)
{
	char *output, *compiled;

	compiled = temp_file(score, "", filename);
	if (!compile_object(score, filename, ccan_dir, compiler, flags,
			    compiled, &output)) {
		score_file_error(score, file, 0,
				 "Compiling object files:\n%s",
				 output);
		*errors = true;
	} else if (!streq(output, "")) {
		score_file_error(score, file, 0,
				 "Compiling object files gave warnings:\n%s",
				 output);
		*warnings = true;
	}
}

static struct ccan_file *get_main_header(struct manifest *m)
{
	struct ccan_file *f;

	list_for_each(&m->h_files, f, list) {
		if (strstarts(f->name, m->basename)
		    && strlen(f->name) == strlen(m->basename) + 2) {
			return f;
		}
	}
	/* Should not happen, since we passed main_header_exists! */
	errx(1, "No main header?");
}

static void build_objects_with_stringchecks(struct manifest *m,
					    unsigned int *timeleft,
					    struct score *score)
{
	struct ccan_file *i;
	bool errors = false, warnings = false;
	char *tmp, *flags;
	int tmpfd;

	/* FIXME:: We need -I so local #includes work outside normal dir. */
	flags = tal_fmt(score, "-I%s %s", m->dir, cflags);

	/* Won't work into macros, but will get inline functions. */
	if (list_empty(&m->c_files)) {
		char *line;
		i = get_main_header(m);
		tmp = temp_file(score, ".c", i->fullname);
		tmpfd = start_file(tmp);
		line = tal_fmt(score, "#include <ccan/%s/%s.h>\n",
			       m->modname, m->basename);
		write_str(tmpfd, line);
		close(tmpfd);
		test_compile(score, i, tmp, flags, &errors, &warnings);
	} else {
		list_for_each(&m->c_files, i, list) {
			tmp = temp_file(score, ".c", i->fullname);
			tmpfd = start_file(tmp);
			write_str(tmpfd, get_ccan_file_contents(i));
			close(tmpfd);
			test_compile(score, i, tmp, flags, &errors, &warnings);
		}
	}

	/* We don't fail ccanlint for this. */
	score->pass = true;

	score->total = 1;
	if (!errors) {
		score->score = !warnings;
	}
}

struct ccanlint objects_build_with_stringchecks = {
	.key = "objects_build_with_stringchecks",
	.name = "Module compiles with extra ctype.h and str function checks",
	.check = build_objects_with_stringchecks,
	.can_run = uses_stringfuncs,
	.needs = "objects_build main_header_exists"
};
REGISTER_TEST(objects_build_with_stringchecks);
