#define _GNU_SOURCE
#include <ccan/tap/tap.h>
#include <stdarg.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdarg.h>
#include "utils.h"
#include <ccan/opt/opt.c>
#include <ccan/opt/usage.c>
#include <ccan/opt/helpers.c>

static char *my_cb(void *p)
{
	return NULL;
}

/* Test helpers. */
int main(int argc, char *argv[])
{
	char *output;

	plan_tests(38);
	opt_register_table(subtables, NULL);
	opt_register_noarg("--kkk/-k", my_cb, NULL, "magic kkk option");
	opt_register_noarg("-?", opt_usage_and_exit, "<MyArgs>...",
			   "This message");
	output = opt_usage("my name", "ExTrA Args");
	diag("%s", output);
	ok1(strstr(output, "Usage: my name"));
	ok1(strstr(output, "--jjj/-j/--lll/-l <arg>"));
	ok1(strstr(output, "ExTrA Args"));
	ok1(strstr(output, "-a "));
	ok1(strstr(output, " Description of a\n"));
	ok1(strstr(output, "-b <arg>"));
	ok1(strstr(output, " Description of b (default: b)\n"));
	ok1(strstr(output, "--ddd "));
	ok1(strstr(output, " Description of ddd\n"));
	ok1(strstr(output, "--eee <filename> "));
	ok1(strstr(output, " (default: eee)\n"));
	ok1(strstr(output, "long table options:\n"));
	ok1(strstr(output, "--ggg/-g "));
	ok1(strstr(output, " Description of ggg\n"));
	ok1(strstr(output, "-h/--hhh <arg>"));
	ok1(strstr(output, " Description of hhh\n"));
	ok1(strstr(output, "--kkk/-k"));
	ok1(strstr(output, "magic kkk option"));
	/* This entry is hidden. */
	ok1(!strstr(output, "--mmm/-m"));
	free(output);

	/* NULL should use string from registered options. */
	output = opt_usage("my name", NULL);
	diag("%s", output);
	ok1(strstr(output, "Usage: my name"));
	ok1(strstr(output, "--jjj/-j/--lll/-l <arg>"));
	ok1(strstr(output, "<MyArgs>..."));
	ok1(strstr(output, "-a "));
	ok1(strstr(output, " Description of a\n"));
	ok1(strstr(output, "-b <arg>"));
	ok1(strstr(output, " Description of b (default: b)\n"));
	ok1(strstr(output, "--ddd "));
	ok1(strstr(output, " Description of ddd\n"));
	ok1(strstr(output, "--eee <filename> "));
	ok1(strstr(output, " (default: eee)\n"));
	ok1(strstr(output, "long table options:\n"));
	ok1(strstr(output, "--ggg/-g "));
	ok1(strstr(output, " Description of ggg\n"));
	ok1(strstr(output, "-h/--hhh <arg>"));
	ok1(strstr(output, " Description of hhh\n"));
	ok1(strstr(output, "--kkk/-k"));
	ok1(strstr(output, "magic kkk option"));
	/* This entry is hidden. */
	ok1(!strstr(output, "--mmm/-m"));
	free(output);

	return exit_status();
}
