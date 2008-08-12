#include "string/string.h"
#include <stdlib.h>
#include <stdio.h>
#include "string/string.c"
#include "tap/tap.h"

/* FIXME: ccanize */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

static char *substrings[] = { "far", "bar", "baz", "b", "ba", "z", "ar", NULL };

#define NUM_SUBSTRINGS (ARRAY_SIZE(substrings) - 1)

static char *strdup_rev(const char *s)
{
	char *ret = strdup(s);
	unsigned int i;

	for (i = 0; i < strlen(s); i++)
		ret[i] = s[strlen(s) - i - 1];
	return ret;
}

int main(int argc, char *argv[])
{
	unsigned int i, j, n;
	char **split, *str;
	void *ctx;
	char *strings[NUM_SUBSTRINGS * NUM_SUBSTRINGS];
	int length;
	struct stat st;

	n = 0;
	for (i = 0; i < NUM_SUBSTRINGS; i++) {
		for (j = 0; j < NUM_SUBSTRINGS; j++) {
			strings[n] = malloc(strlen(substrings[i])
					    + strlen(substrings[j]) + 1);
			sprintf(strings[n++], "%s%s",
				substrings[i], substrings[j]);
		}
	}

	plan_tests(n * n * 5 + 19);
	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			unsigned int k, identical = 0;
			char *reva, *revb;

			/* Find first difference. */
			for (k = 0; strings[i][k]==strings[j][k]; k++) {
				if (k == strlen(strings[i])) {
					identical = 1;
					break;
				}
			}

			if (identical) 
				ok1(streq(strings[i], strings[j]));
			else
				ok1(!streq(strings[i], strings[j]));

			/* Postfix test should be equivalent to prefix
			 * test on reversed string. */
			reva = strdup_rev(strings[i]);
			revb = strdup_rev(strings[j]);

			if (!strings[i][k]) {
				ok1(strstarts(strings[j], strings[i]));
				ok1(strends(revb, reva));
			} else {
				ok1(!strstarts(strings[j], strings[i]));
				ok1(!strends(revb, reva));
			}
			if (!strings[j][k]) {
				ok1(strstarts(strings[i], strings[j]));
				ok1(strends(reva, revb));
			} else {
				ok1(!strstarts(strings[i], strings[j]));
				ok1(!strends(reva, revb));
			}
		}
	}

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

	str = grab_file(NULL, "ccan/string/test/run-grab.c");
	split = strsplit(NULL, str, "\n", NULL);
	length = strlen(split[0]);
	ok1(streq(split[0], "/* This is test for grab_file() function */"));
	for(i = 1; split[i]; i++)	
		length += strlen(split[i]);
	ok1(streq(split[i-1], "/* End of grab_file() test */"));
	if (stat("ccan/string/test/run-grab.c", &st) != 0) 
		err(1, "Could not stat self");
	ok1(st.st_size == length);
		
	return exit_status();
}				
