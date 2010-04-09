/*
 * ccanlint: assorted checks and advice for a ccan package
 * Copyright (C) 2008 Rusty Russell, Idris Soule
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
#include "../tools.h"
#include <unistd.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <ctype.h>
#include <ccan/btree/btree.h>
#include <ccan/str/str.h>
#include <ccan/str_talloc/str_talloc.h>
#include <ccan/talloc/talloc.h>

static unsigned int verbose = 0;
static LIST_HEAD(compulsory_tests);
static LIST_HEAD(normal_tests);
static LIST_HEAD(finished_tests);
bool safe_mode = false;
static struct btree *exclude;

static void usage(const char *name)
{
	fprintf(stderr, "Usage: %s [-s] [-n] [-v] [-d <dirname>]\n"
		"   -v: verbose mode\n"
		"   -s: simply give one line summary\n"
		"   -d: use this directory instead of the current one\n"
		"   -n: do not compile anything\n"
		"   -l: list tests ccanlint performs\n"
		"   -x: exclude tests (e.g. -x trailing_whitespace,valgrind)\n",
		name);
	exit(1);
}

#if 0
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
#endif

bool ask(const char *question)
{
	char reply[2];

	printf("%s ", question);
	fflush(stdout);

	return fgets(reply, sizeof(reply), stdin) != NULL
		&& toupper(reply[0]) == 'Y';
}

static const char *should_skip(struct manifest *m, struct ccanlint *i)
{
	if (btree_lookup(exclude, i->key))
		return "excluded on command line";
	
	if (i->skip_fail)
		return "dependency failed";

	if (i->skip)
		return "dependency was skipped";

	if (i->can_run)
		return i->can_run(m);
	return NULL;
}

static bool run_test(struct ccanlint *i,
		     bool quiet,
		     unsigned int *score,
		     unsigned int *total_score,
		     struct manifest *m)
{
	void *result;
	unsigned int this_score;
	const struct dependent *d;
	const char *skip;

	//one less test to run through
	list_for_each(&i->dependencies, d, node)
		d->dependent->num_depends--;

	skip = should_skip(m, i);
	if (skip) {
		if (verbose)
			printf("  %s: skipped (%s)\n", i->name, skip);

		/* If we're skipping this because a prereq failed, we fail. */
		if (i->skip_fail)
			*total_score += i->total_score;
			
		list_del(&i->list);
		list_add_tail(&finished_tests, &i->list);
		list_for_each(&i->dependencies, d, node) {
			d->dependent->skip = true;
			d->dependent->skip_fail = i->skip_fail;
		}
		return true;
	}

	result = i->check(m);
	if (!result) {
		if (verbose) {
			printf("  %s: OK", i->name);
			if (i->total_score)
				printf(" (+%u/%u)",
				       i->total_score, i->total_score);
			printf("\n");
		}
		if (i->total_score) {
			*score += i->total_score;
			*total_score += i->total_score;
		}

		list_del(&i->list);
		list_add_tail(&finished_tests, &i->list);
		return true;
	}

	if (i->score)
		this_score = i->score(m, result);
	else
		this_score = 0;

	list_del(&i->list);
	list_add_tail(&finished_tests, &i->list);

	*total_score += i->total_score;
	*score += this_score;
	if (verbose) {
		printf("  %s: FAIL (+%u/%u)\n",
		       i->name, this_score, i->total_score);
	}
	if (!quiet) {
		printf("%s\n", i->describe(m, result));

		if (i->handle)
			i->handle(m, result);
	}

	/* Skip any tests which depend on this one. */
	list_for_each(&i->dependencies, d, node) {
		d->dependent->skip = true;
		d->dependent->skip_fail = true;
	}

	return false;
}

static void register_test(struct list_head *h, struct ccanlint *test, ...)
{
	va_list ap;
	struct ccanlint *depends;
	struct dependent *dchild;

	list_add(h, &test->list);

	va_start(ap, test);
	/* Careful: we might have been initialized by a dependent. */
	if (test->dependencies.n.next == NULL)
		list_head_init(&test->dependencies);

	//dependent(s) args (if any), last one is NULL
	while ((depends = va_arg(ap, struct ccanlint *)) != NULL) {
		dchild = malloc(sizeof(*dchild));
		dchild->dependent = test;
		/* The thing we depend on might not be initialized yet! */
		if (depends->dependencies.n.next == NULL)
			list_head_init(&depends->dependencies);
		list_add_tail(&depends->dependencies, &dchild->node);
		test->num_depends++;
	}
	va_end(ap);
}

/**
 * get_next_test - retrieves the next test to be processed
 **/
