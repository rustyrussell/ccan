#ifndef CCAN_TOOLS_H
#define CCAN_TOOLS_H

#define CFLAGS "-O3 -Wall -Wundef -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wmissing-declarations -Werror -Iccan/ -I."

char **get_deps(const void *ctx, const char *dir);

#endif /* CCAN_TOOLS_H */

