#include <tools/ccanlint/ccanlint.h>
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
#include <ccan/noerr/noerr.h>

static void *check_has_info(struct manifest *m)
{
	if (m->info_file)
		return NULL;
	return m;
}

static const char *describe_has_info(struct manifest *m, void *check_result)
{
	return "You have no _info.c file.\n\n"
	"The file _info.c contains the metadata for a ccan package: things\n"
	"like the dependencies, the documentation for the package as a whole\n"
	"and license information.\n";
}

static const char template[] = 
	"#include <string.h>\n"
	"#include \"config.h\"\n"
	"\n"
	"/**\n"
	" * %s - YOUR-ONE-LINE-DESCRIPTION-HERE\n"
	" *\n"
	" * This code ... YOUR-BRIEF-SUMMARY-HERE\n"
	" *\n"
	" * Example:\n"
	" *	FULLY-COMPILABLE-INDENTED-TRIVIAL-BUT-USEFUL-EXAMPLE-HERE\n"
	" */\n"
	"int main(int argc, char *argv[])\n"
	"{\n"
	"	/* Expect exactly one argument */\n"
	"	if (argc != 2)\n"
	"		return 1;\n"
	"\n"
	"	if (strcmp(argv[1], \"depends\") == 0) {\n"
	"		PRINTF-CCAN-PACKAGES-YOU-NEED-ONE-PER-LINE-IF-ANY\n"
	"		return 0;\n"
	"	}\n"
	"\n"
	"	return 1;\n"
	"}\n";

static void create_info_template(struct manifest *m, void *check_result)
{
	FILE *info;

	if (!ask("Should I create a template _info.c file for you?"))
		return;

	info = fopen("_info.c", "w");
	if (!info)
		err(1, "Trying to create a template _info.c");

	if (fprintf(info, template, m->basename) < 0) {
		unlink_noerr("_info.c");
		err(1, "Writing template into _info.c");
	}
	fclose(info);
}

struct ccanlint has_info = {
	.name = "Has _info.c file",
	.check = check_has_info,
	.describe = describe_has_info,
	.handle = create_info_template,
};
