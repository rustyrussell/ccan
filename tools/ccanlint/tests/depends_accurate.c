#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/str/str.h>
#include <ccan/take/take.h>
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

static bool has_dep(struct manifest *m, char **deps, bool *used,
		    const char *depname)
{
	unsigned int i;

	/* We can include ourselves, of course. */
	if (streq(depname + strlen("ccan/"), m->modname))
		return true;

	for (i = 0; deps[i]; i++) {
		if (streq(deps[i], depname)) {
			used[i] = true;
			return true;
		}
	}
	return false;
}

static bool check_dep_includes(struct manifest *m,
			       char **deps, bool *used,
			       struct score *score,
			       struct ccan_file *f)
{
	unsigned int i;
	char **lines = get_ccan_file_lines(f);
	struct line_info *li = get_ccan_line_info(f);
	bool ok = true;

	for (i = 0; lines[i]; i++) {
		char *mod;
		if (!tal_strreg(f, lines[i],
				"^[ \t]*#[ \t]*include[ \t]*[<\"]"
				"(ccan/+.+)/+[^/]+\\.h", &mod))
			continue;

		if (has_dep(m, deps, used, mod))
			continue;

		/* FIXME: we can't be sure about conditional includes,
		 * so don't complain (handle common case of idempotent wrap) */
		if (!li[i].cond || li[i].cond == f->idempotent_cond) {
			score_file_error(score, f, i+1,
					 "%s not listed in _info", mod);
			ok = false;
		}
	}
	return ok;
}

static void check_depends_accurate(struct manifest *m,
				   unsigned int *timeleft, struct score *score)
{
	struct list_head *list;
	unsigned int i, core_deps, test_deps;
	char **deps, **tdeps;
	bool *used;
	bool ok = true;

	/* Get the *direct* dependencies. */
	if (safe_mode) {
		deps = get_safe_ccan_deps(m, m->dir, "depends", false);
		tdeps = get_safe_ccan_deps(m, m->dir, "testdepends", false);
	} else {
		deps = get_deps(m, m->dir, "depends", false,
				get_or_compile_info);
		tdeps = get_deps(m, m->dir, "testdepends", false,
				 get_or_compile_info);
	}

	core_deps = tal_count(deps) - 1;
	test_deps = tal_count(tdeps) - 1;

	used = tal_arrz(m, bool, core_deps + test_deps + 1);

	foreach_ptr(list, &m->c_files, &m->h_files) {
		struct ccan_file *f;

		list_for_each(list, f, list)
			ok &= check_dep_includes(m, deps, used, score, f);
	}

	for (i = 0; i < core_deps; i++) {
		if (!used[i] && strstarts(deps[i], "ccan/"))
			score_file_error(score, m->info_file, 0,
					 "%s is an unused dependency",
					 deps[i]);
	}

	/* Now remove NUL and append test dependencies to deps. */
	deps = tal_dup_arr(m, char *, take(deps), core_deps, test_deps + 2);
	memcpy(deps + core_deps, tdeps, sizeof(tdeps[0]) * test_deps);
	/* ccan/tap is given a free pass. */
	deps[core_deps + test_deps] = (char *)"ccan/tap";
	deps[core_deps + test_deps + 1] = NULL;

	foreach_ptr(list, &m->run_tests, &m->api_tests,
		    &m->compile_ok_tests, &m->compile_fail_tests,
		    &m->other_test_c_files) {
		struct ccan_file *f;

		list_for_each(list, f, list)
			ok &= check_dep_includes(m, deps, used, score, f);
	}

	for (i = core_deps; i < test_deps; i++) {
		if (!used[i])
			score_file_error(score, m->info_file, 0,
					 "%s is an unused test dependency",
					 deps[i]);
	}

	if (!score->error)
		score->score = score->total;

	/* We don't count unused dependencies as an error (yet!) */
	score->pass = ok;
}

struct ccanlint depends_accurate = {
	.key = "depends_accurate",
	.name = "Module's CCAN dependencies are the only CCAN files #included",
	.check = check_depends_accurate,
	.needs = "depends_exist info_compiles test_depends_exist headers_idempotent"
};

REGISTER_TEST(depends_accurate);
