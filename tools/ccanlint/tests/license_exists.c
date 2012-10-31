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

static const char *expected_link(enum license license)
{
	switch (license) {
	case LICENSE_LGPLv2_PLUS:
	case LICENSE_LGPLv2:
		return "../../licenses/LGPL-2.1";
	case LICENSE_LGPLv3:
	case LICENSE_LGPL:
		return "../../licenses/LGPL-3";

	case LICENSE_GPLv2_PLUS:
	case LICENSE_GPLv2:
		return "../../licenses/GPL-2";

	case LICENSE_GPLv3:
	case LICENSE_GPL:
		return "../../licenses/GPL-3";

	case LICENSE_BSD:
		return "../../licenses/BSD-3CLAUSE";

	case LICENSE_MIT:
		return "../../licenses/BSD-MIT";

	case LICENSE_CC0:
		return "../../licenses/CC0";

	default:
		return NULL;
	}
}

static void handle_license_link(struct manifest *m, struct score *score)
{
	struct doc_section *d = find_license_tag(m);
	const char *link = talloc_asprintf(m, "%s/LICENSE", m->dir);
	const char *ldest = expected_link(m->license);
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

extern struct ccanlint license_exists;

static void check_has_license(struct manifest *m,
			      unsigned int *timeleft, struct score *score)
{
	char buf[PATH_MAX];
	ssize_t len;
	char *license = talloc_asprintf(m, "%s/LICENSE", m->dir);
	const char *expected;
	struct doc_section *d;

	d = find_license_tag(m);
	if (!d) {
		score->error = talloc_strdup(score, "No License: tag in _info");
		return;
	}

	m->license = which_license(d);
	if (m->license == LICENSE_UNKNOWN) {
		score_file_error(score, m->info_file, d->srcline,
				 "WARNING: unknown License: in _info: %s",
				 d->lines[0]);
		/* FIXME: For historical reasons, don't fail here. */
		score->pass = true;
		return;
	}

	/* If they have a license tag at all, we pass. */
	score->pass = true;

	expected = expected_link(m->license);

	len = readlink(license, buf, sizeof(buf));
	if (len < 0) {
		/* Could be a real file... OK if not a standard license. */
		if (errno == EINVAL) {
			if (!expected) {
				score->score = score->total;
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
			/* Public domain doesn't really need a file. */
			if (m->license == LICENSE_PUBLIC_DOMAIN) {
				score->score = score->total;
				return;
			}
			score->error = talloc_strdup(score,
						     "LICENSE does not exist");
			if (expected)
				license_exists.handle = handle_license_link;
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
