#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/tal/grab_file/grab_file.h>
#include <ccan/read_write_all/read_write_all.h>
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

static void check_info_compiles(struct manifest *m,
				unsigned int *timeleft, struct score *score)
{
	char *info_c_file, *info, *output;
	int fd;

	/* We don't really fail if we're in safe mode: our dependencies
	 * still run. */
	if (safe_mode) {
		score->pass = true;
		score->score = 0;
		return;
	}

	/* Copy it to a file with proper .c suffix. */
	info = grab_file(score, m->info_file->fullname);
	if (!info) {
		score_file_error(score, m->info_file, 0,
				 "could not be read: %s", strerror(errno));
		return;
	}

	info_c_file = temp_file(info, ".c", "_info");
	fd = open(info_c_file, O_WRONLY|O_CREAT|O_EXCL, 0600);
	if (fd < 0 || !write_all(fd, info, tal_count(info)-1))
		err(1, "Copying _info file");

	if (close(fd) != 0)
		err(1, "Closing _info file");

	m->info_file->compiled[COMPILE_NORMAL] = temp_file(m, "", "info");
	if (!compile_and_link(score, info_c_file, find_ccan_dir(m->dir), "",
			      compiler, cflags, "",
			      m->info_file->compiled[COMPILE_NORMAL],
			      &output)) {
		score_file_error(score, m->info_file, 0,
				 "Errors compiling _info:\n%s", output);
		return;
	}

	score->pass = true;
	score->score = 1;
}

struct ccanlint info_compiles = {
	.key = "info_compiles",
	.name = "_info compiles",
	.check = check_info_compiles,
	.needs = "info_exists",
	.compulsory = true
};

REGISTER_TEST(info_compiles);
