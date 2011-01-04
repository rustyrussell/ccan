/* Simple tool to create config.h.
 * Would be much easier with ccan modules, but deliberately standalone. */
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define DEFAULT_COMPILER "cc"
#define DEFAULT_FLAGS "-g -Wall -Wundef -Wmissing-prototypes -Wmissing-declarations -Wstrict-prototypes -Wold-style-definition"

#define OUTPUT_FILE "configurator.out"
#define INPUT_FILE "configuratortest.c"

static int verbose;

enum test_style {
	EXECUTE,
	OUTSIDE_MAIN,
	DEFINES_FUNC,
	INSIDE_MAIN
};

struct test {
	const char *name;
	enum test_style style;
	const char *depends;
	const char *fragment;
	bool done;
	bool answer;
};

static struct test tests[] = {
	{ "HAVE_ALIGNOF", INSIDE_MAIN, NULL,
	  "return __alignof__(double) > 0 ? 0 : 1;" },
	{ "HAVE_ATTRIBUTE_COLD", DEFINES_FUNC, NULL,
	  "static int __attribute__((cold)) func(int x) { return x; }" },
	{ "HAVE_ATTRIBUTE_CONST", DEFINES_FUNC, NULL,
	  "static int __attribute__((const)) func(int x) { return x; }" },
	{ "HAVE_ATTRIBUTE_MAY_ALIAS", OUTSIDE_MAIN, NULL,
	  "typedef short __attribute__((__may_alias__)) short_a;" },
	{ "HAVE_ATTRIBUTE_NORETURN", DEFINES_FUNC, NULL,
	  "#include <stdlib.h>\n"
	  "static void __attribute__((noreturn)) func(int x) { exit(x); }" },
	{ "HAVE_ATTRIBUTE_PRINTF", DEFINES_FUNC, NULL,
	  "static void __attribute__((format(__printf__, 1, 2))) func(const char *fmt, ...) { }" },
	{ "HAVE_ATTRIBUTE_UNUSED", OUTSIDE_MAIN, NULL,
	  "static int __attribute__((unused)) func(int x) { return x; }" },
	{ "HAVE_ATTRIBUTE_USED", OUTSIDE_MAIN, NULL,
	  "static int __attribute__((used)) func(int x) { return x; }" },
	{ "HAVE_BIG_ENDIAN", EXECUTE, NULL,
	  "union { int i; char c[sizeof(int)]; } u;\n"
	  "u.i = 0x01020304;\n"
	  "return u.c[0] == 0x01 && u.c[1] == 0x02 && u.c[2] == 0x03 && u.c[3] == 0x04 ? 0 : 1;" },
	{ "HAVE_BSWAP_64", DEFINES_FUNC, "HAVE_BYTESWAP_H",
	  "#include <byteswap.h>\n"
	  "static int func(int x) { return bswap_64(x); }" },
	{ "HAVE_BUILTIN_CHOOSE_EXPR", INSIDE_MAIN, NULL,
	  "return __builtin_choose_expr(1, 0, \"garbage\");" },
	{ "HAVE_BUILTIN_CLZ", INSIDE_MAIN, NULL,
	  "return __builtin_clz(1) == (sizeof(int)*8 - 1) ? 0 : 1;" },
	{ "HAVE_BUILTIN_CLZL", INSIDE_MAIN, NULL,
	  "return __builtin_clzl(1) == (sizeof(long)*8 - 1) ? 0 : 1;" },
	{ "HAVE_BUILTIN_CLZLL", INSIDE_MAIN, NULL,
	  "return __builtin_clzll(1) == (sizeof(long long)*8 - 1) ? 0 : 1;" },
	{ "HAVE_BUILTIN_CONSTANT_P", INSIDE_MAIN, NULL,
	  "return __builtin_constant_p(1) ? 0 : 1;" },
	{ "HAVE_BUILTIN_EXPECT", INSIDE_MAIN, NULL,
	  "return __builtin_expect(argc == 1, 1) ? 0 : 1;" },
	{ "HAVE_BUILTIN_FFSL", INSIDE_MAIN, NULL,
	  "return __builtin_ffsl(0L) == 0 ? 0 : 1;" },
	{ "HAVE_BUILTIN_FFSLL", INSIDE_MAIN, NULL,
	  "return __builtin_ffsll(0LL) == 0 ? 0 : 1;" },
	{ "HAVE_BUILTIN_POPCOUNTL", INSIDE_MAIN, NULL,
	  "return __builtin_popcountl(255L) == 8 ? 0 : 1;" },
	{ "HAVE_BUILTIN_TYPES_COMPATIBLE_P", INSIDE_MAIN, NULL,
	  "return __builtin_types_compatible_p(char *, int) ? 1 : 0;" },
	{ "HAVE_BYTESWAP_H", OUTSIDE_MAIN, NULL,
	  "#include <byteswap.h>\n" },
	{ "HAVE_COMPOUND_LITERALS", INSIDE_MAIN, NULL,
	  "char **foo = (char *[]) { \"x\", \"y\", \"z\" };\n"
	  "return foo[0] ? 0 : 1;" },
	{ "HAVE_FOR_LOOP_DECLARATION", INSIDE_MAIN, NULL,
	  "for (int i = 0; i < argc; i++) { return 0; };\n"
	  "return 1;" },
	{ "HAVE_FLEXIBLE_ARRAY_MEMBER", OUTSIDE_MAIN, NULL,
	  "struct foo { unsigned int x; int arr[]; };" },
	{ "HAVE_GETPAGESIZE", DEFINES_FUNC, NULL,
	  "#include <unistd.h>\n"
	  "static int func(void) { return getpagesize(); }" },
	{ "HAVE_LITTLE_ENDIAN", EXECUTE, NULL,
	  "union { int i; char c[sizeof(int)]; } u;\n"
	  "u.i = 0x01020304;\n"
	  "return u.c[0] == 0x04 && u.c[1] == 0x03 && u.c[2] == 0x02 && u.c[3] == 0x01 ? 0 : 1;" },
	{ "HAVE_MMAP", DEFINES_FUNC, NULL,
	  "#include <sys/mman.h>\n"
	  "static void *func(int fd) {\n"
	  "	return mmap(0, 65536, PROT_READ, MAP_SHARED, fd, 0);\n"
	  "}" },
	{ "HAVE_NESTED_FUNCTIONS", DEFINES_FUNC, NULL,
	  "static int func(int val) {\n"
	  "	auto void add(int val2);\n"
	  "	void add(int val2) { val += val2; }\n"
	  "	add(7);\n"
	  "	return val;\n"
	  "}" },
	{ "HAVE_STATEMENT_EXPR", INSIDE_MAIN, NULL,
	  "return ({ int x = argc; x == argc ? 0 : 1; });" },
	{ "HAVE_TYPEOF", INSIDE_MAIN, NULL,
	  "__typeof__(argc) i; i = argc; return i == argc ? 0 : 1;" },
	{ "HAVE_UTIME", DEFINES_FUNC, NULL,
	  "#include <sys/types.h>\n"
	  "#include <utime.h>\n"
	  "static int func(const char *filename) {\n"
	  "	struct utimbuf times = { 0 };\n"
	  "	return utime(filename, &times);\n"
	  "}" },
	{ "HAVE_WARN_UNUSED_RESULT", DEFINES_FUNC, NULL,
	  "#include <sys/types.h>\n"
	  "#include <utime.h>\n"
	  "static __attribute__((warn_unused_result)) int func(int i) {\n"
	  "	return i + 1;\n"
	  "}" },
};

