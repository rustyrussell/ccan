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
#include <ccan/talloc/talloc.h>
#include <ccan/str/str.h>

struct ccanlint has_license;

static struct doc_section *find_license(const struct manifest *m)
{
	struct doc_section *d;

	list_for_each(m->info_file->doc_sections, d, list) {
		if (!streq(d->function, m->basename))
			continue;
		if (streq(d->type, "license"))
			return d;
	}
	return NULL;
}

static const char *expected_link(const struct manifest *m,
				 struct doc_section *d)
{
	if (streq(d->lines[0], "GPL")
	    || streq(d->lines[0], "GPLv3")
	    || streq(d->lines[0], "GPLv3 or later")
	    || streq(d->lines[0], "GPLv3 (or later)")
	    || streq(d->lines[0], "GPL (3 or any later version)"))
		return "../../licenses/GPL-3";
	if (streq(d->lines[0], "GPLv2")
	    || streq(d->lines[0], "GPLv2 or later")
	    || streq(d->lines[0], "GPLv2 (or later)")
	    || streq(d->lines[0], "GPL (2 or any later version)"))
		return "../../licenses/GPL-3";
	if (streq(d->lines[0], "LGPL")
	    || streq(d->lines[0], "LGPLv3")
	    || streq(d->lines[0], "LGPLv3 or later")
	    || streq(d->lines[0], "LGPLv3 (or later)")
	    || streq(d->lines[0], "LGPL (3 or any later version)"))
		return "../../licenses/LGPL-3";
	if (streq(d->lines[0], "LGPLv2")
	    || streq(d->lines[0], "LGPLv2 or later")
	    || streq(d->lines[0], "LGPLv2 (or later)")
	    || streq(d->lines[0], "LGPL (2 or any later version)"))
		return "../../licenses/LGPL-2.1";
	if (streq(d->lines[0], "BSD")
	    || streq(d->lines[0], "BSD-MIT")
	    || streq(d->lines[0], "MIT"))
		return "../../licenses/BSD-MIT";
	return NULL;
}

static void handle_license_link(struct manifest *m, struct score *score)
{
	const char *link = talloc_asprintf(m, "%s/LICENSE", m->dir);
	struct doc_section *d = find_license(m);
	const char *ldest = expected_link(m, d);
	char *q;

	printf(
	"Most modules want a copy of their license, so usually we create a\n"
	"LICENSE symlink into ../../licenses to avoid too many copies.\n");

	/* FIXME: make ask printf-like */
	q = talloc_asprintf(m, "Set up link to %s (license is %s)?",
			    ldest, d->lines[0]);
	if (ask(q)) {
		if (symlink(ldest, link) != 0)
			err(1, "Creating symlink %s -> %s", link, ldest);
	}
}

static void check_has_license(struct manifest *m,
			      bool keep,
			      unsigned int *timeleft, struct score *score)
{
	char buf[PATH_MAX];
	ssize_t len;
	char *license = talloc_asprintf(m, "%s/LICENSE", m->dir);
	const char *expected;
	struct doc_section *d;

	d = find_license(m);
	if (!d) {
		score->error = "No License: tag in _info";
		return;
	}
	expected = expected_link(m, d);

	len = readlink(license, buf, sizeof(buf));
	if (len < 0) {
		/* Could be a real file... OK if not a standard license. */
		if (errno == EINVAL) {
			if (!expected) {
				score->pass = true;
				return;
			}
			score->error
				= talloc_asprintf(score,
					  "License in _info is '%s',"
					  " expect LICENSE symlink '%s'",
					  d->lines[0], expected);
			return;
		}
		if (errno == ENOENT) {
			score->error = "LICENSE does not exist";
			if (expected)
				has_license.handle = handle_license_link;
			return;
		}
		err(1, "readlink on %s", license);
	}
	if (len >= sizeof(buf))
		errx(1, "Reading symlink %s gave huge result", license);

	buf[len] = '\0';

	if (!strstarts(buf, "../../licenses/")) {
		score->error = talloc_asprintf(score,
					       "Expected symlink to"
					       " ../../licenses/..."
					       " not %s", buf);
		return;
	}

	if (!expected) {
		score->error = talloc_asprintf(score,
					  "License in _info is unknown '%s',"
					  " but LICENSE symlink is '%s'",
					  d->lines[0], buf);
		return;
	}

	if (!streq(buf, expected)) {
		score->error = talloc_asprintf(score,
				       "Expected symlink to %s not %s",
				       expected, buf);
		return;
	}
	score->pass = true;
	score->score = score->total;
}

struct ccanlint license_exists = {
	.key = "license_exists",
	.name = "Module has License: entry in _info, and LICENSE symlink/file",
	.check = check_has_license,
	.needs = "info_exists"
};

REGISTER_TEST(license_exists);
