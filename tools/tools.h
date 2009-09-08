#ifndef CCAN_TOOLS_H
#define CCAN_TOOLS_H
#include <stdbool.h>

#define IDENT_CHARS	"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
			"abcdefghijklmnopqrstuvwxyz" \
			"01234567889_"

#define SPACE_CHARS	" \f\n\r\t\v"

/* FIXME: Remove some -I */
#define CFLAGS "-O3 -Wall -Wundef -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wmissing-declarations -Werror -Iccan/ -I. -I../.."

/* This actually compiles and runs the info file to get dependencies. */
char **get_deps(const void *ctx, const char *dir, const char *name,
		bool recurse);

/* This is safer: just looks for ccan/ strings in info */
char **get_safe_ccan_deps(const void *ctx, const char *dir, const char *name,
			  bool recurse);

char *talloc_basename(const void *ctx, const char *dir);
char *talloc_dirname(const void *ctx, const char *dir);
char *talloc_getcwd(const void *ctx);
#endif /* CCAN_TOOLS_H */
