#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/talloc/talloc.h>
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

static char *strip_spaces(const void *ctx, char *line)
{
	char *p = talloc_strdup(ctx, line);
	unsigned int i, j;

	for (i = 0, j = 0; p[i]; i++) {
		if (!isspace(p[i]))
			p[j++] = p[i];
	}
	p[j] = '\0';
	return p;
}

static bool has_dep(struct manifest *m, const char *depname, bool tap_ok)
{
	struct ccan_file *f;

	if (tap_ok && streq(depname, "ccan/tap"))
		return true;

	/* We can include ourselves, of course. */
	if (streq(depname + strlen("ccan/"), m->basename))
		return true;

	list_for_each(&m->dep_dirs, f, list) {
		if (streq(f->name, depname))
			return true;
	}
	return false;
}

static void check_depends_accurate(struct manifest *m,
				   bool keep,
				   unsigned int *timeleft, struct score *score)
{
	struct list_head *list;

	foreach_ptr(list, &m->c_files, &m->h_files,
		    &m->run_tests, &m->api_tests,
		    &m->compile_ok_tests, &m->compile_fail_tests,
		    &m->other_test_c_files) {
		struct ccan_file *f;
		bool tap_ok;

		/* Including ccan/tap is fine for tests. */
		tap_ok = (list != &m->c_files && list != &m->h_files);

		list_for_each(list, f, list) {
			unsigned int i;
			char **lines = get_ccan_file_lines(f);

			for (i = 0; lines[i]; i++) {
				char *p;
				if (lines[i][strspn(lines[i], " \t")] != '#')
					continue;
				p = strip_spaces(f, lines[i]);
				if (!strstarts(p, "#include<ccan/")
				    && !strstarts(p, "#include\"ccan/"))
					continue;
				p += strlen("#include\"");
				if (!strchr(strchr(p, '/') + 1, '/'))
					continue;
				*strchr(strchr(p, '/') + 1, '/') = '\0';
				if (has_dep(m, p, tap_ok))
					continue;
				score->error = "Includes a ccan module"
					" not listed in _info";
				score_file_error(score, f, i+1, lines[i]);
			}
		}
	}

	if (!score->error) {
		score->pass = true;
		score->score = score->total;
	}
}

struct ccanlint depends_accurate = {
	.key = "depends-accurate",
	.name = "Module's CCAN dependencies are the only ccan files #included",
	.check = check_depends_accurate,
};

REGISTER_TEST(depends_accurate, &depends_exist, NULL);
