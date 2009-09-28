#include <err.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "ccan/tap/tap.h"
#include "ccan/talloc/talloc.h"
#include "ccan/str/str.h"
#include "ccan/array_size/array_size.h"
#include "tools.h"

static struct test *tests = NULL;
static struct obj *objs = NULL;
static int verbose;

struct test_type {
	const char *name;
	void (*buildfn)(const char *dir, struct test_type *t, const char *name,
			const char *apiobj, const char *libs);
	void (*runfn)(const char *name);
};

struct test {
	struct test *next;
	struct test_type *type;
	char *name;
};

struct obj {
	struct obj *next;
	bool generate;
	char *name;
};

static char *output_name(const char *name)
{
	char *ret;

	assert(strends(name, ".c"));

	ret = talloc_strdup(name, name);
	ret[strlen(ret) - 2] = '\0';
	return ret;
}

static char *obj_list(const char *dir)
{
	char *list = talloc_strdup(objs, "");
	struct obj *i;

	for (i = objs; i; i = i->next)
		list = talloc_asprintf_append(list, "%s ", i->name);

	/* FIXME */
	if (!streq(dir, "tap") && !strends(dir, "/tap"))
		list = talloc_asprintf_append(list, "ccan/tap/tap.o");
	return list;
}

static void compile_objs(void)
{
	struct obj *i;

	for (i = objs; i; i = i->next) {
		char *cmd = talloc_asprintf(i, "gcc " CFLAGS " -o %s.o -c %s%s",
					    output_name(i->name), i->name,
					    verbose ? "" : "> /dev/null 2>&1");
		ok(system(cmd) == 0, "%s", cmd);
	}
}

static void cleanup_objs(void)
{
	struct obj *i;

	for (i = objs; i; i = i->next) {
		if (!i->generate)
			continue;
		unlink(talloc_asprintf(i, "%s.o", output_name(i->name)));
	}
}

static void add_test(const char *testdir, const char *name, struct test_type *t)
{
	struct test *test = talloc(testdir, struct test);

	test->next = tests;
	test->type = t;
	test->name = talloc_asprintf(test, "%s/%s", testdir, name);
	tests = test;
}

static void add_obj(const char *testdir, const char *name, bool generate)
{
	struct obj *obj = talloc(testdir, struct obj);

	obj->next = objs;
	obj->name = talloc_asprintf(obj, "%s/%s", testdir, name);
	obj->generate = generate;
	objs = obj;
}

static int build(const char *dir, const char *name, const char *apiobj,
		 const char *libs, int fail)
{
	const char *cmd;
	int ret;

	cmd = talloc_asprintf(name, "gcc " CFLAGS " %s -o %s %s %s %s%s %s",
			      fail ? "-DFAIL" : "",
			      output_name(name), name, apiobj, obj_list(dir),
			      libs, verbose ? "" : "> /dev/null 2>&1");

	if (verbose)
		fprintf(stderr, "Running %s\n", cmd);

	ret = system(cmd);
	if (ret == -1)
		diag("cmd '%s' failed to execute", cmd);

	return ret;
}

static void compile_ok(const char *dir, struct test_type *t, const char *name,
		       const char *apiobj, const char *libs)
{
	ok(build(dir, name, "", libs, 0) == 0, "%s %s", t->name, name);
}

/* api tests get the API obj linked in as well. */
static void compile_api_ok(const char *dir, struct test_type *t,
			   const char *name, const char *apiobj,
			   const char *libs)
{
	ok(build(dir, name, apiobj, libs, 0) == 0, "%s %s", t->name, name);
}

static void compile_fail(const char *dir, struct test_type *t, const char *name,
			 const char *apiobj, const char *libs)
{
	if (build(dir, name, "", libs, 0) != 0)
		fail("non-FAIL build %s", name);
	else
		ok(build(dir, name, "", libs, 1) > 0, "%s %s", t->name, name);
}

static void no_run(const char *name)
{
}

static void run(const char *name)
{
	if (system(output_name(name)) != 0)
		fail("running %s had error", name);
}

static void cleanup(const char *name)
{
	unlink(output_name(name));
}

static struct test_type test_types[] = {
	{ "compile_ok", compile_ok, no_run },
	{ "compile_fail", compile_fail, no_run },
	{ "run", compile_ok, run },
	{ "api", compile_api_ok, run },
};

int main(int argc, char *argv[])
{
	DIR *dir;
	struct dirent *d;
	char *testdir, *cwd;
	const char *apiobj = "";
	char *libs = talloc_strdup(NULL, "");
	struct test *test;
	unsigned int num_tests = 0, num_objs = 0, i;

	if (argc > 1 && streq(argv[1], "--verbose")) {
		verbose = 1;
		argc--;
		argv++;
	}

	while (argc > 1 && strstarts(argv[1], "--lib=")) {
		libs = talloc_asprintf_append(libs, " -l%s",
					      argv[1] + strlen("--lib="));
		argc--;
		argv++;
	}

	if (argc > 1 && strstarts(argv[1], "--apiobj=")) {
		apiobj = argv[1] + strlen("--apiobj=");
		argc--;
		argv++;
	}

	if (argc < 2)
		errx(1, "Usage: run_tests [--verbose] [--apiobj=<obj>] <dir> [<extra-objs>...]");

	testdir = talloc_asprintf(NULL, "%s/test", argv[1]);
	dir = opendir(testdir);
	if (!dir)
		err(1, "Opening '%s'", testdir);

	while ((d = readdir(dir)) != NULL) {
		if (d->d_name[0] == '.' || !strends(d->d_name, ".c"))
			continue;

		for (i = 0; i < ARRAY_SIZE(test_types); i++) {
			if (strstarts(d->d_name, test_types[i].name)) {
				add_test(testdir, d->d_name, &test_types[i]);
				num_tests++;
				break;
			}
		}
		if (i == ARRAY_SIZE(test_types)) {
			add_obj(testdir, d->d_name, true);
			num_objs++;
		}
	}

	plan_tests(num_tests + num_objs + (num_objs ? 1 : 0));
	/* First all the extra object compilations. */
	compile_objs();

	/* Now add any object files from the command line */
	cwd = talloc_strdup(testdir, ".");
	for (i = 2; i < argc; i++)
		add_obj(cwd, argv[i], false);

	/* Do all the test compilations. */
	for (test = tests; test; test = test->next)
		test->type->buildfn(argv[1], test->type, test->name,
				    apiobj, libs);

	cleanup_objs();

	/* Now run all the ones which wanted to run. */
	for (test = tests; test; test = test->next) {
		test->type->runfn(test->name);
		cleanup(test->name);
	}

	exit(exit_status());
}
