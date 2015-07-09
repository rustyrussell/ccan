#include "config.h"
#include "manifest.h"
#include "tools.h"
#include <ccan/str/str.h>
#include <ccan/tal/link/link.h>
#include <ccan/tal/grab_file/grab_file.h>
#include <ccan/tal/path/path.h>
#include <ccan/hash/hash.h>
#include <ccan/htable/htable_type.h>
#include <ccan/noerr/noerr.h>
#include <ccan/foreach/foreach.h>
#include <ccan/asort/asort.h>
#include <ccan/array_size/array_size.h>
#include <ccan/err/err.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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
		f->contents = grab_file(f, f->fullname);
		if (!f->contents)
			err(1, "Reading file %s", f->fullname);
		f->contents_size = tal_count(f->contents) - 1;
	}
	return f->contents;
}

char **get_ccan_file_lines(struct ccan_file *f)
{
	if (!f->lines)
		f->lines = tal_strsplit(f, get_ccan_file_contents(f), "\n",
					STR_EMPTY_OK);

	return f->lines;
}

struct ccan_file *new_ccan_file(const void *ctx, const char *dir,
				const char *name)
{
	struct ccan_file *f;
	unsigned int i;

	assert(dir[0] == '/');

	f = tal(ctx, struct ccan_file);
	f->lines = NULL;
	f->line_info = NULL;
	f->doc_sections = NULL;
	for (i = 0; i < ARRAY_SIZE(f->compiled); i++)
		f->compiled[i] = NULL;
	f->name = tal_strdup(f, name);
	f->fullname = path_join(f, dir, f->name);
	f->contents = NULL;
	f->simplified = NULL;
	f->idempotent_cond = NULL;

	return f;
}

static void add_files(struct manifest *m, const char *base, const char *subdir)
{
	DIR *d;
	struct dirent *ent;
	char **subs = tal_arr(m, char *, 0);
	const char *thisdir;

	if (!subdir)
		thisdir = base;
	else
		thisdir = path_join(subs, base, subdir);

	d = opendir(thisdir);
	if (!d)
		err(1, "Opening directory %s", thisdir);

	while ((ent = readdir(d)) != NULL) {
		struct stat st;
		struct ccan_file *f;
		struct list_head *dest;
		bool is_c_src;

		if (ent->d_name[0] == '.')
			continue;

		f = new_ccan_file(m, m->dir,
				  subdir ? path_join(m, subdir, ent->d_name)
				  : ent->d_name);
		if (stat(f->fullname, &st) != 0)
			err(1, "stat %s", f->fullname);

		if (S_ISDIR(st.st_mode)) {
			size_t len = tal_count(subs);
			tal_resize(&subs, len+1);
			subs[len] = tal_strcat(subs, f->name, "/");
			continue;
		}
		if (!S_ISREG(st.st_mode)) {
			tal_free(f);
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
	if (!subdir) {
		size_t i;

		if (!m->info_file
		    && list_empty(&m->c_files)
		    && list_empty(&m->h_files))
			errx(1, "No _info, C or H files found here!");

		/* Don't enter subdirs with _info: they're separate modules. */
		for (i = 0; i < tal_count(subs); i++) {
			struct stat st;
			char *subinfo = path_join(subs, base,
						  path_join(subs, subs[i],
							    "_info"));
			if (lstat(subinfo, &st) != 0)
				add_files(m, base, subs[i]);
		}
	}
	tal_free(subs);
}

static int cmp_names(struct ccan_file *const *a, struct ccan_file *const *b,
		     void *unused)
{
	return strcmp((*a)->name, (*b)->name);
}

static void sort_files(struct list_head *list)
{
	struct ccan_file **files = tal_arr(NULL, struct ccan_file *, 0), *f;
	unsigned int i;

	while ((f = list_top(list, struct ccan_file, list)) != NULL) {
		tal_expand(&files, &f, 1);
		list_del(&f->list);
	}
	asort(files, tal_count(files), cmp_names, NULL);

	for (i = 0; i < tal_count(files); i++)
		list_add_tail(list, &files[i]->list);
	tal_free(files);
}

struct manifest *get_manifest(const void *ctx, const char *dir)
{
	struct manifest *m;
	char *canon_dir;
	unsigned int len;
	struct list_head *list;

	if (!manifests) {
		manifests = tal(NULL, struct htable_manifest);
		htable_manifest_init(manifests);
	}

	canon_dir = path_canon(ctx, dir);
	if (!canon_dir)
		err(1, "Getting canonical version of directory %s", dir);

	m = htable_manifest_get(manifests, canon_dir);
	if (m)
		return m;

	m = tal_linkable(tal(NULL, struct manifest));
	m->info_file = NULL;
	m->compiled[COMPILE_NORMAL] = m->compiled[COMPILE_NOFEAT] = NULL;
	m->dir = tal_steal(m, canon_dir);
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
	list_head_init(&m->test_deps);

	/* Trim trailing /. */
	len = strlen(m->dir);
	while (len && m->dir[len-1] == '/')
		m->dir[--len] = '\0';

	m->basename = strrchr(m->dir, '/');
	if (!m->basename)
		errx(1, "I don't expect to be run from the root directory");
	m->basename++;

	assert(strstarts(m->dir, find_ccan_dir(m->dir)));
	m->modname = m->dir + strlen(find_ccan_dir(m->dir)) + strlen("ccan/");

	add_files(m, canon_dir, NULL);

	/* Nicer to run tests in a predictable order. */
	foreach_ptr(list, &m->api_tests, &m->run_tests, &m->compile_ok_tests,
		    &m->compile_fail_tests)
		sort_files(list);

	htable_manifest_add(manifests, tal_link(manifests, m));

	return m;
}
