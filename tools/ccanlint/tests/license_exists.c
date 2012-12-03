#include <tools/ccanlint/ccanlint.h>
#include <ccan/tal/tal.h>
#include <ccan/tal/str/str.h>
#include <ccan/tal/path/path.h>
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
#include <ccan/take/take.h>

/* We might need more ../ for nested modules. */
static const char *link_prefix(struct manifest *m)
{
	char *prefix = tal_strdup(m, "../../");
	unsigned int i;

	for (i = 0; i < strcount(m->modname, "/"); i++)
		prefix = tal_strcat(m, take(prefix), "../");

	return tal_strcat(m, take(prefix), "licenses/");
}

static const char *expected_link(const tal_t *ctx,
				 const char *prefix, enum license license)
{
	const char *shortname;

	switch (license) {
	case LICENSE_LGPLv2_PLUS:
	case LICENSE_LGPLv2:
		shortname = "LGPL-2.1";
		break;
	case LICENSE_LGPLv3:
	case LICENSE_LGPL:
		shortname = "LGPL-3";
		break;

	case LICENSE_GPLv2_PLUS:
	case LICENSE_GPLv2:
		shortname = "GPL-2";
		break;

	case LICENSE_GPLv3:
	case LICENSE_GPL:
		shortname = "GPL-3";
		break;

	case LICENSE_BSD:
		shortname = "BSD-3CLAUSE";
		break;

	case LICENSE_MIT:
		shortname = "BSD-MIT";
		break;

	case LICENSE_CC0:
		shortname = "CC0";
		break;

	default:
		return NULL;
	}

	return tal_strcat(ctx, prefix, shortname);
}

static void handle_license_link(struct manifest *m, struct score *score)
{
	struct doc_section *d = find_license_tag(m);
	const char *prefix = link_prefix(m);
	const char *link = path_join(m, m->dir, "LICENSE");
	const char *ldest = expected_link(score, prefix, m->license);
	char *q;

	printf(
	"Most modules want a copy of their license, so usually we create a\n"
	"LICENSE symlink into %s to avoid too many copies.\n", prefix);

	/* FIXME: make ask printf-like */
	q = tal_fmt(m, "Set up link to %s (license is %s)?",
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
	char *license = path_join(m, m->dir, "LICENSE");
	const char *expected;
	struct doc_section *d;
	const char *prefix = link_prefix(m);

	d = find_license_tag(m);
	if (!d) {
		score->error = tal_strdup(score, "No License: tag in _info");
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

	expected = expected_link(m, prefix, m->license);

	len = readlink(license, buf, sizeof(buf));
	if (len < 0) {
		/* Could be a real file... OK if not a standard license. */
		if (errno == EINVAL) {
			if (!expected) {
				score->score = score->total;
				return;
			}
			score->error
				= tal_fmt(score,
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
			score->error = tal_strdup(score,
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

	if (!strstarts(buf, prefix)) {
		score->error = tal_fmt(score,
				       "Expected symlink into %s not %s",
				       prefix, buf);
		return;
	}

	if (!expected) {
		score->error = tal_fmt(score,
				       "License in _info is unknown '%s',"
				       " but LICENSE symlink is '%s'",
				       d->lines[0], buf);
		return;
	}

	if (!streq(buf, expected)) {
		score->error = tal_fmt(score,
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
