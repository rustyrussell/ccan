#ifndef CCAN_TOOLS_MANIFEST_H
#define CCAN_TOOLS_MANIFEST_H
#include "config.h"
#include "ccanlint/licenses.h"
#include <ccan/list/list.h>

enum compile_type {
	COMPILE_NORMAL,
	COMPILE_NOFEAT,
	COMPILE_COVERAGE,
	COMPILE_TYPES
};

struct manifest {
	char *dir;
	/* The name of the module, ie. elements of dir name after ccan/. */
	char *modname;
	/* The final element of dir name */
	char *basename;
	struct ccan_file *info_file;

	/* Linked off deps. */
	struct list_node list;
	/* Where our final compiled output is */
	char *compiled[COMPILE_TYPES];

	struct list_head c_files;
	struct list_head h_files;

	struct list_head run_tests;
	struct list_head api_tests;
	struct list_head compile_ok_tests;
	struct list_head compile_fail_tests;
	struct list_head other_test_c_files;
	struct list_head other_test_files;

	struct list_head other_files;
	struct list_head examples;
	struct list_head mangled_examples;

	/* From tests/check_depends_exist.c */
	struct list_head deps;
	struct list_head test_deps;

	/* From tests/license_exists.c */
	enum license license;
};

/* Get the manifest for a given directory. */
struct manifest *get_manifest(const void *ctx, const char *dir);

struct ccan_file {
	struct list_node list;

	/* Name (usually, within m->dir). */
	char *name;

	/* Full path name. */
	char *fullname;

	/* Pristine version of the original file.
	 * Use get_ccan_file_contents to fill this. */
	const char *contents;
	size_t contents_size;

	/* Use get_ccan_file_lines / get_ccan_line_info to fill these. */
	char **lines;
	struct line_info *line_info;

	struct list_head *doc_sections;

	/* If this file gets compiled (eg. .C file to .o file), result here. */
	char *compiled[COMPILE_TYPES];

	/* Filename containing output from valgrind. */
	char *valgrind_log;

	/* Leak output from valgrind. */
	char *leak_info;

	/* Simplified stream (lowercase letters and single spaces) */
	char *simplified;

	/* Condition for idempotent wrapper (filled by headers_idempotent) */
	struct pp_conditions *idempotent_cond;
};

/* A new ccan_file, with the given dir and name (either can be take()). */
struct ccan_file *new_ccan_file(const void *ctx,
				const char *dir, const char *name);

/* Use this rather than accessing f->contents directly: loads on demand. */
const char *get_ccan_file_contents(struct ccan_file *f);

/* Use this rather than accessing f->lines directly: loads on demand. */
char **get_ccan_file_lines(struct ccan_file *f);

#endif /* CCAN_TOOLS_MANIFEST_H */