static char *grab_fd(int fd)
{
	int ret;
	size_t max, size = 0;
	char *buffer;

	max = 16384;
	buffer = malloc(max+1);
	while ((ret = read(fd, buffer + size, max - size)) > 0) {
		size += ret;
		if (size == max)
			buffer = realloc(buffer, max *= 2);
	}
	if (ret < 0)
		err(1, "reading from command");
	buffer[size] = '\0';
	return buffer;
}

static char *run(const char *cmd, int *exitstatus)
{
	pid_t pid;
	int p[2];
	char *ret;
	int status;

	if (pipe(p) != 0)
		err(1, "creating pipe");

	pid = fork();
	if (pid == -1)
		err(1, "forking");

	if (pid == 0) {
		if (dup2(p[1], STDOUT_FILENO) != STDOUT_FILENO
		    || dup2(p[1], STDERR_FILENO) != STDERR_FILENO
		    || close(p[0]) != 0
		    || close(STDIN_FILENO) != 0
		    || open("/dev/null", O_RDONLY) != STDIN_FILENO)
			exit(128);

		status = system(cmd);
		if (WIFEXITED(status))
			exit(WEXITSTATUS(status));
		/* Here's a hint... */
		exit(128 + WTERMSIG(status));
	}

	close(p[1]);
	ret = grab_fd(p[0]);
	/* This shouldn't fail... */
	if (waitpid(pid, &status, 0) != pid)
		err(1, "Failed to wait for child");
	close(p[0]);
	if (WIFEXITED(status))
		*exitstatus = WEXITSTATUS(status);
	else
		*exitstatus = -WTERMSIG(status);
	return ret;
}

static char *connect_args(char *argv[], const char *extra)
{
	unsigned int i, len = strlen(extra) + 1;
	char *ret;

	for (i = 1; argv[i]; i++)
		len += 1 + strlen(argv[i]);

	ret = malloc(len);
	len = 0;
	for (i = 1; argv[i]; i++) {
		strcpy(ret + len, argv[i]);
		len += strlen(argv[i]);
		ret[len++] = ' ';
	}
	strcpy(ret + len, extra);
	return ret;
}

static struct test *find_test(const char *name)
{
	unsigned int i;

