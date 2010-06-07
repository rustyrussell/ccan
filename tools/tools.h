#ifndef CCAN_TOOLS_H
#define CCAN_TOOLS_H
#include <stdbool.h>

#define IDENT_CHARS	"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
			"abcdefghijklmnopqrstuvwxyz" \
			"01234567889_"

#define SPACE_CHARS	" \f\n\r\t\v"

/* FIXME: Nested functions break with -Wmissing-prototypes -Wmissing-declarations */
#define CFLAGS "-g -Wall -Wundef -Wstrict-prototypes -Wold-style-definition -Werror -I../.."

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
char *talloc_basename(const void *ctx, const char *dir);
char *talloc_dirname(const void *ctx, const char *dir);
char *talloc_getcwd(const void *ctx);
char *run_command(const void *ctx, unsigned int *time_ms, const char *fmt, ...);
char *temp_file(const void *ctx, const char *extension);
bool move_file(const char *oldname, const char *newname);

/* From compile.c.
 *
 * These all compile into a temporary dir, and return the filename.
 * On failure they return NULL, and errmsg is set to compiler output.
 */
/* Compile multiple object files into a single. */
char *link_objects(const void *ctx, const char *objs, char **errmsg);
/* Compile a single C file to an object file.  Returns errmsg if fails. */
char *compile_object(const void *ctx, const char *cfile, const char *ccandir,
		     const char *outfile);
/* Compile and link single C file, with object files, libs, etc.  NULL on
 * success, error output on fail. */
char *compile_and_link(const void *ctx, const char *cfile, const char *ccandir,
		       const char *objs, const char *extra_cflags,
		       const char *libs, const char *outfile);

/* If keep is false, return a temporary file.  Otherwise, base it on srcname */
char *maybe_temp_file(const void *ctx, const char *extension, bool keep,
		      const char *srcname);

/* Default wait for run_command.  Should never time out. */
extern const unsigned int default_timeout_ms;

#endif /* CCAN_TOOLS_H */
