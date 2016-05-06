#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
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

static const char *can_build(struct manifest *m)
{
	char *msg;

	/* FIXME: In safe mode, we'd need complex guesstiparsing. */
	if (safe_mode)
		return NULL;

	msg = get_ported(m, m->dir, true, get_or_compile_info);
	if (!msg)
		return NULL;
	return tal_fmt(m, "'_info ported' says '%s'", msg);
}

static void check_info_ported(struct manifest *m,
			      unsigned int *timeleft, struct score *score)
{
	score->pass = true;
	score->score = 1;
}

struct ccanlint info_ported = {
	.key = "info_ported",
	.can_run = can_build,
	.name = "_info indicates support for this platform",
	.check = check_info_ported,
	.needs = "info_compiles"
};

REGISTER_TEST(info_ported);
