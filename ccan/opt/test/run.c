#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <ccan/opt/opt.c>
#include <ccan/opt/usage.c>
#include <ccan/opt/helpers.c>
#include "utils.h"

static void reset_options(void)
{
	free(opt_table);
	opt_table = NULL;
	opt_count = opt_num_short = opt_num_short_arg = opt_num_long = 0;
	free(err_output);
	err_output = NULL;
}

int main(int argc, char *argv[])
{
	const char *myname = argv[0];

	plan_tests(148);

	/* Simple short arg.*/
	opt_register_noarg("-a", test_noarg, NULL, "All");
	ok1(parse_args(&argc, &argv, "-a", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(argv[1] == NULL);
	ok1(test_cb_called == 1);

	/* Simple long arg. */
	opt_register_noarg("--aaa", test_noarg, NULL, "AAAAll");
	ok1(parse_args(&argc, &argv, "--aaa", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(argv[1] == NULL);
	ok1(test_cb_called == 2);

	/* Both long and short args. */
	opt_register_noarg("--aaa/-a", test_noarg, NULL, "AAAAAAll");
	ok1(parse_args(&argc, &argv, "--aaa", "-a", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(argv[1] == NULL);
	ok1(test_cb_called == 4);

	/* Extra arguments preserved. */
	ok1(parse_args(&argc, &argv, "--aaa", "-a", "extra", "args", NULL));
	ok1(argc == 3);
	ok1(argv[0] == myname);
	ok1(strcmp(argv[1], "extra") == 0);
	ok1(strcmp(argv[2], "args") == 0);
	ok1(test_cb_called == 6);

	/* Argument variants. */
	reset_options();
	test_cb_called = 0;
	opt_register_arg("-a/--aaa", test_arg, NULL, "aaa", "AAAAAAll");
	ok1(parse_args(&argc, &argv, "--aaa", "aaa", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(test_cb_called == 1);

	ok1(parse_args(&argc, &argv, "--aaa=aaa", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(test_cb_called == 2);

	ok1(parse_args(&argc, &argv, "-a", "aaa", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(test_cb_called == 3);

	/* Now, tables. */
	/* Short table: */
	reset_options();
	test_cb_called = 0;
	opt_register_table(short_table, NULL);
	ok1(parse_args(&argc, &argv, "-a", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(argv[1] == NULL);
	ok1(test_cb_called == 1);
	/* This one needs an arg. */
	ok1(parse_args(&argc, &argv, "-b", NULL) == false);
	ok1(test_cb_called == 1);
	ok1(parse_args(&argc, &argv, "-b", "b", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(argv[1] == NULL);
	ok1(test_cb_called == 2);

	/* Long table: */
	reset_options();
	test_cb_called = 0;
	opt_register_table(long_table, NULL);
	ok1(parse_args(&argc, &argv, "--ddd", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(argv[1] == NULL);
	ok1(test_cb_called == 1);
	/* This one needs an arg. */
	ok1(parse_args(&argc, &argv, "--eee", NULL) == false);
	ok1(test_cb_called == 1);
	ok1(parse_args(&argc, &argv, "--eee", "eee", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(argv[1] == NULL);
	ok1(test_cb_called == 2);

	/* Short and long, both. */
	reset_options();
	test_cb_called = 0;
	opt_register_table(long_and_short_table, NULL);
	ok1(parse_args(&argc, &argv, "-g", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(argv[1] == NULL);
	ok1(test_cb_called == 1);
	ok1(parse_args(&argc, &argv, "--ggg", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(argv[1] == NULL);
	ok1(test_cb_called == 2);
	/* This one needs an arg. */
	ok1(parse_args(&argc, &argv, "-h", NULL) == false);
	ok1(test_cb_called == 2);
	ok1(parse_args(&argc, &argv, "-h", "hhh", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(argv[1] == NULL);
	ok1(test_cb_called == 3);
	ok1(parse_args(&argc, &argv, "--hhh", NULL) == false);
	ok1(test_cb_called == 3);
	ok1(parse_args(&argc, &argv, "--hhh", "hhh", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(argv[1] == NULL);
	ok1(test_cb_called == 4);

	/* Those will all work as tables. */
	test_cb_called = 0;
	reset_options();
	opt_register_table(subtables, NULL);
	ok1(parse_args(&argc, &argv, "-a", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(argv[1] == NULL);
	ok1(test_cb_called == 1);
	/* This one needs an arg. */
	ok1(parse_args(&argc, &argv, "-b", NULL) == false);
	ok1(test_cb_called == 1);
	ok1(parse_args(&argc, &argv, "-b", "b", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(argv[1] == NULL);
	ok1(test_cb_called == 2);

	ok1(parse_args(&argc, &argv, "--ddd", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(argv[1] == NULL);
	ok1(test_cb_called == 3);
	/* This one needs an arg. */
	ok1(parse_args(&argc, &argv, "--eee", NULL) == false);
	ok1(test_cb_called == 3);
	ok1(parse_args(&argc, &argv, "--eee", "eee", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(argv[1] == NULL);
	ok1(test_cb_called == 4);

	/* Short and long, both. */
	ok1(parse_args(&argc, &argv, "-g", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(argv[1] == NULL);
	ok1(test_cb_called == 5);
	ok1(parse_args(&argc, &argv, "--ggg", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(argv[1] == NULL);
	ok1(test_cb_called == 6);
	/* This one needs an arg. */
	ok1(parse_args(&argc, &argv, "-h", NULL) == false);
	ok1(test_cb_called == 6);
	ok1(parse_args(&argc, &argv, "-h", "hhh", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(argv[1] == NULL);
	ok1(test_cb_called == 7);
	ok1(parse_args(&argc, &argv, "--hhh", NULL) == false);
	ok1(test_cb_called == 7);
	ok1(parse_args(&argc, &argv, "--hhh", "hhh", NULL));
	ok1(argc == 1);
	ok1(argv[0] == myname);
	ok1(argv[1] == NULL);
	ok1(test_cb_called == 8);

	/* Now the tricky one: -? must not be confused with an unknown option */
	test_cb_called = 0;
	reset_options();

	/* glibc's getopt does not handle ? with arguments. */
	opt_register_noarg("-?", test_noarg, NULL, "Help");
	ok1(parse_args(&argc, &argv, "-?", NULL));
	ok1(test_cb_called == 1);
	ok1(parse_args(&argc, &argv, "-a", NULL) == false);
	ok1(test_cb_called == 1);
	ok1(strstr(err_output, ": -a: unrecognized option"));
	ok1(parse_args(&argc, &argv, "--aaaa", NULL) == false);
	ok1(test_cb_called == 1);
	ok1(strstr(err_output, ": --aaaa: unrecognized option"));

	test_cb_called = 0;
	reset_options();

	return exit_status();
}
