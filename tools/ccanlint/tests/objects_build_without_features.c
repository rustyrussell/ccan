#include <tools/ccanlint/ccanlint.h>
#include <ccan/tal/str/str.h>
#include "reduce_features.h"
#include "build.h"

static void check_objs_build_without_features(struct manifest *m,
					      unsigned int *timeleft,
					      struct score *score)
{
	const char *flags = tal_fmt(score, "%s %s",
				    REDUCE_FEATURES_FLAGS, cflags);
	build_objects(m, score, flags, COMPILE_NOFEAT);
}

struct ccanlint objects_build_without_features = {
	.key = "objects_build_without_features",
	.name = "Module object files can be built (without features)",
	.check = check_objs_build_without_features,
	.needs = "reduce_features objects_build"
};
REGISTER_TEST(objects_build_without_features);

