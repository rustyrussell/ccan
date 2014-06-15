#include "config.h"
#include <stdio.h>
#include <string.h>

/**
 * talloc - tree allocator routines
 *
 * Talloc is a hierarchical memory pool system with destructors: you keep your
 * objects in heirarchies reflecting their lifetime.  Every pointer returned
 * from talloc() is itself a valid talloc context, from which other talloc()s
 * can be attached.  This means you can do this:
 *
 *  struct foo *X = talloc(mem_ctx, struct foo);
 *  X->name = talloc_strdup(X, "foo");
 *
 * and the pointer X->name would be a "child" of the talloc context "X" which
 * is itself a child of mem_ctx.  So if you do talloc_free(mem_ctx) then it is
 * all destroyed, whereas if you do talloc_free(X) then just X and X->name are
 * destroyed, and if you do talloc_free(X->name) then just the name element of
 * X is destroyed.
 *
 * If you think about this, then what this effectively gives you is an n-ary
 * tree, where you can free any part of the tree with talloc_free().
 *
 * Talloc has been measured with a time overhead of around 4% over glibc
 * malloc, and 48/80 bytes per allocation (32/64 bit).
 *
 * This version is based on svn://svnanon.samba.org/samba/branches/SAMBA_4_0/source/lib/talloc revision 23158.
 *
 * Example:
 *	#include <stdio.h>
 *	#include <stdarg.h>
 *	#include <err.h>
 *	#include <ccan/talloc/talloc.h>
 *
 *	// A structure containing a popened command.
 *	struct command
 *	{
 *		FILE *f;
 *		const char *command;
 *	};
 *
 *	// When struct command is freed, we also want to pclose pipe.
 *	static int close_cmd(struct command *cmd)
 *	{
 *		pclose(cmd->f);
 *		// 0 means "we succeeded, continue freeing"
 *		return 0;
 *	}
 *
 *	// This function opens a writable pipe to the given command.
 *	static struct command *open_output_cmd(const void *ctx,
 *					       const char *fmt, ...)
 *	{
 *		va_list ap;
 *		struct command *cmd = talloc(ctx, struct command);
 *
 *		if (!cmd)
 *			return NULL;
 *
 *		va_start(ap, fmt);
 *		cmd->command = talloc_vasprintf(cmd, fmt, ap);
 *		va_end(ap);
 *		if (!cmd->command) {
 *			talloc_free(cmd);
 *			return NULL;
 *		}
 *
 *		cmd->f = popen(cmd->command, "w");
 *		if (!cmd->f) {
 *			talloc_free(cmd);
 *			return NULL;
 *		}
 *		talloc_set_destructor(cmd, close_cmd);
 *		return cmd;
 *	}
 *
 *	int main(int argc, char *argv[])
 *	{
 *		struct command *cmd;
 *
 *		if (argc != 2)
 *			errx(1, "Usage: %s <command>\n", argv[0]);
 *
 *		cmd = open_output_cmd(NULL, "%s hello", argv[1]);
 *		if (!cmd)
 *			err(1, "Running '%s hello'", argv[1]);
 *		fprintf(cmd->f, "This is a test\n");
 *		talloc_free(cmd);
 *		return 0;
 *	}
 *
 * License: LGPL (v2.1 or any later version)
 */
int main(int argc, char *argv[])
{
	if (argc != 2)
		return 1;

	if (strcmp(argv[1], "depends") == 0) {
		printf("ccan/compiler\n");
		printf("ccan/typesafe_cb\n");
		return 0;
	}

	if (strcmp(argv[1], "testdepends") == 0) {
		printf("ccan/failtest\n");
		return 0;
	}

	return 1;
}
