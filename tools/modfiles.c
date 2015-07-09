/* List files in a module, or modules. */
#include <ccan/opt/opt.h>
#include <ccan/foreach/foreach.h>
#include <ccan/tal/str/str.h>
#include "manifest.h"
#include <stdio.h>

static void add_file(const struct ccan_file *f,
		     bool fullpath, bool nul_term, bool gitonly)
{
	/* Hacky way of seeing if git knows about this. */
	if (gitonly) {
		char *cmd = tal_fmt(f, "git status --porcelain --ignored %s | grep -q '^ *[!?]'", f->fullname);
		if (system(cmd) == 0)
			return;
	}
	printf("%s%c", fullpath ? f->fullname : f->name,
	       nul_term ? '\0' : '\n');
}

int main(int argc, char *argv[])
{
	bool code = true;
	bool license = true;
	bool tests = true;
	bool other = true;
	bool info = true;
	bool gitonly = false;
	bool nul_term = false;
	bool fullpath = false;
	int i;

	opt_register_noarg("--no-code", opt_set_invbool, &code,
			   "Don't list .c and .h files");
	opt_register_noarg("--no-license", opt_set_invbool, &license,
			   "Don't list license file");
	opt_register_noarg("--no-info", opt_set_invbool, &info,
			   "Don't list _info file");
	opt_register_noarg("--no-tests", opt_set_invbool, &tests,
			   "Don't list test files");
	opt_register_noarg("--no-other", opt_set_invbool, &other,
			   "Don't list other files");
	opt_register_noarg("--git-only", opt_set_bool, &gitonly,
			   "Only include files in git");
	opt_register_noarg("--fullpath", opt_set_bool, &fullpath,
			   "Print full path names instead of module-relative ones");
	opt_register_noarg("--null|-0", opt_set_bool, &nul_term,
			   "Separate files with nul character instead of \\n");
	opt_register_noarg("-h|--help", opt_usage_and_exit,
			   "<moduledir>...",
			   "Show usage");

	opt_parse(&argc, argv, opt_log_stderr_exit);
	if (argc < 2)
		opt_usage_exit_fail("Expected one or more module directories");

	for (i = 1; i < argc; i++) {
		struct manifest *m = get_manifest(NULL, argv[i]);
		struct list_head *list;
		struct ccan_file *f;

		if (info)
			add_file(m->info_file, fullpath, nul_term, gitonly);

		if (code) {
			foreach_ptr(list, &m->c_files, &m->h_files) {
				list_for_each(list, f, list)
					add_file(f, fullpath, nul_term, gitonly);
			}
		}
		if (license) {
			list_for_each(&m->other_files, f, list) {
				if (streq(f->name, "LICENSE"))
					add_file(f, fullpath, nul_term, gitonly);
			}
		}
		if (tests) {
			foreach_ptr(list, &m->run_tests, &m->api_tests,
				    &m->compile_ok_tests, &m->compile_fail_tests,
				    &m->other_test_c_files,
				    &m->other_test_files) {
				list_for_each(list, f, list)
					add_file(f, fullpath, nul_term, gitonly);
			}
		}
		if (other) {
			list_for_each(&m->other_files, f, list) {
				if (!streq(f->name, "LICENSE"))
					add_file(f, fullpath, nul_term, gitonly);
			}
		}
	}
	return 0;
}
