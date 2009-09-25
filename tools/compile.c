#include "tools.h"
#include <ccan/talloc/talloc.h>
#include <stdlib.h>

/* Compile multiple object files into a single.  Returns errmsg if fails. */
char *link_objects(const void *ctx, const char *outfile, const char *objs)
{
	return run_command(ctx, "cc " CFLAGS " -c -o %s %s", outfile, objs);
}

/* Compile a single C file to an object file.  Returns errmsg if fails. */
char *compile_object(const void *ctx, const char *outfile, const char *cfile)
{
	return run_command(ctx, "cc " CFLAGS " -c -o %s %s", outfile, cfile);
}

/* Compile and link single C file, with object files.
 * Returns name of result, or NULL (and fills in errmsg). */
char *compile_and_link(const void *ctx, const char *cfile, const char *objs,
		       const char *extra_cflags, const char *libs,
		       char **errmsg)
{
	char *file = temp_file(ctx, "");

	*errmsg = run_command(ctx, "cc " CFLAGS " %s -o %s %s %s %s",
			      extra_cflags, file, cfile, objs, libs);
	if (*errmsg) {
		talloc_free(file);
		return NULL;
	}
	return file;
}
