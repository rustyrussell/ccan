#ifndef CCAN_TOOLS_H
#define CCAN_TOOLS_H
#include "config.h"
#include <ccan/compiler/compiler.h>
#include <ccan/rbuf/rbuf.h>
#include <ccan/tal/tal.h>
#include <ccan/tal/str/str.h>
#include <stdlib.h>
#include <stdbool.h>

/* These are the defaults. */
#define DEFAULT_CCAN_COMPILER "cc"
#define DEFAULT_CCAN_CFLAGS "-g"

#define IDENT_CHARS	"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
			"abcdefghijklmnopqrstuvwxyz" \
			"01234567889_"

#define SPACE_CHARS	" \f\n\r\t\v"

#define COVERAGE_CFLAGS "-fprofile-arcs -ftest-coverage"

/* Actual compiler and cflags (defaults to CCAN_COMPILER and CCAN_CFLAGS). */
extern const char *compiler, *cflags;

/* This compiles up the _info file into a temporary. */
char *compile_info(const void *ctx, const char *dir);

/* This actually compiles and runs the info file to get dependencies. */
char **get_deps(const void *ctx, const char *dir, const char *style,
		bool recurse,
		char *(*get_info)(const void *ctx, const char *dir));

/* This is safer: just looks for ccan/ strings in info */
char **get_safe_ccan_deps(const void *ctx, const char *dir, const char *style,
			  bool recurse);

/* This also needs to compile the info file:
 * style == NULL: don't recurse.
 * style == depends: recurse dependencies.
 * style == testdepends: recurse testdepends and depends.
 */
char **get_libs(const void *ctx, const char *dir, const char *style,
		char *(*get_info)(const void *ctx, const char *dir));

char **get_cflags(const void *ctx, const char *dir,
		char *(*get_info)(const void *ctx, const char *dir));

char *get_ported(const void *ctx, const char *dir, bool recurse,
		 char *(*get_info)(const void *ctx, const char *dir));

/* From tools.c */
/* If set, print all commands run, all output they give and exit status. */
extern bool tools_verbose;
bool PRINTF_FMT(4,5) run_command(const void *ctx,
				 unsigned int *time_ms,
				 char **output,
				 const char *fmt, ...);
char *run_with_timeout(const void *ctx, const char *cmd,
		       bool *ok, unsigned *timeout_ms);
const char *temp_dir(void);
void keep_temp_dir(void);
bool move_file(const char *oldname, const char *newname);

void *do_tal_realloc(void *p, size_t size);

/* Freed on exit: a good parent for auto cleanup. */
tal_t *autofree(void);

/* From compile.c.
 *
 * These all compile into a temporary dir, and return the filename.
 * On failure they return NULL, and errmsg is set to compiler output.
 */
/* If set, say what we're compiling to. */
extern bool compile_verbose;
/* Compile multiple object files into a single. */
char *link_objects(const void *ctx, const char *basename,
		   const char *objs, char **errmsg);
/* Compile a single C file to an object file.  Returns false if fails. */
bool compile_object(const void *ctx, const char *cfile, const char *ccandir,
		    const char *compiler,
		    const char *cflags,
		    const char *outfile, char **output);
/* Compile and link single C file, with object files, libs, etc. */
bool compile_and_link(const void *ctx, const char *cfile, const char *ccandir,
		      const char *objs,
		      const char *compiler, const char *cflags,
		      const char *libs, const char *outfile, char **output);

/* Returns a file in temp_dir() */
char *temp_file(const void *ctx, const char *extension, const char *srcname);

/* Default wait for run_command.  Should never time out. */
extern const unsigned int default_timeout_ms;

/* Get ccan/ top dir, given a directory within it. */
const char *find_ccan_dir(const char *base);
#endif /* CCAN_TOOLS_H */
