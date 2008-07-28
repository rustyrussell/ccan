#include "tools.h"
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include "string/string.h"

int main(int argc, char *argv[])
{
	char **deps;
	unsigned int i;

	if (argc != 2)
		errx(1, "Usage: ccan_depends <dir>\n"
		        "Spits out all the ccan dependencies (recursively)");

	deps = get_deps(NULL, argv[1]);
	for (i = 0; deps[i]; i++)
		if (strstarts(deps[i], "ccan/"))
			printf("%s\n", deps[i]);
	return 0;
}
