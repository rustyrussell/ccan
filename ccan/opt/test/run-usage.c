#define _GNU_SOURCE
#include <ccan/tap/tap.h>
#include <stdarg.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdarg.h>
#include "utils.h"
#include <ccan/opt/opt.c>
#include <ccan/opt/usage.c>

static char *my_cb(void *p)
{
	return NULL;
}

/* Test helpers. */
int main(int argc, char *argv[])
{
	char *output;
	plan_tests(18);
	opt_register_table(subtables, NULL);
	opt_register_noarg("--kkk", 'k', my_cb, NULL, "magic kkk option");
	output = opt_usage("my name", "ExTrA Args");
	diag("%s", output);
	ok1(strstr(output, "Usage: my name"));
	ok1(strstr(output, "--jjj/-j <arg>"));
	ok1(strstr(output, "ExTrA Args"));
	ok1(strstr(output, "-a "));
	ok1(strstr(output, " Description of a\n"));
	ok1(strstr(output, "-b <arg>"));
	ok1(strstr(output, " Description of b\n"));
	ok1(strstr(output, "--ddd "));
	ok1(strstr(output, " Description of ddd\n"));
	ok1(strstr(output, "--eee <arg> "));
	ok1(strstr(output, " Description of eee\n"));
	ok1(strstr(output, "long table options:\n"));
	/* This table is hidden. */
	ok1(!strstr(output, "--ggg/-g "));
	ok1(!strstr(output, " Description of ggg\n"));
	ok1(!strstr(output, "--hhh/-h <arg>"));
	ok1(!strstr(output, " Description of hhh\n"));
	ok1(strstr(output, "--kkk/-k"));
	ok1(strstr(output, "magic kkk option"));
	free(output);

	return exit_status();
}
