#include <ccan/str_talloc/str_talloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <ccan/str_talloc/str_talloc.c>
#include <ccan/tap/tap.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

static const char *substrings[]
= { "far", "bar", "baz", "b", "ba", "z", "ar", NULL };

int main(int argc, char *argv[])
{
	char **split, *str;
	void *ctx;

	plan_tests(16);
	split = strsplit(NULL, "hello  world", " ");
	ok1(talloc_array_length(split) == 4);
	ok1(!strcmp(split[0], "hello"));
	ok1(!strcmp(split[1], ""));
	ok1(!strcmp(split[2], "world"));
	ok1(split[3] == NULL);
	talloc_free(split);

	split = strsplit(NULL, "hello  world", "o ");
	ok1(talloc_array_length(split) == 6);
	ok1(!strcmp(split[0], "hell"));
	ok1(!strcmp(split[1], ""));
	ok1(!strcmp(split[2], ""));
	ok1(!strcmp(split[3], "w"));
	ok1(!strcmp(split[4], "rld"));
	ok1(split[5] == NULL);

	ctx = split;
	split = strsplit(ctx, "hello  world", "o ");
	ok1(talloc_parent(split) == ctx);
	talloc_free(ctx);

	str = strjoin(NULL, (char **)substrings, ", ");
	ok1(!strcmp(str, "far, bar, baz, b, ba, z, ar, "));
	ctx = str;
	str = strjoin(ctx, (char **)substrings, "");
	ok1(!strcmp(str, "farbarbazbbazar"));
	ok1(talloc_parent(str) == ctx);
	talloc_free(ctx);

	return exit_status();
}				
