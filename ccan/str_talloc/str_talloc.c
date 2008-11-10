#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include "str_talloc.h"
#include <ccan/talloc/talloc.h>

char **strsplit(const void *ctx, const char *string, const char *delims,
		 unsigned int *nump)
{
	char **lines = NULL;
	unsigned int max = 64, num = 0;

	lines = talloc_array(ctx, char *, max+1);

	while (*string != '\0') {
		unsigned int len = strcspn(string, delims);
		lines[num] = talloc_array(lines, char, len + 1);
		memcpy(lines[num], string, len);
		lines[num][len] = '\0';
		string += len;
		string += strspn(string, delims) ? 1 : 0;
		if (++num == max)
			lines = talloc_realloc(ctx, lines, char *, max*=2 + 1);
	}
	lines[num] = NULL;
	if (nump)
		*nump = num;
	return lines;
}

char *strjoin(const void *ctx, char *strings[], const char *delim)
{
	unsigned int i;
	char *ret = talloc_strdup(ctx, "");

	for (i = 0; strings[i]; i++) {
		ret = talloc_append_string(ret, strings[i]);
		ret = talloc_append_string(ret, delim);
	}
	return ret;
}
