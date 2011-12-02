#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/talloc/talloc.h>
#include <ccan/str/str.h>
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
#include <ctype.h>
#include "reduce_features.h"
#include "build.h"

static const char *can_build(struct manifest *m)
{
	if (safe_mode)
		return "Safe mode enabled";
	return NULL;
}

static void check_depends_built_without_features(struct manifest *m,
						 unsigned int *timeleft,
						 struct score *score)
{
	struct manifest *i;
	char *flags;

	flags = talloc_asprintf(score, "%s %s", cflags,
				REDUCE_FEATURES_FLAGS);

	list_for_each(&m->deps, i, list) {
		char *errstr = build_submodule(i, flags, COMPILE_NOFEAT);

		if (errstr) {
			score->error = talloc_asprintf(score,
						       "Dependency %s"
						       " did not build:\n%s",
						       i->basename, errstr);
			return;
		}
	}

	score->pass = true;
	score->score = score->total;
}

struct ccanlint depends_build_without_features = {
	.key = "depends_build_without_features",
	.name = "Module's CCAN dependencies can be found or built (reduced features)",
	.check = check_depends_built_without_features,
	.can_run = can_build,
	.needs = "depends_build reduce_features"
};

REGISTER_TEST(depends_build_without_features);
