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
#include <ccan/str/str.h>
#include <ccan/tal/tal.h>
#include <ccan/take/take.h>

char **strsplit(const tal_t *ctx,
		const char *string, const char *delims, enum strsplit flags)
{
	char **parts, *str;
	size_t max = 64, num = 0;

	parts = tal_arr(ctx, char *, max + 1);
	if (unlikely(!parts)) {
		if (taken(string))
			tal_free(string);
		if (taken(delims))
			tal_free(delims);
		return NULL;
	}
	str = tal_strdup(parts, string);
	if (unlikely(!str))
		goto fail;
	if (unlikely(!delims) && is_taken(delims))
		goto fail;

	if (flags == STR_NO_EMPTY)
		str += strspn(str, delims);

	while (*str != '\0') {
		size_t len = strcspn(str, delims), dlen;

		parts[num] = str;
		dlen = strspn(str + len, delims);
		parts[num][len] = '\0';
		if (flags == STR_EMPTY_OK && dlen)
			dlen = 1;
		str += len + dlen;
		if (++num == max && !tal_resize(&parts, max*=2 + 1))
			goto fail;
	}
	parts[num] = NULL;
	if (taken(delims))
		tal_free(delims);
	return parts;

fail:
	tal_free(parts);
	if (taken(delims))
		tal_free(delims);
	return NULL;
}

char *strjoin(const tal_t *ctx,
	      char *strings[], const char *delim, enum strjoin flags)
{
	unsigned int i;
	char *ret = NULL;
	size_t totlen = 0, dlen;

	if (unlikely(!strings) && is_taken(strings))
		goto fail;

	if (unlikely(!delim) && is_taken(delim))
		goto fail;

	dlen = strlen(delim);
	ret = tal_arr(ctx, char, dlen*2+1);
	if (!ret)
		goto fail;

	ret[0] = '\0';
	for (i = 0; strings[i]; i++) {
		size_t len = strlen(strings[i]);

		if (flags == STR_NO_TRAIL && !strings[i+1])
			dlen = 0;
		if (!tal_resize(&ret, totlen + len + dlen + 1))
			goto fail;
		memcpy(ret + totlen, strings[i], len);
		totlen += len;
		memcpy(ret + totlen, delim, dlen);
		totlen += dlen;
	}
	ret[totlen] = '\0';
out:
	if (taken(strings))
		tal_free(strings);
	if (taken(delim))
		tal_free(delim);
	return ret;
fail:
	ret = tal_free(ret);
	goto out;
}

bool strreg(const tal_t *ctx, const char *string, const char *regex, ...)
{
	size_t nmatch = 1 + strcount(regex, "(");
	regmatch_t matches[nmatch];
	regex_t r;
	bool ret = false;
	unsigned int i;
	va_list ap;

	if (unlikely(!regex) && is_taken(regex))
		goto fail_no_re;

	if (regcomp(&r, regex, REG_EXTENDED) != 0)
		goto fail_no_re;

	if (unlikely(!string) && is_taken(string))
		goto fail;

	if (regexec(&r, string, nmatch, matches, 0) != 0)
		goto fail;

	ret = true;
	va_start(ap, regex);
	for (i = 1; i < nmatch; i++) {
		char **arg = va_arg(ap, char **);
		if (arg) {
			/* eg. ([a-z])? can give "no match". */
			if (matches[i].rm_so == -1)
				*arg = NULL;
			else {
				*arg = tal_strndup(ctx,
						   string + matches[i].rm_so,
						   matches[i].rm_eo
						   - matches[i].rm_so);
				/* FIXME: If we fail, we set some and leak! */
				if (!*arg) {
					ret = false;
					break;
				}
			}
		}
	}
	va_end(ap);
fail:
	regfree(&r);
fail_no_re:
	if (taken(regex))
		tal_free(regex);
	if (taken(string))
		tal_free(string);
	return ret;
}
