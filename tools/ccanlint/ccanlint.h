#ifndef CCAN_LINT_H
#define CCAN_LINT_H
#include <ccan/list/list.h>
#include <stdbool.h>
#include "../doc_extract.h"

struct manifest {
	char *basename;
	struct ccan_file *info_file;

	struct list_head c_files;
	struct list_head h_files;

	struct list_head run_tests;
	struct list_head api_tests;
	struct list_head compile_ok_tests;
	struct list_head compile_fail_tests;
	struct list_head other_test_files;

	struct list_head other_files;
};

struct manifest *get_manifest(void);

struct ccanlint {
	struct list_node list;

	/* Unique name of test */
	const char *name;

	/* Total score that this test is worth.  0 means compulsory tests. */
	unsigned int total_score;

	/* If this returns non-NULL, it means the check failed. */
	void *(*check)(struct manifest *m);

	/* The non-NULL return from check is passed to one of these: */

	/* So, what did this get out of the total_score?  (NULL means 0). */
	unsigned int (*score)(struct manifest *m, void *check_result);

	/* Verbose description of what was wrong. */
	const char *(*describe)(struct manifest *m, void *check_result);

	/* Can we do something about it? (NULL if not) */
	void (*handle)(struct manifest *m, void *check_result);
};

/* Ask the user a yes/no question: the answer is NO if there's an error. */
bool ask(const char *question);

struct ccan_file {
	struct list_node list;

	char *name;

	unsigned int num_lines;
	char **lines;

	struct list_head *doc_sections;
};

/* Use this rather than accessing f->lines directly: loads on demand. */
char **get_ccan_file_lines(struct ccan_file *f);

/* Similarly for ->doc_sections */
struct list_head *get_ccan_file_docs(struct ccan_file *f);

/* Call the reporting on every line in the file.  sofar contains
 * previous results. */
char *report_on_lines(struct list_head *files,
		      char *(*report)(const char *),
		      char *sofar);

/* The critical tests which mean fail if they don't pass. */
extern struct ccanlint no_info;
extern struct ccanlint has_main_header;

/* Normal tests. */
extern struct ccanlint trailing_whitespace;


#endif /* CCAN_LINT_H */
