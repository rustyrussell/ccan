#include "tools.h"
#include <stdlib.h>

#ifndef CCAN_COMPILER
#define CCAN_COMPILER DEFAULT_CCAN_COMPILER
#endif
#ifndef CCAN_CFLAGS
#define CCAN_CFLAGS DEFAULT_CCAN_CFLAGS
#endif
const char *compiler = CCAN_COMPILER;
const char *cflags = CCAN_CFLAGS;

bool compile_verbose = false;

/* Compile multiple object files into a single.  Returns NULL if fails. */
char *link_objects(const void *ctx, const char *basename,
		   const char *objs, char **errmsg)
{
	char *file = temp_file(ctx, ".o", basename);

	if (compile_verbose)
		printf("Linking objects into %s\n", file);

	if (run_command(ctx, NULL, errmsg, "ld -r -o %s %s", file, objs))
		return file;

	tal_free(file);
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
