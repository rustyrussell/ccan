#include "tools.h"
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include "string/string.h"
#include "talloc/talloc.h"

int main(int argc, char *argv[])
{
	char **deps;
	unsigned int i;
	bool compile = false;

	if (argv[1] && streq(argv[1], "--compile")) {
		argv++;
		argc--;
		compile = true;
	}
	if (argc != 2)
		errx(1, "Usage: ccan_depends [--compile] <dir>\n"
		        "Spits out all the ccan dependencies (recursively)");

	if (compile)
		deps = get_deps(talloc_autofree_context(), argv[1]);
	else
		deps = get_safe_ccan_deps(talloc_autofree_context(), argv[1]);

	for (i = 0; deps[i]; i++)
		if (strstarts(deps[i], "ccan/"))
			printf("%s\n", deps[i]);
	return 0;
}
