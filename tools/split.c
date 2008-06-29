#include "tools.h"
#include "talloc/talloc.h"
#include <string.h>

/* This is a dumb one which copies.  We could mangle instead. */
char **split(const void *ctx, const char *text, const char *delims,
	     unsigned int *nump)
{
	char **lines = NULL;
	unsigned int max = 64, num = 0;

	lines = talloc_array(ctx, char *, max+1);

	while (*text != '\0') {
		unsigned int len = strcspn(text, delims);
		lines[num] = talloc_array(lines, char, len + 1);
		memcpy(lines[num], text, len);
		lines[num][len] = '\0';
		text += len;
		text += strspn(text, delims);
		if (++num == max)
			lines = talloc_realloc(ctx, lines, char *, max*=2 + 1);
	}
	lines[num] = NULL;
	if (nump)
		*nump = num;
	return lines;
}

