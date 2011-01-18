#include <tools/ccanlint/ccanlint.h>
#include <tools/doc_extract.h>
#include <tools/tools.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <ccan/str/str.h>
#include <ccan/talloc/talloc.h>
#include <ccan/noerr/noerr.h>
#include <ccan/grab_file/grab_file.h>

REGISTER_TEST(info_documentation_exists);

static void create_info_template_doc(struct manifest *m, struct score *score)
{
	int fd = open("_info.new", O_WRONLY|O_CREAT|O_EXCL, 0666);
	FILE *new;
	char *oldcontents;

	if (fd < 0 || !(new = fdopen(fd, "w")))
		err(1, "Creating _info.new to insert documentation");

	if (fprintf(new,
		    "/**\n"
		    " * %s - [[ONE LINE DESCRIPTION HERE]]\n"
		    " *\n"
		    " * Paragraphs why %s exists and where to use it.\n"
		    " *\n"
		    " * Followed by an Example: section with a standalone\n"
		    " * (trivial and usually useless) program\n"
		    " */\n", m->basename, m->basename) < 0) {
		unlink_noerr("_info.new");
		err(1, "Writing to _info.new to insert documentation");
	}

	oldcontents = grab_file(m, "_info", NULL);
	if (!oldcontents) {
		unlink_noerr("_info.new");
		err(1, "Reading _info");
	}
	if (fprintf(new, "%s", oldcontents) < 0) {
		unlink_noerr("_info.new");
		err(1, "Appending _info to _info.new");
	}
	if (fclose(new) != 0) {
		unlink_noerr("_info.new");
		err(1, "Closing _info.new");
	}
	if (!move_file("_info.new", "_info")) {
		unlink_noerr("_info.new");
		err(1, "Renaming _info.new to _info");
	}
}

static void check_info_documentation_exists(struct manifest *m,
					 bool keep,
					 unsigned int *timeleft,
					 struct score *score)
{
	struct list_head *infodocs = get_ccan_file_docs(m->info_file);
	struct doc_section *d;
	bool summary = false, description = false;

	list_for_each(infodocs, d, list) {
		if (!streq(d->function, m->basename))
			continue;
		if (streq(d->type, "summary"))
			summary = true;
		if (streq(d->type, "description"))
			description = true;
	}

	if (summary && description) {
		score->score = score->total;
		score->pass = true;
	} else if (!summary) {
		score->error = talloc_strdup(score,
		"_info file has no module documentation.\n\n"
		"CCAN modules use /**-style comments for documentation: the\n"
		"overall documentation belongs in the _info metafile.\n");
		info_documentation_exists.handle = create_info_template_doc;
	} else if (!description)  {
		score->error = talloc_strdup(score,
		"_info file has no module description.\n\n"
		"The lines after the first summary line in the _info file\n"
		"documentation should describe the purpose and use of the\n"
		"overall package\n");
	}
}

struct ccanlint info_documentation_exists = {
	.key = "info_documentation_exists",
	.name = "Module has documentation in _info",
	.check = check_info_documentation_exists,
	.needs = "info_exists"
};

