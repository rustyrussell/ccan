#ifndef CCAN_LINT_H
#define CCAN_LINT_H
#include <ccan/list/list.h>
#include <stdbool.h>
#include "../doc_extract.h"

#define REGISTER_TEST(name, ...) extern struct ccanlint name
#include "generated-compulsory-tests"
#include "generated-normal-tests"
#undef REGISTER_TEST

#define REGISTER_TEST(name, ...) 

/* 0 == Describe failed tests.
   1 == Describe results for partial failures.
   2 == One line per test, plus details of failures.

   Mainly for debugging ccanlint:
   3 == Describe every object built.
   4 == Describe every action. */
extern int verbose;

struct manifest {
	char *dir;
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
	struct list_head examples;
	struct list_head mangled_examples;

	/* From tests/check_depends_exist.c */
	struct list_head dep_dirs;
};

struct manifest *get_manifest(const void *ctx, const char *dir);

struct file_error {
	struct list_node list;
	struct ccan_file *file;
	unsigned int line; /* 0 not to print */
	const char *error;
};

struct score {
	bool pass;
	unsigned int score, total;
	const char *error;
	struct list_head per_file_errors;
};

struct ccanlint {
	struct list_node list;

	/* More concise unique name of test. */
	const char *key;

	/* Unique name of test */
	const char *name;

	/* Can we run this test?  Return string explaining why, if not. */
	const char *(*can_run)(struct manifest *m);

	/* keep is set if you should keep the results.
	 * If timeleft is set to 0, means it timed out.
	 * score is the result, and a talloc context freed after all our
	 * depends are done. */
	void (*check)(struct manifest *m,
		      bool keep, unsigned int *timeleft, struct score *score);

	/* Can we do something about it? (NULL if not) */
	void (*handle)(struct manifest *m, struct score *score);

	/* Internal use fields: */
	/* Who depends on us? */
	struct list_head dependencies;
	/* How many things do we (still) depend on? */
	unsigned int num_depends;
	/* Did we skip a dependency?  If so, must skip this, too. */
	const char *skip;
	/* Did we fail a dependency?  If so, skip and mark as fail. */
	bool skip_fail;
	/* Did the user want to keep these results? */
	bool keep_results;
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

	/* Name (usually, within m->dir). */
	char *name;

	/* Full path name. */
	char *fullname;

	/* Pristine version of the original file.
	 * Use get_ccan_file_contents to fill this. */
	const char *contents;
	size_t contents_size;

	/* Use get_ccan_file_lines / get_ccan_line_info to fill these. */
	unsigned int num_lines;
	char **lines;
	struct line_info *line_info;

	struct list_head *doc_sections;

	/* If this file gets compiled (eg. .C file to .o file), result here. */
	char *compiled;

	/* Compiled with coverage information. */
	char *cov_compiled;
};

/* A new ccan_file, with the given name (talloc_steal onto returned value). */
struct ccan_file *new_ccan_file(const void *ctx, const char *dir, char *name);

/* Use this rather than accessing f->contents directly: loads on demand. */
const char *get_ccan_file_contents(struct ccan_file *f);

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

/* Add an error about this file (and line, if non-zero) to the score struct */
void score_file_error(struct score *, struct ccan_file *f, unsigned line,
		      const char *error);

/* Normal tests. */
extern struct ccanlint trailing_whitespace;

/* Dependencies */
struct dependent {
	struct list_node node;
	struct ccanlint *dependent;
};

/* Are we happy to compile stuff, or just non-intrusive tests? */
extern bool safe_mode;

/* Where is the ccan dir?  Available after first manifest. */
extern const char *ccan_dir;

#endif /* CCAN_LINT_H */
