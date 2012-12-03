#include "tools.h"
#include <stdlib.h>
#include <stdio.h>
#include <ccan/err/err.h>
#include <ccan/str/str.h>

int main(int argc, char *argv[])
{
	char **deps;
	unsigned int i;
	bool compile = false;
	bool recurse = true;
	bool ccan = true;
	const char *style = "depends";

	if (argv[1] && streq(argv[1], "--direct")) {
		argv++;
		argc--;
		recurse = false;
	}
	if (argv[1] && streq(argv[1], "--compile")) {
		argv++;
		argc--;
		compile = true;
	}
	if (argv[1] && streq(argv[1], "--non-ccan")) {
		argv++;
		argc--;
		ccan = false;
	}
	if (argv[1] && streq(argv[1], "--tests")) {
		argv++;
		argc--;
		style = "testdepends";
	}
	if (argc != 2)
		errx(1, "Usage: ccan_depends [--direct] [--compile] [--non-ccan] [--tests] <dir>\n"
		        "Spits out all the ccan dependencies (recursively unless --direct)");

	/* We find depends without compiling by looking for ccan/ */
	if (!ccan && !compile)
		errx(1, "--non-ccan needs --compile");

	if (compile)
		deps = get_deps(NULL, argv[1], style, recurse, compile_info);
	else
		deps = get_safe_ccan_deps(NULL, argv[1], style, recurse);

	for (i = 0; deps[i]; i++)
		if (strstarts(deps[i], "ccan/") == ccan)
			printf("%s\n", deps[i]);
	return 0;
}
