#include "ccanlint.h"
#include "get_file_lines.h"
#include <talloc/talloc.h>
#include <string/string.h>
#include <noerr/noerr.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <dirent.h>

char **get_ccan_file_lines(struct ccan_file *f)
{
	if (!f->lines) {
		char *buffer = grab_file(f, f->name, NULL);
		if (!buffer)
			err(1, "Getting file %s", f->name);
		f->lines = strsplit(f, buffer, "\n", &f->num_lines);
	}
	return f->lines;
}

static void add_files(struct manifest *m, const char *dir)
{
	DIR *d;
	struct dirent *ent;

	if (dir[0])
		d = opendir(dir);
	else
		d = opendir(".");
	if (!d)
		err(1, "Opening directory %s", dir[0] ? dir : ".");

	while ((ent = readdir(d)) != NULL) {
		struct stat st;
		struct ccan_file *f;
		struct list_head *dest;
		bool is_c_src;

		if (ent->d_name[0] == '.')
			continue;

		f = talloc(m, struct ccan_file);
		f->lines = NULL;
		f->name = talloc_asprintf(f, "%s%s", dir, ent->d_name);
		if (lstat(f->name, &st) != 0)
			err(1, "lstat %s", f->name);

		if (S_ISDIR(st.st_mode)) {
			f->name = talloc_append_string(f->name, "/");
			add_files(m, f->name);
			continue;
		}
		if (!S_ISREG(st.st_mode)) {
			talloc_free(f);
			continue;
		}

		if (streq(f->name, "_info.c")) {
			m->info_file = f;
			continue;
		}

		is_c_src = strends(f->name, ".c");
		if (!is_c_src && !strends(f->name, ".h"))
			dest = &m->other_files;
		else if (!strchr(f->name, '/')) {
			if (is_c_src)
				dest = &m->c_files;
			else
				dest = &m->h_files;
		} else if (strstarts(f->name, "test/")) {
			if (is_c_src) {
				if (strstarts(f->name, "test/run"))
					dest = &m->run_tests;
				else if (strstarts(f->name, "test/compile_ok"))
					dest = &m->compile_ok_tests;
				else if (strstarts(f->name, "test/compile_fail"))
					dest = &m->compile_fail_tests;
				else
					dest = &m->other_test_files;
			} else
				dest = &m->other_test_files;
		} else
			dest = &m->other_files;

		list_add(dest, &f->list);
	}
	closedir(d);
}

char *report_on_lines(struct list_head *files,
		      char *(*report)(const char *),
		      char *sofar)
{
	struct ccan_file *f;

	list_for_each(files, f, list) {
		unsigned int i;
		char **lines = get_ccan_file_lines(f);

		for (i = 0; i < f->num_lines; i++) {
			char *r = report(lines[i]);
			if (!r)
				continue;

			sofar = talloc_asprintf_append(sofar,
						       "%s:%u:%s\n",
						       f->name, i+1, r);
			talloc_free(r);
		}
	}
	return sofar;
}

struct manifest *get_manifest(void)
{
	struct manifest *m = talloc(NULL, struct manifest);
	unsigned int len;

	m->info_file = NULL;
	list_head_init(&m->c_files);
	list_head_init(&m->h_files);
	list_head_init(&m->run_tests);
	list_head_init(&m->compile_ok_tests);
	list_head_init(&m->compile_fail_tests);
	list_head_init(&m->other_test_files);
	list_head_init(&m->other_files);

	/* *This* is why people hate C. */
	len = 32;
	m->basename = talloc_array(m, char, len);
	while (!getcwd(m->basename, len)) {
		if (errno != ERANGE)
			err(1, "Getting current directory");
		m->basename = talloc_realloc(m, m->basename, char, len *= 2);
	}

	len = strlen(m->basename);
	while (len && m->basename[len-1] == '/')
		m->basename[--len] = '\0';

	m->basename = strrchr(m->basename, '/');
	if (!m->basename)
		errx(1, "I don't expect to be run from the root directory");
	m->basename++;

	add_files(m, "");
	return m;
}
