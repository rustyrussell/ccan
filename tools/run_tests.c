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
#include "tools.h"

/* FIXME: Use build bug later. */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static struct test *tests = NULL;
static struct obj *objs = NULL;
static int verbose;

struct test_type
{
	const char *name;
	void (*buildfn)(const char *dir, struct test_type *t, const char *name);
	void (*runfn)(const char *name);
};

struct test
{
	struct test *next;
	struct test_type *type;
	char *name;
};

struct obj
{
	struct obj *next;
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

static char *obj_list(void)
{
	char *list = talloc_strdup(objs, "");
	struct obj *i;

	for (i = objs; i; i = i->next)
		list = talloc_asprintf_append(list, "%s ", i->name);

	/* FIXME */
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

	for (i = objs; i; i = i->next)
		unlink(talloc_asprintf(i, "%s.o", output_name(i->name)));
}

static void add_test(const char *testdir, const char *name, struct test_type *t)
{
	struct test *test = talloc(testdir, struct test);

	test->next = tests;
	test->type = t;
	test->name = talloc_asprintf(test, "%s/%s", testdir, name);
	tests = test;
}

static void add_obj(const char *testdir, const char *name)
{
	struct obj *obj = talloc(testdir, struct obj);

	obj->next = objs;
	obj->name = talloc_asprintf(obj, "%s/%s", testdir, name);
	objs = obj;
}

static int build(const char *dir, const char *name, int fail)
{
	const char *cmd;
	int ret;

	cmd = talloc_asprintf(name, "gcc " CFLAGS " %s -o %s %s %s -L. -lccan %s",
			      fail ? "-DFAIL" : "",
			      output_name(name), name, obj_list(),
			      verbose ? "" : "> /dev/null 2>&1");

	if (verbose)
		fprintf(stderr, "Running %s\n", cmd);

	ret = system(cmd);
	if (ret == -1)
		diag("cmd '%s' failed to execute", cmd);

	return ret;
}

static void compile_ok(const char *dir, struct test_type *t, const char *name)
{
	ok(build(dir, name, 0) == 0, "%s %s", t->name, name);
}

static void compile_fail(const char *dir, struct test_type *t, const char *name)
{
	if (build(dir, name, 0) != 0)
		fail("non-FAIL build %s", name);
	else
		ok(build(dir, name, 1) > 0, "%s %s", t->name, name);
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
	{ "api", compile_ok, run },
};

int main(int argc, char *argv[])
{
	DIR *dir;
	struct dirent *d;
	char *testdir;
	struct test *test;
	unsigned int num_tests = 0, num_objs = 0;

	if (argc > 1 && streq(argv[1], "--verbose")) {
		verbose = 1;
		argc--;
		argv++;
	}

	if (argc != 2)
		errx(1, "Usage: run_tests [--verbose] <dir>");

	testdir = talloc_asprintf(NULL, "%s/test", argv[1]);
	dir = opendir(testdir);
	if (!dir)
		err(1, "Opening '%s'", testdir);

	while ((d = readdir(dir)) != NULL) {
		unsigned int i;
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
			add_obj(testdir, d->d_name);
			num_objs++;
		}
	}

	plan_tests(num_tests + num_objs + (num_objs ? 1 : 0));
	/* First all the extra object compilations. */
	compile_objs();

	/* Do all the test compilations. */
	for (test = tests; test; test = test->next)
		test->type->buildfn(argv[1], test->type, test->name);

	cleanup_objs();

	/* Now run all the ones which wanted to run. */
	for (test = tests; test; test = test->next) {
		test->type->runfn(test->name);
		cleanup(test->name);
	}

	exit(exit_status());
}
