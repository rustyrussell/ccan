#define _GNU_SOURCE
#include <stdio.h>
#include <ccan/tap/tap.h>
#include <setjmp.h>
#include <stdlib.h>
#include "utils.h"

/* We don't actually want it to exit... */
static jmp_buf exited;
#define exit(status) longjmp(exited, (status) + 1)

#define printf saved_printf
static int saved_printf(const char *fmt, ...);

#include <ccan/opt/helpers.c>
#include <ccan/opt/opt.c>
#include <ccan/opt/usage.c>

static void reset_options(void)
{
	free(opt_table);
	opt_table = NULL;
	opt_count = 0;
}

static char *output = NULL;

static int saved_printf(const char *fmt, ...)
{
	va_list ap;
	char *p;
	int ret;

	va_start(ap, fmt);
	ret = vasprintf(&p, fmt, ap);
	va_end(ap);

	if (output) {
		output = realloc(output, strlen(output) + strlen(p) + 1);
		strcat(output, p);
		free(p);
	} else
		output = p;

	return ret;
}	

/* Test helpers. */
int main(int argc, char *argv[])
{
	plan_tests(88);

	/* opt_set_bool */
	{
		bool arg = false;
		reset_options();
		opt_register_noarg(NULL, 'a', opt_set_bool, &arg, NULL);
		ok1(parse_args(&argc, &argv, "-a", NULL));
		ok1(arg);
		opt_register_arg(NULL, 'b', opt_set_bool_arg, NULL, &arg, NULL);
		ok1(parse_args(&argc, &argv, "-b", "no", NULL));
		ok1(!arg);
		ok1(parse_args(&argc, &argv, "-b", "yes", NULL));
		ok1(arg);
		ok1(parse_args(&argc, &argv, "-b", "false", NULL));
		ok1(!arg);
		ok1(parse_args(&argc, &argv, "-b", "true", NULL));
		ok1(arg);
	}
	/* opt_set_invbool */
	{
		bool arg = true;
		reset_options();
		opt_register_noarg(NULL, 'a', opt_set_invbool, &arg, NULL);
		ok1(parse_args(&argc, &argv, "-a", NULL));
		ok1(!arg);
		opt_register_arg(NULL, 'b', opt_set_invbool_arg, NULL,
				 &arg, NULL);
		ok1(parse_args(&argc, &argv, "-b", "no", NULL));
		ok1(arg);
		ok1(parse_args(&argc, &argv, "-b", "yes", NULL));
		ok1(!arg);
		ok1(parse_args(&argc, &argv, "-b", "false", NULL));
		ok1(arg);
		ok1(parse_args(&argc, &argv, "-b", "true", NULL));
		ok1(!arg);
	}
	/* opt_set_charp */
	{
		char *arg = (char *)"wrong";
		reset_options();
		opt_register_arg(NULL, 'a', opt_set_charp, NULL, &arg, NULL);
		ok1(parse_args(&argc, &argv, "-a", "string", NULL));
		ok1(strcmp(arg, "string") == 0);
	}
	/* opt_set_intval */
	{
		int arg = 1000;
		reset_options();
		opt_register_arg(NULL, 'a', opt_set_intval, NULL, &arg, NULL);
		ok1(parse_args(&argc, &argv, "-a", "9999", NULL));
		ok1(arg == 9999);
		ok1(parse_args(&argc, &argv, "-a", "-9999", NULL));
		ok1(arg == -9999);
		ok1(parse_args(&argc, &argv, "-a", "0", NULL));
		ok1(arg == 0);
		ok1(!parse_args(&argc, &argv, "-a", "100crap", NULL));
		if (sizeof(int) == 4)
			ok1(!parse_args(&argc, &argv, "-a", "4294967296", NULL));
		else
			fail("Handle other int sizes");
	}
	/* opt_set_uintval */
	{
		unsigned int arg = 1000;
		reset_options();
		opt_register_arg(NULL, 'a', opt_set_uintval, NULL, &arg, NULL);
		ok1(parse_args(&argc, &argv, "-a", "9999", NULL));
		ok1(arg == 9999);
		ok1(!parse_args(&argc, &argv, "-a", "-9999", NULL));
		ok1(parse_args(&argc, &argv, "-a", "0", NULL));
		ok1(arg == 0);
		ok1(!parse_args(&argc, &argv, "-a", "100crap", NULL));
		ok1(!parse_args(&argc, &argv, "-a", "4294967296", NULL));
	}
	/* opt_set_longval */
	{
		long int arg = 1000;
		reset_options();
		opt_register_arg(NULL, 'a', opt_set_longval, NULL, &arg, NULL);
		ok1(parse_args(&argc, &argv, "-a", "9999", NULL));
		ok1(arg == 9999);
		ok1(parse_args(&argc, &argv, "-a", "-9999", NULL));
		ok1(arg == -9999);
		ok1(parse_args(&argc, &argv, "-a", "0", NULL));
		ok1(arg == 0);
		ok1(!parse_args(&argc, &argv, "-a", "100crap", NULL));
		if (sizeof(long) == 4)
			ok1(!parse_args(&argc, &argv, "-a", "4294967296", NULL));
		else if (sizeof(long)== 8)
			ok1(!parse_args(&argc, &argv, "-a", "18446744073709551616", NULL));
		else
			fail("FIXME: Handle other long sizes");
	}
	/* opt_set_ulongval */
	{
		unsigned long int arg = 1000;
		reset_options();
		opt_register_arg(NULL, 'a', opt_set_ulongval, NULL, &arg, NULL);
		ok1(parse_args(&argc, &argv, "-a", "9999", NULL));
		ok1(arg == 9999);
		ok1(!parse_args(&argc, &argv, "-a", "-9999", NULL));
		ok1(parse_args(&argc, &argv, "-a", "0", NULL));
		ok1(arg == 0);
		ok1(!parse_args(&argc, &argv, "-a", "100crap", NULL));
		if (sizeof(long) == 4)
			ok1(!parse_args(&argc, &argv, "-a", "4294967296", NULL));
		else if (sizeof(long)== 8)
			ok1(!parse_args(&argc, &argv, "-a", "18446744073709551616", NULL));
		else
			fail("FIXME: Handle other long sizes");
	}
	/* opt_inc_intval */
	{
		int arg = 1000;
		reset_options();
		opt_register_noarg(NULL, 'a', opt_inc_intval, &arg, NULL);
		ok1(parse_args(&argc, &argv, "-a", NULL));
		ok1(arg == 1001);
		ok1(parse_args(&argc, &argv, "-a", "-a", NULL));
		ok1(arg == 1003);
		ok1(parse_args(&argc, &argv, "-aa", NULL));
		ok1(arg == 1005);
	}

	/* opt_show_version_and_exit. */
	{
		int exitval;
		reset_options();
		opt_register_noarg(NULL, 'a',
				   opt_version_and_exit, "1.2.3", NULL);
		exitval = setjmp(exited);
		if (exitval == 0) {
			parse_args(&argc, &argv, "-a", NULL);
			fail("opt_show_version_and_exit returned?");
		} else {
			ok1(exitval - 1 == 0);
		}
		ok1(strcmp(output, "1.2.3\n") == 0);
		free(output);
		output = NULL;
	}

	/* opt_usage_and_exit. */
	{
		int exitval;
		reset_options();
		opt_register_noarg(NULL, 'a',
				   opt_usage_and_exit, "[args]", NULL);
		exitval = setjmp(exited);
		if (exitval == 0) {
			parse_args(&argc, &argv, "-a", NULL);
			fail("opt_usage_and_exit returned?");
		} else {
			ok1(exitval - 1 == 0);
		}
		ok1(strstr(output, "[args]"));
		ok1(strstr(output, argv[0]));
		ok1(strstr(output, "[-a]"));
		free(output);
		output = NULL;
	}

	/* opt_show_bool */
	{
		bool b;
		char buf[OPT_SHOW_LEN+2] = { 0 };
		buf[OPT_SHOW_LEN] = '!';

		b = true;
		opt_show_bool(buf, &b);
		ok1(strcmp(buf, "true") == 0);
		ok1(buf[OPT_SHOW_LEN] == '!');

		b = false;
		opt_show_bool(buf, &b);
		ok1(strcmp(buf, "false") == 0);
		ok1(buf[OPT_SHOW_LEN] == '!');
	}

	/* opt_show_invbool */
	{
		bool b;
		char buf[OPT_SHOW_LEN+2] = { 0 };
		buf[OPT_SHOW_LEN] = '!';

		b = true;
		opt_show_invbool(buf, &b);
		ok1(strcmp(buf, "false") == 0);
		ok1(buf[OPT_SHOW_LEN] == '!');

		b = false;
		opt_show_invbool(buf, &b);
		ok1(strcmp(buf, "true") == 0);
		ok1(buf[OPT_SHOW_LEN] == '!');
	}

	/* opt_show_charp */
	{
		char str[OPT_SHOW_LEN*2], *p;
		char buf[OPT_SHOW_LEN+2] = { 0 };
		buf[OPT_SHOW_LEN] = '!';

		/* Short test. */
		p = str;
		strcpy(p, "short");
		opt_show_charp(buf, &p);
		ok1(strcmp(buf, "\"short\"") == 0);
		ok1(buf[OPT_SHOW_LEN] == '!');

		/* Truncate test. */
		memset(p, 'x', OPT_SHOW_LEN*2);
		p[OPT_SHOW_LEN*2-1] = '\0';
		opt_show_charp(buf, &p);
		ok1(buf[0] == '"');
		ok1(buf[OPT_SHOW_LEN-1] == '"');
		ok1(buf[OPT_SHOW_LEN] == '!');
		ok1(strspn(buf+1, "x") == OPT_SHOW_LEN-2);
	}

	/* opt_show_intval */
	{
		int i;
		char buf[OPT_SHOW_LEN+2] = { 0 };
		buf[OPT_SHOW_LEN] = '!';

		i = -77;
		opt_show_intval(buf, &i);
		ok1(strcmp(buf, "-77") == 0);
		ok1(buf[OPT_SHOW_LEN] == '!');

		i = 77;
		opt_show_intval(buf, &i);
		ok1(strcmp(buf, "77") == 0);
		ok1(buf[OPT_SHOW_LEN] == '!');
	}

	/* opt_show_uintval */
	{
		unsigned int ui;
		char buf[OPT_SHOW_LEN+2] = { 0 };
		buf[OPT_SHOW_LEN] = '!';

		ui = 4294967295U;
		opt_show_uintval(buf, &ui);
		ok1(strcmp(buf, "4294967295") == 0);
		ok1(buf[OPT_SHOW_LEN] == '!');
	}

	/* opt_show_longval */
	{
		long l;
		char buf[OPT_SHOW_LEN+2] = { 0 };
		buf[OPT_SHOW_LEN] = '!';

		l = 1234567890L;
		opt_show_longval(buf, &l);
		ok1(strcmp(buf, "1234567890") == 0);
		ok1(buf[OPT_SHOW_LEN] == '!');
	}

	/* opt_show_ulongval */
	{
		unsigned long ul;
		char buf[OPT_SHOW_LEN+2] = { 0 };
		buf[OPT_SHOW_LEN] = '!';

		ul = 4294967295UL;
		opt_show_ulongval(buf, &ul);
		ok1(strcmp(buf, "4294967295") == 0);
		ok1(buf[OPT_SHOW_LEN] == '!');
	}

	return exit_status();
}
