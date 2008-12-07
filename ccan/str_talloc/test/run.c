#include "str_talloc/str_talloc.h"
#include <stdlib.h>
#include <stdio.h>
#include "str_talloc/str_talloc.c"
#include "tap/tap.h"
#include "str/str.h"

/* FIXME: ccanize */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

static char *substrings[] = { "far", "bar", "baz", "b", "ba", "z", "ar", NULL };

int main(int argc, char *argv[])
{
	unsigned int n;
	char **split, *str;
	void *ctx;

	plan_tests(19);
	split = strsplit(NULL, "hello  world", " ", &n);
	ok1(n == 3);
	ok1(streq(split[0], "hello"));
	ok1(streq(split[1], ""));
	ok1(streq(split[2], "world"));
	ok1(split[3] == NULL);
	talloc_free(split);

	split = strsplit(NULL, "hello  world", " ", NULL);
	ok1(streq(split[0], "hello"));
	ok1(streq(split[1], ""));
	ok1(streq(split[2], "world"));
	ok1(split[3] == NULL);
	talloc_free(split);

	split = strsplit(NULL, "hello  world", "o ", NULL);
	ok1(streq(split[0], "hell"));
	ok1(streq(split[1], ""));
	ok1(streq(split[2], ""));
	ok1(streq(split[3], "w"));
	ok1(streq(split[4], "rld"));
	ok1(split[5] == NULL);

	ctx = split;
	split = strsplit(ctx, "hello  world", "o ", NULL);
	ok1(talloc_parent(split) == ctx);
	talloc_free(ctx);

	str = strjoin(NULL, substrings, ", ");
	ok1(streq(str, "far, bar, baz, b, ba, z, ar, "));
	ctx = str;
	str = strjoin(ctx, substrings, "");
	ok1(streq(str, "farbarbazbbazar"));
	ok1(talloc_parent(str) == ctx);
	talloc_free(ctx);

	return exit_status();
}				
