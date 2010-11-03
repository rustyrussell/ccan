#ifndef CCAN_TOOLS_H
#define CCAN_TOOLS_H
#include <stdbool.h>
#include "config.h"

#ifndef CCAN_COMPILER
#define CCAN_COMPILER "cc"
#endif
#ifndef CCAN_CFLAGS
#define CCAN_CFLAGS "-g -Wall -Wundef -Wmissing-prototypes -Wmissing-declarations -Wstrict-prototypes -Wold-style-definition -Werror"
#endif

#define IDENT_CHARS	"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
			"abcdefghijklmnopqrstuvwxyz" \
			"01234567889_"

#define SPACE_CHARS	" \f\n\r\t\v"

#define COVERAGE_CFLAGS "-fprofile-arcs -ftest-coverage"

/* This actually compiles and runs the info file to get dependencies. */
char **get_deps(const void *ctx, const char *dir, bool recurse,
		char **infofile);

/* This is safer: just looks for ccan/ strings in info */
char **get_safe_ccan_deps(const void *ctx, const char *dir,
			  bool recurse, char **infofile);

/* This also needs to compile the info file. */
char **get_libs(const void *ctx, const char *dir,
		unsigned int *num, char **infofile);

/* From tools.c */
/* If set, print all commands run, all output they give and exit status. */
extern bool tools_verbose;
char *talloc_basename(const void *ctx, const char *dir);
char *talloc_dirname(const void *ctx, const char *dir);
char *talloc_getcwd(const void *ctx);
char *run_command(const void *ctx, unsigned int *time_ms, const char *fmt, ...);
char *run_with_timeout(const void *ctx, const char *cmd,
		       bool *ok, unsigned *timeout_ms);
char *temp_dir(const void *ctx);
bool move_file(const char *oldname, const char *newname);

/* From compile.c.
 *
 * These all compile into a temporary dir, and return the filename.
 * On failure they return NULL, and errmsg is set to compiler output.
 */
/* If set, say what we're compiling to. */
extern bool compile_verbose;
/* Compile multiple object files into a single. */
char *link_objects(const void *ctx, const char *basename, bool in_pwd,
		   const char *objs, char **errmsg);
/* Compile a single C file to an object file.  Returns errmsg if fails. */
char *compile_object(const void *ctx, const char *cfile, const char *ccandir,
		     const char *extra_cflags,
		     const char *outfile);
/* Compile and link single C file, with object files, libs, etc.  NULL on
 * success, error output on fail. */
char *compile_and_link(const void *ctx, const char *cfile, const char *ccandir,
		       const char *objs, const char *extra_cflags,
		       const char *libs, const char *outfile);

/* If in_pwd is false, return a file int temp_dir, otherwise a local file. */
char *maybe_temp_file(const void *ctx, const char *extension, bool in_pwd,
		      const char *srcname);

/* Default wait for run_command.  Should never time out. */
extern const unsigned int default_timeout_ms;

#endif /* CCAN_TOOLS_H */
