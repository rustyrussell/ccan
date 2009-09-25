#ifndef CCAN_LINT_H
#define CCAN_LINT_H
#include <ccan/list/list.h>
#include <stdbool.h>
#include "../doc_extract.h"

#define REGISTER_TEST(name, ...) extern struct ccanlint name
#include "generated-init-tests"
#undef REGISTER_TEST

#define REGISTER_TEST(name, ...) 

struct manifest {
	/* The module name, ie. final element of dir name */
	char *basename;
	struct ccan_file *info_file;

	struct list_head c_files;
	struct list_head h_files;

	struct list_head run_tests;
	struct list_head api_tests;
	struct list_head compile_ok_tests;
	struct list_head compile_fail_tests;
	struct list_head other_test_c_files;
	struct list_head other_test_files;

	struct list_head other_files;

	/* From tests/check_depends_exist.c */
	struct list_head dep_dirs;
	/* From tests/check_depends_built.c */
	struct list_head dep_objs;
};

struct manifest *get_manifest(const void *ctx);

struct ccanlint {
	struct list_node list;

	/* Unique name of test */
	const char *name;

	/* Total score that this test is worth.  0 means compulsory tests. */
	unsigned int total_score;

	/* Can we run this test?  Return string explaining why, if not. */
	const char *(*can_run)(struct manifest *m);

	/* If this returns non-NULL, it means the check failed. */
	void *(*check)(struct manifest *m);

	/* The non-NULL return from check is passed to one of these: */

	/* So, what did this get out of the total_score?  (NULL means 0). */
	unsigned int (*score)(struct manifest *m, void *check_result);

	/* Verbose description of what was wrong. */
	const char *(*describe)(struct manifest *m, void *check_result);

	/* Can we do something about it? (NULL if not) */
	void (*handle)(struct manifest *m, void *check_result);

	/* Internal use fields: */
	/* Who depends on us? */
	struct list_head dependencies;
	/* How many things do we (still) depend on? */
	unsigned int num_depends;
	/* Did we skip a dependency?  If so, must skip this, too. */
	bool skip;
	/* Did we fail a dependency?  If so, skip and mark as fail. */
	bool skip_fail;
};

/* Ask the user a yes/no question: the answer is NO if there's an error. */
bool ask(const char *question);

enum line_info_type {
	PREPROC_LINE, /* Line starts with # */
	CODE_LINE, /* Code (ie. not pure comment). */
	DOC_LINE, /* Line with kernel-doc-style comment. */
	COMMENT_LINE, /* (pure) comment line */
};

/* So far, only do simple #ifdef/#ifndef/#if defined/#if !defined tests,
 * and #if <SYMBOL>/#if !<SYMBOL> */
struct pp_conditions {
	/* We're inside another ifdef? */
	struct pp_conditions *parent;

	enum {
		PP_COND_IF,
		PP_COND_IFDEF,
		PP_COND_UNKNOWN,
	} type;

	bool inverse;
	const char *symbol;
};

/* Preprocessor information about each line. */
struct line_info {
	enum line_info_type type;

	/* Is this actually a continuation of line above? (which ends in \) */
	bool continued;

	/* Conditions for this line to be compiled. */
	struct pp_conditions *cond;
};

struct ccan_file {
	struct list_node list;

	char *name;

	/* Pristine version of the original file.
	 * Use get_ccan_file_lines to fill this. */
	const char *contents;
	size_t contents_size;

	/* Use get_ccan_file_lines / get_ccan_line_info to fill these. */
	unsigned int num_lines;
	char **lines;
	struct line_info *line_info;

	struct list_head *doc_sections;

	/* If this file gets compiled (eg. .C file to .o file), result here. */
	const char *compiled;
};

/* A new ccan_file, with the given name (talloc_steal onto returned value). */
struct ccan_file *new_ccan_file(const void *ctx, char *name);

/* Use this rather than accessing f->lines directly: loads on demand. */
char **get_ccan_file_lines(struct ccan_file *f);

/* Use this rather than accessing f->lines directly: loads on demand. */
struct line_info *get_ccan_line_info(struct ccan_file *f);

enum line_compiled {
	NOT_COMPILED,
	COMPILED,
	MAYBE_COMPILED,
};

/* Simple evaluator.  If symbols are set this way, is this condition true?
 * NULL values mean undefined, NULL symbol terminates. */
enum line_compiled get_ccan_line_pp(struct pp_conditions *cond,
				    const char *symbol,
				    const unsigned int *value, ...);

/* Get token if it's equal to token. */
bool get_token(const char **line, const char *token);
/* Talloc copy of symbol token, or NULL.  Increment line. */
char *get_symbol_token(void *ctx, const char **line);

/* Similarly for ->doc_sections */
struct list_head *get_ccan_file_docs(struct ccan_file *f);


/* Call the reporting on every line in the file.  sofar contains
 * previous results. */
char *report_on_lines(struct list_head *files,
		      char *(*report)(const char *),
		      char *sofar);

/* Normal tests. */
extern struct ccanlint trailing_whitespace;

/* Dependencies */
struct dependent {
	struct list_node node;
	struct ccanlint *dependent;
};

/* Are we happy to compile stuff, or just non-intrusive tests? */
extern bool safe_mode;

#endif /* CCAN_LINT_H */
