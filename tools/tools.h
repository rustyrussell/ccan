#ifndef CCAN_TOOLS_H
#define CCAN_TOOLS_H

#define CFLAGS "-O3 -Wall -Wundef -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wmissing-declarations -Werror -Iccan/ -I."

char **split(const void *ctx, const char *text, const char *delims,
	     unsigned int *nump);

char **get_deps(const void *ctx, const char *dir);

void *grab_fd(const void *ctx, int fd);
void *grab_file(const void *ctx, const char *filename);

#endif /* CCAN_TOOLS_H */

