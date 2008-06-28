/*
 * ccanlint: assorted checks and advice for a ccan package
 * Copyright (C) 2008 Rusty Russell
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 *   This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "ccanlint.h"
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <ctype.h>

static unsigned int verbose = 0;
static LIST_HEAD(tests);

static void init_tests(void)
{
#include "generated-init-tests" 
}

static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-s] [-v] [-d <dirname>]\n"
		"   -v: verbose mode\n"
		"   -s: simply give one line per FAIL and total score\n"
		"   -d: use this directory instead of the current one\n",
		name);
	exit(1);
}

static void indent_print(const char *string)
{
	while (*string) {
		unsigned int line = strcspn(string, "\n");
		printf("\t%.*s", line, string);
		if (string[line] == '\n') {
			printf("\n");
			line++;
		}
		string += line;
	}
}

bool ask(const char *question)
{
	char reply[2];

	printf("%s ", question);
	fflush(stdout);

	return fgets(reply, sizeof(reply), stdin) != NULL
		&& toupper(reply[0]) == 'Y';
}

static bool run_test(const struct ccanlint *i,
		     bool summary,
		     unsigned int *score,
		     unsigned int *total_score,
		     struct manifest *m)
{
	void *result;
	unsigned int this_score;

	if (i->total_score)
		*total_score += i->total_score;

	result = i->check(m);
	if (!result) {
		if (verbose)
			printf("  %s: OK\n", i->name);
		if (i->total_score)
			*score += i->total_score;
		return true;
	}

	if (i->score)
		this_score = i->score(m, result);
	else
		this_score = 0;

	*score += this_score;
	if (summary) {
		printf("%s FAILED (%u/%u)\n",
		       i->name, this_score, i->total_score);

		if (verbose)
			indent_print(i->describe(m, result));
		return false;
	}

	printf("%s\n", i->describe(m, result));

	if (i->handle)
		i->handle(m, result);

	return false;
}

int main(int argc, char *argv[])
{
	int c;
	bool summary = false;
	unsigned int score, total_score;
	struct manifest *m;
	const struct ccanlint *i;

	/* I'd love to use long options, but that's not standard. */
	/* FIXME: getopt_long ccan package? */
	while ((c = getopt(argc, argv, "sd:v")) != -1) {
		switch (c) {
		case 'd':
			if (chdir(optarg) != 0)
				err(1, "Changing into directory '%s'", optarg);
			break;
		case 's':
			summary = true;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage(argv[0]);
		}
	}

	if (optind < argc)
		usage(argv[0]);

	m = get_manifest();

	init_tests();

	/* If you don't pass the compulsory tests, you don't even get a score */
	if (verbose)
		printf("Compulsory tests:\n");
	list_for_each(&tests, i, list)
		if (!i->total_score)
			if (!run_test(i, summary, &score, &total_score, m))
				exit(1);

	if (verbose)
		printf("\nNormal tests:\n");
	score = total_score = 0;
	list_for_each(&tests, i, list)
		if (i->total_score)
			run_test(i, summary, &score, &total_score, m);

	printf("Total score: %u/%u\n", score, total_score);

	return 0;
}
