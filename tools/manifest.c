#include "config.h"
#include "manifest.h"
#include "tools.h"
#include <ccan/talloc/talloc.h>
#include <ccan/str/str.h>
#include <ccan/str_talloc/str_talloc.h>
#include <ccan/talloc_link/talloc_link.h>
#include <ccan/hash/hash.h>
#include <ccan/htable/htable_type.h>
#include <ccan/grab_file/grab_file.h>
#include <ccan/noerr/noerr.h>
#include <ccan/foreach/foreach.h>
#include <ccan/asort/asort.h>
#include <ccan/array_size/array_size.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>

static size_t dir_hash(const char *name)
{
	return hash(name, strlen(name), 0);
}

static const char *manifest_name(const struct manifest *m)
{
	return m->dir;
}

static bool dir_cmp(const struct manifest *m, const char *dir)
{
	return strcmp(m->dir, dir) == 0;
}

HTABLE_DEFINE_TYPE(struct manifest, manifest_name, dir_hash, dir_cmp,
		   htable_manifest);
static struct htable_manifest *manifests;

const char *get_ccan_file_contents(struct ccan_file *f)
{
	if (!f->contents) {
		f->contents = grab_file(f, f->fullname, &f->contents_size);
		if (!f->contents)
			err(1, "Reading file %s", f->fullname);
	}
	return f->contents;
}

char **get_ccan_file_lines(struct ccan_file *f)
{
	if (!f->lines)
		f->lines = strsplit(f, get_ccan_file_contents(f), "\n");

	/* FIXME: is f->num_lines necessary? */
	f->num_lines = talloc_array_length(f->lines) - 1;
	return f->lines;
}

struct ccan_file *new_ccan_file(const void *ctx, const char *dir, char *name)
{
	struct ccan_file *f;
	unsigned int i;

	assert(dir[0] == '/');

	f = talloc(ctx, struct ccan_file);
	f->lines = NULL;
	f->line_info = NULL;
	f->doc_sections = NULL;
	for (i = 0; i < ARRAY_SIZE(f->compiled); i++)
		f->compiled[i] = NULL;
	f->name = talloc_steal(f, name);
	f->fullname = talloc_asprintf(f, "%s/%s", dir, f->name);
	f->contents = NULL;
	f->simplified = NULL;
	return f;
}

static void add_files(struct manifest *m, const char *dir)
{
	DIR *d;
	struct dirent *ent;
	char **subs = NULL;

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

		f = new_ccan_file(m, m->dir,
				  talloc_asprintf(m, "%s%s",
						  dir, ent->d_name));
		if (lstat(f->name, &st) != 0)
			err(1, "lstat %s", f->name);

		if (S_ISDIR(st.st_mode)) {
			size_t len = talloc_array_length(subs);
			subs = talloc_realloc(m, subs, char *, len+1);
			subs[len] = talloc_append_string(f->name, "/");
			continue;
		}
		if (!S_ISREG(st.st_mode)) {
			talloc_free(f);
			continue;
		}

		if (streq(f->name, "_info")) {
			m->info_file = f;
			continue;
		}

		is_c_src = strends(f->name, ".c");
		if (!is_c_src && !strends(f->name, ".h")) {
			dest = &m->other_files;
		} else if (!strchr(f->name, '/')) {
			if (is_c_src)
				dest = &m->c_files;
			else
				dest = &m->h_files;
		} else if (strstarts(f->name, "test/")) {
			if (is_c_src) {
				if (strstarts(f->name, "test/api"))
					dest = &m->api_tests;
				else if (strstarts(f->name, "test/run"))
					dest = &m->run_tests;
				else if (strstarts(f->name, "test/compile_ok"))
					dest = &m->compile_ok_tests;
				else if (strstarts(f->name, "test/compile_fail"))
					dest = &m->compile_fail_tests;
				else
					dest = &m->other_test_c_files;
			} else
				dest = &m->other_test_files;
		} else
			dest = &m->other_files;

		list_add(dest, &f->list);
	}
	closedir(d);

	/* Before we recurse, sanity check this is a ccan module. */
	if (!dir[0]) {
		size_t i;

		if (!m->info_file
		    && list_empty(&m->c_files)
		    && list_empty(&m->h_files))
			errx(1, "No _info, C or H files found here!");

		for (i = 0; i < talloc_array_length(subs); i++)
			add_files(m, subs[i]);
	}
	talloc_free(subs);
}

static int cmp_names(struct ccan_file *const *a, struct ccan_file *const *b,
		     void *unused)
{
	return strcmp((*a)->name, (*b)->name);
}

static void sort_files(struct list_head *list)
{
	struct ccan_file **files = NULL, *f;
	unsigned int i, num;

	num = 0;
	while ((f = list_top(list, struct ccan_file, list)) != NULL) {
		files = talloc_realloc(NULL, files, struct ccan_file *, num+1);
		files[num++] = f;
		list_del(&f->list);
	}
	asort(files, num, cmp_names, NULL);

	for (i = 0; i < num; i++)
		list_add_tail(list, &files[i]->list);
	talloc_free(files);
}

struct manifest *get_manifest(const void *ctx, const char *dir)
{
	struct manifest *m;
	char *olddir, *canon_dir;
	unsigned int len;
	struct list_head *list;

	if (!manifests) {
		manifests = talloc(NULL, struct htable_manifest);
		htable_manifest_init(manifests);
	}

	olddir = talloc_getcwd(NULL);
	if (!olddir)
		err(1, "Getting current directory");

	if (chdir(dir) != 0)
		err(1, "Failed to chdir to %s", dir);

	canon_dir = talloc_getcwd(olddir);
	if (!canon_dir)
		err(1, "Getting current directory");

	m = htable_manifest_get(manifests, canon_dir);
	if (m)
		goto done;

	m = talloc_linked(ctx, talloc(NULL, struct manifest));
	m->info_file = NULL;
	m->compiled[COMPILE_NORMAL] = m->compiled[COMPILE_NOFEAT] = NULL;
	m->dir = talloc_steal(m, canon_dir);
	list_head_init(&m->c_files);
	list_head_init(&m->h_files);
	list_head_init(&m->api_tests);
	list_head_init(&m->run_tests);
	list_head_init(&m->compile_ok_tests);
	list_head_init(&m->compile_fail_tests);
	list_head_init(&m->other_test_c_files);
	list_head_init(&m->other_test_files);
	list_head_init(&m->other_files);
	list_head_init(&m->examples);
	list_head_init(&m->mangled_examples);
	list_head_init(&m->deps);

	len = strlen(m->dir);
	while (len && m->dir[len-1] == '/')
		m->dir[--len] = '\0';

	m->basename = strrchr(m->dir, '/');
	if (!m->basename)
		errx(1, "I don't expect to be run from the root directory");
	m->basename++;

	add_files(m, "");

	/* Nicer to run tests in a predictable order. */
	foreach_ptr(list, &m->api_tests, &m->run_tests, &m->compile_ok_tests,
		    &m->compile_fail_tests)
		sort_files(list);

	htable_manifest_add(manifests, m);

done:
	if (chdir(olddir) != 0)
		err(1, "Returning to original directory '%s'", olddir);
	talloc_free(olddir);

	return m;
}
