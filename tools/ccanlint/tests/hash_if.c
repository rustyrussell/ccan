#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/str/str.h>
#include <ccan/foreach/foreach.h>
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

static void check_hash_if(struct manifest *m,
			  unsigned int *timeleft, struct score *score)
{
	struct list_head *list;
	const char *explanation =
	"\n\t(#if works like #ifdef, but with gcc's -Wundef, we can detect\n"
	"\tmistyped or unknown configuration options)";

	/* We don't fail ccanlint for this. */ 
	score->pass = true;

	foreach_ptr(list, &m->c_files, &m->h_files,
		    &m->run_tests, &m->api_tests,
		    &m->compile_ok_tests, &m->compile_fail_tests,
		    &m->other_test_c_files) {
		struct ccan_file *f;

		list_for_each(list, f, list) {
			unsigned int i;
			char **lines = get_ccan_file_lines(f);

			for (i = 0; lines[i]; i++) {
				const char *line = lines[i];
				char *sym;

				if (!get_token(&line, "#"))
					continue;
				if (!(get_token(&line, "if")
				      && get_token(&line, "defined")
				      && get_token(&line, "("))
				    && !get_token(&line, "ifdef"))
					continue;

				sym = get_symbol_token(lines, &line);
				if (!sym || !strstarts(sym, "HAVE_"))
					continue;
				score_file_error(score, f, i+1,
						 "%s should be tested with #if"
						 "%s",
						 sym, explanation);
				explanation = "";
			}
		}
	}

	if (!score->error) {
		score->score = score->total;
	}
}

struct ccanlint hash_if = {
	.key = "hash_if",
	.name = "Features are checked with #if not #ifdef",
	.check = check_hash_if,
	.needs = "info_exists"
};

REGISTER_TEST(hash_if);
