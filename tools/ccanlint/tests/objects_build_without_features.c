#include <tools/ccanlint/ccanlint.h>
#include <ccan/talloc/talloc.h>
#include "../compulsory_tests/build.h"

static void check_objs_build_without_features(struct manifest *m,
					      bool keep,
					      unsigned int *timeleft,
					      struct score *score)
{
	const char *flags = talloc_asprintf(score, "-I. %s", cflags);
	build_objects(m, keep, score, flags);
}

struct ccanlint objects_build_without_features = {
	.key = "objects_build_without_features",
	.name = "Module object files can be built (without features)",
	.check = check_objs_build_without_features,
	.needs = "reduce_features objects_build"
};
REGISTER_TEST(objects_build_without_features);