	for (i = 0; i < sizeof(tests)/sizeof(tests[0]); i++) {
		if (strcmp(tests[i].name, name) == 0)
			return &tests[i];
	}
	abort();
}

#define PRE_BOILERPLATE "/* Test program generated by configurator. */\n"
#define MAIN_START_BOILERPLATE "int main(int argc, char *argv[]) {\n"
#define USE_FUNC_BOILERPLATE "(void)func;\n"
#define MAIN_BODY_BOILERPLATE "return 0;\n"
#define MAIN_END_BOILERPLATE "}\n"

static bool run_test(const char *cmd, struct test *test)
{
	char *output;
	FILE *outf;
	int status;

	if (test->done)
		return test->answer;

	if (test->depends && !run_test(cmd, find_test(test->depends))) {
		test->answer = false;
		test->done = true;
		return test->answer;
	}

	outf = fopen(INPUT_FILE, "w");
	if (!outf)
		err(1, "creating %s", INPUT_FILE);

	fprintf(outf, "%s", PRE_BOILERPLATE);
	switch (test->style) {
	case EXECUTE:
	case INSIDE_MAIN:
		fprintf(outf, "%s", MAIN_START_BOILERPLATE);
		fprintf(outf, "%s", test->fragment);
		fprintf(outf, "%s", MAIN_END_BOILERPLATE);
		break;
	case OUTSIDE_MAIN:
		fprintf(outf, "%s", test->fragment);
		fprintf(outf, "%s", MAIN_START_BOILERPLATE);
		fprintf(outf, "%s", MAIN_BODY_BOILERPLATE);
		fprintf(outf, "%s", MAIN_END_BOILERPLATE);
		break;
	case DEFINES_FUNC:
		fprintf(outf, "%s", test->fragment);
		fprintf(outf, "%s", MAIN_START_BOILERPLATE);
		fprintf(outf, "%s", USE_FUNC_BOILERPLATE);
		fprintf(outf, "%s", MAIN_BODY_BOILERPLATE);
		fprintf(outf, "%s", MAIN_END_BOILERPLATE);
		break;
	}
	fclose(outf);

	if (verbose > 1)
		if (system("cat " INPUT_FILE) == -1);

	output = run(cmd, &status);
	if (status != 0 || strstr(output, "warning")) {
		if (verbose)
			printf("Compile %s for %s, status %i: %s\n",
			       status ? "fail" : "warning",
			       test->name, status, output);
		if (test->style == EXECUTE)
			errx(1, "Test for %s did not compile:\n%s",
			     test->name, output);
		test->answer = false;
		free(output);
	} else {
		/* Compile succeeded. */
		free(output);
		/* We run INSIDE_MAIN tests for sanity checking. */
		if (test->style == EXECUTE || test->style == INSIDE_MAIN) {
			output = run("./" OUTPUT_FILE, &status);
			if (test->style == INSIDE_MAIN && status != 0)
				errx(1, "Test for %s failed with %i:\n%s",
				     test->name, status, output);
			if (verbose && status)
				printf("%s exited %i\n", test->name, status);
			free(output);
		}
		test->answer = (status == 0);
	}
	test->done = true;
	return test->answer;
}

int main(int argc, char *argv[])
{
	char *cmd;
	char *default_args[] = { "", DEFAULT_COMPILER, DEFAULT_FLAGS, NULL };
	unsigned int i;

	if (argc > 1) {
		if (strcmp(argv[1], "--help") == 0) {
			printf("Usage: configurator [-v] [<compiler> <flags>...]\n"
			       "  <compiler> <flags> will have \"-o <outfile> <infile.c>\" appended\n"
			       "Default: %s %s\n",
			       DEFAULT_COMPILER, DEFAULT_FLAGS);
			exit(0);
		}
		if (strcmp(argv[1], "-v") == 0) {
			argc--;
			argv++;
			verbose = 1;
		} else if (strcmp(argv[1], "-vv") == 0) {
			argc--;
			argv++;
			verbose = 2;
		}
	}

	if (argc == 1)
		argv = default_args;

	cmd = connect_args(argv, "-o " OUTPUT_FILE " " INPUT_FILE);
	for (i = 0; i < sizeof(tests)/sizeof(tests[0]); i++)
		run_test(cmd, &tests[i]);

	unlink(OUTPUT_FILE);
	unlink(INPUT_FILE);

	cmd[strlen(cmd) - strlen(" -o " OUTPUT_FILE " " INPUT_FILE)] = '\0';
	printf("/* Generated by CCAN configurator */\n");
	printf("#define CCAN_COMPILER \"%s\"\n", argv[1]);
	printf("#define CCAN_CFLAGS \"%s\"\n\n", cmd + strlen(argv[1]) + 1);
	for (i = 0; i < sizeof(tests)/sizeof(tests[0]); i++)
		printf("#define %s %u\n", tests[i].name, tests[i].answer);
	return 0;
}
