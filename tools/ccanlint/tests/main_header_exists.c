#include <tools/ccanlint/ccanlint.h>
#include <ccan/tal/tal.h>
#include <ccan/tal/str/str.h>
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
#include <ccan/noerr/noerr.h>

static void check_has_main_header(struct manifest *m,
				  unsigned int *timeleft, struct score *score)
{
	struct ccan_file *f;

	list_for_each(&m->h_files, f, list) {
		if (strstarts(f->name, m->basename)
		    && strlen(f->name) == strlen(m->basename) + 2) {
			score->pass = true;
			score->score = score->total;
			return;
		}
	}
	score->error = tal_fmt(score,
	"You have no %s/%s.h header file.\n\n"
	"CCAN modules have a name, the same as the directory name.  They're\n"
	"expected to have an interface in the header of the same name.\n",
				       m->modname, m->basename);
}

struct ccanlint main_header_exists = {
	.key = "main_header_exists",
	.name = "Module has main header file",
	.check = check_has_main_header,
	.needs = ""
};

REGISTER_TEST(main_header_exists);
