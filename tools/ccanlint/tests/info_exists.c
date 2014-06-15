#include <tools/ccanlint/ccanlint.h>
#include <ccan/tal/tal.h>
#include <ccan/tal/str/str.h>
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

static void check_has_info(struct manifest *m,
			   unsigned int *timeleft,
			   struct score *score)
{
	if (m->info_file) {
		score->pass = true;
		score->score = score->total;
		add_info_options(m->info_file);
	} else {
		score->error = tal_strdup(score,
	"You have no _info file.\n\n"
	"The file _info contains the metadata for a ccan package: things\n"
	"like the dependencies, the documentation for the package as a whole\n"
	"and license information.\n");
	}
}

static const char template[] =
	"#include \"config.h\"\n"
	"#include <stdio.h>\n"
	"#include <string.h>\n"
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

static void create_info_template(struct manifest *m, struct score *score)
{
	FILE *info;
	const char *filename;

	if (!ask("Should I create a template _info file for you?"))
		return;

	filename = tal_fmt(m, "%s/%s", m->dir, "_info");
	info = fopen(filename, "w");
	if (!info)
		err(1, "Trying to create a template _info in %s", filename);

	if (fprintf(info, template, m->modname) < 0) {
		unlink_noerr(filename);
		err(1, "Writing template into %s", filename);
	}
	fclose(info);
}

struct ccanlint info_exists = {
	.key = "info_exists",
	.name = "Module has _info file",
	.compulsory = true,
	.check = check_has_info,
	.handle = create_info_template,
	.needs = ""
};

REGISTER_TEST(info_exists);
