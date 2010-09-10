#include "tools.h"
#include <ccan/talloc/talloc.h>
#include <stdlib.h>

bool compile_verbose = false;

/* Compile multiple object files into a single.  Returns errmsg if fails. */
char *link_objects(const void *ctx, const char *basename, bool in_pwd,
		   const char *objs, char **errmsg)
{
	char *file = maybe_temp_file(ctx, ".o", in_pwd, basename);

	if (compile_verbose)
		printf("Linking objects into %s\n", file);

	*errmsg = run_command(ctx, NULL, "ld -r -o %s %s", file, objs);
	if (*errmsg) {
		talloc_free(file);
		return NULL;
	}
	return file;
}

/* Compile a single C file to an object file.  Returns errmsg if fails. */
char *compile_object(const void *ctx, const char *cfile, const char *ccandir,
		     const char *extra_cflags,
		     const char *outfile)
{
	if (compile_verbose)
		printf("Compiling %s\n", outfile);
	return run_command(ctx, NULL, "cc " CFLAGS " -I%s %s -c -o %s %s",
			   ccandir, extra_cflags, outfile, cfile);
}

/* Compile and link single C file, with object files.
 * Returns error message or NULL on success. */
char *compile_and_link(const void *ctx, const char *cfile, const char *ccandir,
		       const char *objs, const char *extra_cflags,
		       const char *libs, const char *outfile)
{
	if (compile_verbose)
		printf("Compiling and linking %s\n", outfile);
	return run_command(ctx, NULL, "cc " CFLAGS " -I%s %s -o %s %s %s %s",
			   ccandir, extra_cflags, outfile, cfile, objs, libs);
}