static inline struct ccanlint *get_next_test(struct list_head *test)
{
	struct ccanlint *i;

	if (list_empty(test))
		return NULL;

	list_for_each(test, i, list) {
		if (i->num_depends == 0)
			return i;
	}
	errx(1, "Can't make process; test dependency cycle");
}

static void init_tests(void)
{
	const struct ccanlint *i;
	struct btree *keys, *names;

#undef REGISTER_TEST
#define REGISTER_TEST(name, ...) register_test(&normal_tests, &name, __VA_ARGS__)
#include "generated-normal-tests"
#undef REGISTER_TEST
#define REGISTER_TEST(name, ...) register_test(&compulsory_tests, &name, __VA_ARGS__)
#include "generated-compulsory-tests"

	/* Self-consistency check: make sure no two tests
	   have the same key or name. */
	keys = btree_new(btree_strcmp);
	names = btree_new(btree_strcmp);
	list_for_each(&compulsory_tests, i, list) {
		if (!btree_insert(keys, i->key))
			errx(1, "BUG: Duplicate test key '%s'", i->key);
		if (!btree_insert(keys, i->name))
			errx(1, "BUG: Duplicate test name '%s'", i->name);
	}
	list_for_each(&normal_tests, i, list) {
		if (!btree_insert(keys, i->key))
			errx(1, "BUG: Duplicate test key '%s'", i->key);
		if (!btree_insert(keys, i->name))
			errx(1, "BUG: Duplicate test name '%s'", i->name);
	}
	btree_delete(keys);
	btree_delete(names);
	
	if (!verbose)
		return;

	printf("\nCompulsory Tests\n");
	list_for_each(&compulsory_tests, i, list) {
		printf("%s depends on %u others\n", i->name, i->num_depends);
		if (!list_empty(&i->dependencies)) {
			const struct dependent *d;
			printf("These depend on us:\n");
			list_for_each(&i->dependencies, d, node)
				printf("\t%s\n", d->dependent->name);
		}
	}

	printf("\nNormal Tests\n");
	list_for_each(&normal_tests, i, list) {
		printf("%s depends on %u others\n", i->name, i->num_depends);
		if (!list_empty(&i->dependencies)) {
			const struct dependent *d;
			printf("These depend on us:\n");
			list_for_each(&i->dependencies, d, node)
				printf("\t%s\n", d->dependent->name);
		}
	}
}

static void print_test(const struct ccanlint *i)
{
	int space = 25 - strlen(i->key);
	
	if (space >= 2) {
		printf("   %s", i->key);
		while (space--)
			putchar(' ');
	} else {
		printf("   %s  ", i->key);
	}
	
	printf("%s\n", i->name);
}

static void list_tests(void)
{
	const struct ccanlint *i;
	
	init_tests();
	
	printf("Compulsory tests:\n");
	list_for_each(&compulsory_tests, i, list)
		print_test(i);
	printf("Normal tests:\n");
	list_for_each(&normal_tests, i, list)
		print_test(i);
	
	exit(0);
}

int main(int argc, char *argv[])
{
	int c;
	bool summary = false;
	unsigned int score = 0, total_score = 0;
	struct manifest *m;
	struct ccanlint *i;
	const char *prefix = "", *dir = ".";
	
	exclude = btree_new(btree_strcmp);

	/* I'd love to use long options, but that's not standard. */
	/* FIXME: getopt_long ccan package? */
	while ((c = getopt(argc, argv, "sd:vnlx:")) != -1) {
		switch (c) {
		case 'd':
			dir = optarg;
			prefix = talloc_append_string(talloc_basename(NULL,
								      optarg),
						      ": ");
			break;
		case 'l':
			list_tests();
		case 's':
			summary = true;
			break;
		case 'v':
			verbose++;
			break;
		case 'n':
			safe_mode = true;
			break;
		case 'x': {
			char **exclude_strs = strsplit(NULL, optarg, ",", NULL);
			size_t i;
			for (i = 0; exclude_strs[i]; i++)
				btree_insert(exclude, exclude_strs[i]);
		} break;
		default:
			usage(argv[0]);
		}
	}

	if (optind < argc)
		usage(argv[0]);

	m = get_manifest(talloc_autofree_context(), dir);

	init_tests();

	/* If you don't pass the compulsory tests, you don't even get a score */
	if (verbose)
		printf("Compulsory tests:\n");

	while ((i = get_next_test(&compulsory_tests)) != NULL) {
		if (!run_test(i, summary, &score, &total_score, m)) {
			errx(1, "%s%s failed", prefix, i->name);
		}
	}

	if (verbose)
		printf("\nNormal tests:\n");
	while ((i = get_next_test(&normal_tests)) != NULL)
		run_test(i, summary, &score, &total_score, m);

	printf("%sTotal score: %u/%u\n", prefix, score, total_score);
	return 0;
}
