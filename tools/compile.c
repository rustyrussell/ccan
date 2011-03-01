#include "tools.h"
#include <ccan/talloc/talloc.h>
#include <stdlib.h>

bool compile_verbose = false;

/* Compile multiple object files into a single.  Returns NULL if fails. */
char *link_objects(const void *ctx, const char *basename, bool in_pwd,
		   const char *objs, char **errmsg)
{
	char *file = maybe_temp_file(ctx, ".o", in_pwd, basename);

	if (compile_verbose)
		printf("Linking objects into %s\n", file);

	if (run_command(ctx, NULL, errmsg, "ld -r -o %s %s", file, objs))
		return file;

	talloc_free(file);
	return NULL;
}

/* Compile a single C file to an object file. */
bool compile_object(const void *ctx, const char *cfile, const char *ccandir,
		    const char *compiler,
		    const char *cflags,
		    const char *outfile, char **output)
{
	if (compile_verbose)
		printf("Compiling %s\n", outfile);
	return run_command(ctx, NULL, output,
			   "%s %s -I%s -c -o %s %s",
			   compiler, cflags, ccandir, outfile, cfile);
}

/* Compile and link single C file, with object files.
 * Returns false on failure. */
bool compile_and_link(const void *ctx, const char *cfile, const char *ccandir,
		      const char *objs, const char *compiler,
		      const char *cflags,
		      const char *libs, const char *outfile, char **output)
{
	if (compile_verbose)
		printf("Compiling and linking %s\n", outfile);
	return run_command(ctx, NULL, output,
			   "%s %s -I%s -o %s %s %s %s",
			   compiler, cflags,
			   ccandir, outfile, cfile, objs, libs);
}
