#include <tools/ccanlint/ccanlint.h>
#include <tools/doc_extract.h>
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

struct info_docs
{
	bool summary;
	bool description;
	bool example;
};

static void *check_has_info_documentation(struct manifest *m)
{
	struct list_head *infodocs = get_ccan_file_docs(m->info_file);
	struct doc_section *d;
	struct info_docs id = { false, false, false };

	list_for_each(infodocs, d, list) {
		if (!streq(d->function, m->basename))
			continue;
		if (streq(d->type, "summary"))
			id.summary = true;
		if (streq(d->type, "description"))
			id.description = true;
		if (streq(d->type, "example"))
			id.example = true;
	}

	if (id.summary && id.description && id.example)
		return NULL;
	return talloc_memdup(m, &id, sizeof(id));
}

/* This is defined below. */
extern struct ccanlint has_info_documentation;

static void create_info_template_doc(struct manifest *m, void *check_result)
{
	int fd = open("_info.c.new", O_WRONLY|O_CREAT|O_EXCL, 0666);
	FILE *new;
	char *oldcontents;

	if (fd < 0 || !(new = fdopen(fd, "w")))
		err(1, "Creating _info.c.new to insert documentation");

	if (fprintf(new,
		    "/**\n"
		    " * %s - [[ONE LINE DESCRIPTION HERE]]\n"
		    " *\n"
		    " * Paragraphs why %s exists and where to use it.\n"
		    " *\n"
		    " * Followed by an Example: section with a standalone\n"
		    " * (trivial and usually useless) program\n"
		    " */\n", m->basename, m->basename) < 0) {
		unlink_noerr("_info.c.new");
		err(1, "Writing to _info.c.new to insert documentation");
	}

	oldcontents = grab_file(m, "_info.c", NULL);
	if (!oldcontents) {
		unlink_noerr("_info.c.new");
		err(1, "Reading _info.c");
	}
	if (fprintf(new, "%s", oldcontents) < 0) {
		unlink_noerr("_info.c.new");
		err(1, "Appending _info.c to _info.c.new");
	}
	if (fclose(new) != 0) {
		unlink_noerr("_info.c.new");
		err(1, "Closing _info.c.new");
	}
	if (rename("_info.c.new", "_info.c") != 0) {
		unlink_noerr("_info.c.new");
		err(1, "Renaming _info.c.new to _info.c");
	}
}

static const char *describe_has_info_documentation(struct manifest *m,
						   void *check_result)
{
	struct info_docs *id = check_result;
	char *reason = talloc_strdup(m, "");

	if (!id->summary) {
		has_info_documentation.handle = create_info_template_doc;
		reason = talloc_asprintf_append(reason,
		"Your _info.c has no module documentation.\n\n"
		"CCAN modules use /**-style comments for documentation: the\n"
	        "overall documentation belongs in the _info.c metafile.\n");
	}
	if (!id->description)
		reason = talloc_asprintf_append(reason,
		"Your _info.c has no module description.\n\n"
		"The lines after the first summary line in the _info.c file\n"
		"documentation should describe the purpose and use of the\n"
		"overall package\n");
	if (!id->example)
		reason = talloc_asprintf_append(reason,
		"Your _info.c has no module example.\n\n"
		"There should be an Example: section of the _info.c documentation\n"
		"which provides a concise toy program which uses your module\n");
	return reason;
}

static unsigned int has_info_documentation_score(struct manifest *m,
						 void *check_result)
{
	struct info_docs *id = check_result;
	return id->summary + id->description + id->example;
}

struct ccanlint has_info_documentation = {
	.name = "Documentation in _info.c",
	.total_score = 3,
	.score = has_info_documentation_score,
	.check = check_has_info_documentation,
	.describe = describe_has_info_documentation,
};
