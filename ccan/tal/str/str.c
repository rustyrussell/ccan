/* Licensed under BSD-MIT - see LICENSE file for details */
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include "str.h"
#include <sys/types.h>
#include <regex.h>
#include <stdarg.h>
#include <unistd.h>
#include <ccan/tal/tal.h>
#include <ccan/str/str.h>

char **strsplit(const void *ctx, const char *string, const char *delims,
		enum strsplit flags)
{
	char **lines = NULL;
	size_t max = 64, num = 0;

	lines = tal_arr(ctx, char *, max+1);

	if (flags == STR_NO_EMPTY)
		string += strspn(string, delims);

	while (*string != '\0') {
		size_t len = strcspn(string, delims), dlen;

		lines[num] = tal_arr(lines, char, len + 1);
		memcpy(lines[num], string, len);
		lines[num][len] = '\0';
		string += len;
		dlen = strspn(string, delims);
		if (flags == STR_EMPTY_OK && dlen)
			dlen = 1;
		string += dlen;
		if (++num == max)
			tal_resize(&lines, max*=2 + 1);
	}
	lines[num] = NULL;
	return lines;
}

char *strjoin(const void *ctx, char *strings[], const char *delim,
	      enum strjoin flags)
{
	unsigned int i;
	char *ret = tal_strdup(ctx, "");
	size_t totlen = 0, dlen = strlen(delim);

	for (i = 0; strings[i]; i++) {
		size_t len = strlen(strings[i]);
		if (flags == STR_NO_TRAIL && !strings[i+1])
			dlen = 0;
		tal_resize(&ret, totlen + len + dlen + 1);
		memcpy(ret + totlen, strings[i], len);
		totlen += len;
		memcpy(ret + totlen, delim, dlen);
		totlen += dlen;
	}
	ret[totlen] = '\0';
	return ret;
}

bool strreg(const void *ctx, const char *string, const char *regex, ...)
{
	size_t nmatch = 1 + strcount(regex, "(");
	regmatch_t *matches = tal_arr(ctx, regmatch_t, nmatch);
	regex_t r;
	bool ret;

	if (!matches || regcomp(&r, regex, REG_EXTENDED) != 0)
		return false;

	if (regexec(&r, string, nmatch, matches, 0) == 0) {
		unsigned int i;
		va_list ap;

		ret = true;
		va_start(ap, regex);
		for (i = 1; i < nmatch; i++) {
			char **arg;
			arg = va_arg(ap, char **);
			if (arg) {
				/* eg. ([a-z])? can give "no match". */
				if (matches[i].rm_so == -1)
					*arg = NULL;
				else {
					*arg = tal_strndup(ctx,
						      string + matches[i].rm_so,
						      matches[i].rm_eo
						      - matches[i].rm_so);
					if (!*arg) {
						ret = false;
						break;
					}
				}
			}
		}
		va_end(ap);
	} else
		ret = false;
	tal_free(matches);
	regfree(&r);
	return ret;
}
