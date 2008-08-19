#ifndef CCAN_TOOLS_H
#define CCAN_TOOLS_H

#define CFLAGS "-O3 -Wall -Wundef -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wmissing-declarations -Werror -Iccan/ -I."

/* This actually compiles and runs the _info.c file to get dependencies. */
char **get_deps(const void *ctx, const char *dir);

/* This is safer: just looks for ccan/ strings in _info.c */
char **get_safe_ccan_deps(const void *ctx, const char *dir);

#endif /* CCAN_TOOLS_H */

