#include <ccan/tal/str/str.h>
#include <stdlib.h>
#include <stdio.h>
#include <ccan/tal/str/str.c>
#include <ccan/tap/tap.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

static const char *substrings[]
= { "far", "bar", "baz", "b", "ba", "z", "ar", NULL };

int main(int argc, char *argv[])
{
	char **split, *str;
	void *ctx;

	plan_tests(24);
	split = strsplit(NULL, "hello  world", " ", STR_EMPTY_OK);
	ok1(!strcmp(split[0], "hello"));
	ok1(!strcmp(split[1], ""));
	ok1(!strcmp(split[2], "world"));
	ok1(split[3] == NULL);
	tal_free(split);

	split = strsplit(NULL, "hello  world", " ", STR_NO_EMPTY);
	ok1(!strcmp(split[0], "hello"));
	ok1(!strcmp(split[1], "world"));
	ok1(split[2] == NULL);
	tal_free(split);

	split = strsplit(NULL, "  hello  world", " ", STR_NO_EMPTY);
	ok1(!strcmp(split[0], "hello"));
	ok1(!strcmp(split[1], "world"));
	ok1(split[2] == NULL);
	tal_free(split);

	split = strsplit(NULL, "hello  world", "o ", STR_EMPTY_OK);
	ok1(!strcmp(split[0], "hell"));
	ok1(!strcmp(split[1], ""));
	ok1(!strcmp(split[2], ""));
	ok1(!strcmp(split[3], "w"));
	ok1(!strcmp(split[4], "rld"));
	ok1(split[5] == NULL);

	ctx = split;
	split = strsplit(ctx, "hello  world", "o ", STR_EMPTY_OK);
	ok1(tal_parent(split) == ctx);
	tal_free(ctx);

	str = strjoin(NULL, (char **)substrings, ", ", STR_TRAIL);
	ok1(!strcmp(str, "far, bar, baz, b, ba, z, ar, "));
	ctx = str;
	str = strjoin(ctx, (char **)substrings, "", STR_TRAIL);
	ok1(!strcmp(str, "farbarbazbbazar"));
	ok1(tal_parent(str) == ctx);
	str = strjoin(ctx, (char **)substrings, ", ", STR_NO_TRAIL);
	ok1(tal_parent(str) == ctx);
	ok1(!strcmp(str, "far, bar, baz, b, ba, z, ar"));
	str = strjoin(ctx, (char **)substrings, "", STR_NO_TRAIL);
	ok1(!strcmp(str, "farbarbazbbazar"));
	ok1(tal_parent(str) == ctx);
	tal_free(ctx);

	return exit_status();
}
