#include <ccan/stringbuilder/stringbuilder.h>
#include <ccan/stringbuilder/stringbuilder.c>
#include <ccan/str/str.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <ccan/tap/tap.h>

int main(int argc, char *argv[])
{
	char string[20];
	const char* str_array[] = {
		"xxx", "yyy"
	};
	int res;

	res = stringbuilder(string, sizeof(string), NULL,
			"aaa", "bbb");
	printf("res: %s, string: %s\n",
			strerror(res), string);
	ok1(res == 0);
	ok1(streq(string, "aaabbb"));

	res = stringbuilder(string, sizeof(string), NULL,
			"aaaaa", "bbbbb", "ccccc", "ddddd",
			"eeeee", "fffff");
	printf("res: %s, string: %s\n",
			strerror(res), string);
	ok1(res == EMSGSIZE);

	res = stringbuilder(string, sizeof(string), ", ",
			"aaa");
	printf("res: %s, string: %s\n",
			strerror(res), string);
	ok1(res == 0);
	ok1(streq(string, "aaa"));

	res = stringbuilder(string, sizeof(string), ", ",
			"aaa", "bbb");
	printf("res: %s, string: %s\n",
			strerror(res), string);
	ok1(res == 0);
	ok1(streq(string, "aaa, bbb"));

	res = stringbuilder_array(string, sizeof(string), NULL,
			sizeof(str_array)/sizeof(str_array[0]), str_array);
	printf("res: %s, string: %s\n",
			strerror(res), string);
	ok1(res == 0);
	ok1(streq(string, "xxxyyy"));

	res = stringbuilder_array(string, sizeof(string), ", ",
			sizeof(str_array)/sizeof(str_array[0]), str_array);
	printf("res: %s, string: %s\n",
			strerror(res), string);
	ok1(res == 0);
	ok1(streq(string, "xxx, yyy"));

	return exit_status();
}
