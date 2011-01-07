/*
 * ccanlint: assorted checks and advice for a ccan package
 * Copyright (C) 2008 Rusty Russell, Idris Soule
 * Copyright (C) 2010 Rusty Russell, Idris Soule
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <ctype.h>
#include <ccan/btree/btree.h>
#include <ccan/str/str.h>
#include <ccan/str_talloc/str_talloc.h>
#include <ccan/talloc/talloc.h>
#include <ccan/opt/opt.h>
#include <ccan/foreach/foreach.h>

int verbose = 0;
static LIST_HEAD(compulsory_tests);
static LIST_HEAD(normal_tests);
static LIST_HEAD(finished_tests);
bool safe_mode = false;
static struct btree *cmdline_exclude;
static struct btree *info_exclude;
static unsigned int timeout;

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
	char reply[80];

	printf("%s ", question);
	fflush(stdout);

	return fgets(reply, sizeof(reply), stdin) != NULL
		&& toupper(reply[0]) == 'Y';
}

static const char *should_skip(struct manifest *m, struct ccanlint *i)
{
	if (btree_lookup(cmdline_exclude, i->key))
		return "excluded on command line";

	if (btree_lookup(info_exclude, i->key))
		return "excluded in _info file";
	
	if (i->skip)
		return i->skip;

	if (i->skip_fail)
		return "dependency failed";

	if (i->can_run)
		return i->can_run(m);
	return NULL;
}

static bool run_test(struct ccanlint *i,
		     bool quiet,
		     unsigned int *running_score,
		     unsigned int *running_total,
		     struct manifest *m)
{
	unsigned int timeleft;
	const struct dependent *d;
	const char *skip;
	struct score *score;

	//one less test to run through
	list_for_each(&i->dependencies, d, node)
		d->dependent->num_depends--;

	score = talloc(m, struct score);
	list_head_init(&score->per_file_errors);
	score->error = NULL;
	score->pass = false;
	score->score = 0;
	score->total = 1;

	skip = should_skip(m, i);

	if (skip) {
	skip:
		if (verbose && !streq(skip, "not relevant to target"))
			printf("%s: skipped (%s)\n", i->name, skip);

		/* If we're skipping this because a prereq failed, we fail:
		 * count it as a score of 1. */
		if (i->skip_fail)
			(*running_total)++;
			
		list_del(&i->list);
		list_add_tail(&finished_tests, &i->list);
		list_for_each(&i->dependencies, d, node) {
			if (d->dependent->skip)
				continue;
			d->dependent->skip = "dependency was skipped";
			d->dependent->skip_fail = i->skip_fail;
		}
		return i->skip_fail ? false : true;
	}

	timeleft = timeout ? timeout : default_timeout_ms;
	i->check(m, i->keep_results, &timeleft, score);
	if (timeout && timeleft == 0) {
		skip = "timeout";
		goto skip;
	}

	assert(score->score <= score->total);
	if ((!score->pass && !quiet)
	    || (score->score < score->total && verbose)
	    || verbose > 1) {
		printf("%s: %s", i->name, score->pass ? "PASS" : "FAIL");
		if (score->total > 1)
			printf(" (+%u/%u)", score->score, score->total);
		printf("\n");
	}

	if ((!quiet && !score->pass) || verbose) {
		struct file_error *f;
		unsigned int lines = 1;

		if (score->error)
			printf("%s%s\n", score->error,
			       list_empty(&score->per_file_errors) ? "" : ":");

		list_for_each(&score->per_file_errors, f, list) {
			if (f->line)
				printf("%s:%u:%s\n",
				       f->file->fullname, f->line, f->error);
			else if (f->file)
				printf("%s:%s\n", f->file->fullname, f->error);
			else
				printf("%s\n", f->error);
			if (verbose < 2 && ++lines > 5) {
				printf("... more (use -vv to see them all)\n");
				break;
			}
		}
		if (!quiet && !score->pass && i->handle)
			i->handle(m, score);
	}

	*running_score += score->score;
	*running_total += score->total;

	list_del(&i->list);
	list_add_tail(&finished_tests, &i->list);

	if (!score->pass) {
		/* Skip any tests which depend on this one. */
		list_for_each(&i->dependencies, d, node) {
			if (d->dependent->skip)
				continue;
			d->dependent->skip = "dependency failed";
			d->dependent->skip_fail = true;
		}
	}
	return score->pass;
}

static void register_test(struct list_head *h, struct ccanlint *test)
{
	list_add(h, &test->list);
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

static struct ccanlint *find_test(const char *key)
{
	struct ccanlint *i;

	list_for_each(&compulsory_tests, i, list)
		if (streq(i->key, key))
			return i;

	list_for_each(&normal_tests, i, list)
		if (streq(i->key, key))
			return i;

	return NULL;
}

#undef REGISTER_TEST
#define REGISTER_TEST(name, ...) extern struct ccanlint name
#include "generated-normal-tests"
#include "generated-compulsory-tests"

static void init_tests(void)
{
	struct ccanlint *c;
	struct btree *keys, *names;
	struct list_head *list;

#undef REGISTER_TEST
#define REGISTER_TEST(name) register_test(&normal_tests, &name)
#include "generated-normal-tests"
#undef REGISTER_TEST
#define REGISTER_TEST(name) register_test(&compulsory_tests, &name)
#include "generated-compulsory-tests"

	/* Initialize dependency lists. */
	foreach_ptr(list, &compulsory_tests, &normal_tests) {
		list_for_each(list, c, list) {
			list_head_init(&c->dependencies);
		}
	}

	/* Resolve dependencies. */
	foreach_ptr(list, &compulsory_tests, &normal_tests) {
		list_for_each(list, c, list) {
			char **deps = strsplit(NULL, c->needs, " ", NULL);
			unsigned int i;

			for (i = 0; deps[i]; i++) {
				struct ccanlint *dep;
				struct dependent *dchild;

				dep = find_test(deps[i]);
				if (!dep)
					errx(1, "BUG: unknown dep '%s' for %s",
					     deps[i], c->key);
				dchild = talloc(NULL, struct dependent);
				dchild->dependent = c;
				list_add_tail(&dep->dependencies,
					      &dchild->node);
				c->num_depends++;
			}
			talloc_free(deps);
		}
	}

	/* Self-consistency check: make sure no two tests
	   have the same key or name. */
	keys = btree_new(btree_strcmp);
	names = btree_new(btree_strcmp);
	foreach_ptr(list, &compulsory_tests, &normal_tests) {
		list_for_each(list, c, list) {
			if (!btree_insert(keys, c->key))
				errx(1, "BUG: Duplicate test key '%s'",
				     c->key);
			if (!btree_insert(names, c->name))
				errx(1, "BUG: Duplicate test name '%s'",
				     c->name);
		}
	}
	btree_delete(keys);
	btree_delete(names);

	if (!verbose)
		return;

	foreach_ptr(list, &compulsory_tests, &normal_tests) {
		printf("\%s Tests\n",
		       list == &compulsory_tests ? "Compulsory" : "Normal");

		if (!list_empty(&c->dependencies)) {
			const struct dependent *d;
			printf("These depend on us:\n");
			list_for_each(&c->dependencies, d, node)
				printf("\t%s\n", d->dependent->name);
		}
	}
}

static char *keep_test(const char *testname, void *unused)
{
	struct ccanlint *i = find_test(testname);
	if (!i)
		errx(1, "No test %s to --keep", testname);
	i->keep_results = true;
	return NULL;
}

static char *skip_test(const char *testname, void *unused)
{
	btree_insert(cmdline_exclude, testname);
	return NULL;
}

static void print_tests(struct list_head *tests, const char *type)
{
	struct ccanlint *i;

	printf("%s tests:\n", type);
	/* This makes them print in topological order. */
	while ((i = get_next_test(tests)) != NULL) {
		const struct dependent *d;
		printf("   %-25s %s\n", i->key, i->name);
		list_del(&i->list);
		list_for_each(&i->dependencies, d, node)
			d->dependent->num_depends--;
	}
}

static char *list_tests(void *arg)
{
	print_tests(&compulsory_tests, "Compulsory");
	print_tests(&normal_tests, "Normal");
	exit(0);
}

static void test_dgraph_vertices(struct list_head *tests, const char *style)
{
	const struct ccanlint *i;

	list_for_each(tests, i, list) {
		/*
		 * todo: escape labels in case ccanlint test keys have
		 *       characters interpreted as GraphViz syntax.
		 */
		printf("\t\"%p\" [label=\"%s\"%s]\n", i, i->key, style);
	}
}

static void test_dgraph_edges(struct list_head *tests)
{
	const struct ccanlint *i;
	const struct dependent *d;

	list_for_each(tests, i, list)
		list_for_each(&i->dependencies, d, node)
			printf("\t\"%p\" -> \"%p\"\n", d->dependent, i);
}

static char *test_dependency_graph(void *arg)
{
	puts("digraph G {");

	test_dgraph_vertices(&compulsory_tests, ", style=filled, fillcolor=yellow");
	test_dgraph_vertices(&normal_tests,     "");

	test_dgraph_edges(&compulsory_tests);
	test_dgraph_edges(&normal_tests);

	puts("}");

	exit(0);
}

/* Remove empty lines. */
static char **collapse(char **lines, unsigned int *nump)
{
	unsigned int i, j;
	for (i = j = 0; lines[i]; i++) {
		if (lines[i][0])
			lines[j++] = lines[i];
	}
	if (nump)
		*nump = j;
	return lines;
}

static void add_info_options(struct ccan_file *info, bool mark_fails)
{
	struct doc_section *d;
	unsigned int i;
	struct ccanlint *test;

	list_for_each(get_ccan_file_docs(info), d, list) {
		if (!streq(d->type, "ccanlint"))
			continue;

		for (i = 0; i < d->num_lines; i++) {
			char **words = collapse(strsplit(d, d->lines[i], " \t",
							 NULL), NULL);
			if (!words[0])
				continue;

			if (strncmp(words[0], "//", 2) == 0)
				continue;

			test = find_test(words[0]);
			if (!test) {
				warnx("%s: unknown ccanlint test '%s'",
				      info->fullname, words[0]);
				continue;
			}

			if (!words[1]) {
				warnx("%s: no argument to test '%s'",
				      info->fullname, words[0]);
				continue;
			}

			/* Known failure? */
			if (strcasecmp(words[1], "FAIL") == 0) {
				if (mark_fails)
					btree_insert(info_exclude, words[0]);
			} else {
				if (!test->takes_options)
					warnx("%s: %s doesn't take options",
					      info->fullname, words[0]);
				/* Copy line exactly into options. */
				test->options = strstr(d->lines[i], words[0])
					+ strlen(words[0]);
			}
		}
	}
}

static bool depends_on(struct ccanlint *i, struct ccanlint *target)
{
	const struct dependent *d;

	if (i == target)
		return true;

	list_for_each(&i->dependencies, d, node) {
		if (depends_on(d->dependent, target))
			return true;
	}
	return false;
}

/* O(N^2), who cares? */
static void skip_unrelated_tests(struct ccanlint *target)
{
	struct ccanlint *i;
	struct list_head *list;

	foreach_ptr(list, &compulsory_tests, &normal_tests)
		list_for_each(list, i, list)
			if (!depends_on(i, target))
				i->skip = "not relevant to target";
}

int main(int argc, char *argv[])
{
	bool summary = false;
	unsigned int score = 0, total_score = 0;
	struct manifest *m;
	struct ccanlint *i;
	const char *prefix = "";
	char *dir = talloc_getcwd(NULL), *base_dir = dir, *target = NULL;
	
	init_tests();

	cmdline_exclude = btree_new(btree_strcmp);
	info_exclude = btree_new(btree_strcmp);

	opt_register_arg("--dir|-d", opt_set_charp, opt_show_charp, &dir,
			 "use this directory");
	opt_register_noarg("-n|--safe-mode", opt_set_bool, &safe_mode,
			 "do not compile anything");
	opt_register_noarg("-l|--list-tests", list_tests, NULL,
			 "list tests ccanlint performs (and exit)");
	opt_register_noarg("--test-dep-graph", test_dependency_graph, NULL,
			 "print dependency graph of tests in Graphviz .dot format");
	opt_register_arg("-k|--keep <testname>", keep_test, NULL, NULL,
			   "keep results of <testname> (can be used multiple times)");
	opt_register_noarg("--summary|-s", opt_set_bool, &summary,
			   "simply give one line summary");
	opt_register_noarg("--verbose|-v", opt_inc_intval, &verbose,
			   "verbose mode (up to -vvvv)");
	opt_register_arg("-x|--exclude <testname>", skip_test, NULL, NULL,
			 "exclude <testname> (can be used multiple times)");
	opt_register_arg("-t|--timeout <milleseconds>", opt_set_uintval,
			 NULL, &timeout,
			 "ignore (terminate) tests that are slower than this");
	opt_register_arg("--target <testname>", opt_set_charp,
			 NULL, &target,
			 "only run one test (and its prerequisites)");
	opt_register_noarg("-?|-h|--help", opt_usage_and_exit,
			   "\nA program for checking and guiding development"
			   " of CCAN modules.",
			   "This usage message");

	opt_parse(&argc, argv, opt_log_stderr_exit);

	if (dir[0] != '/')
		dir = talloc_asprintf_append(NULL, "%s/%s", base_dir, dir);
	if (dir != base_dir)
		prefix = talloc_append_string(talloc_basename(NULL, dir), ": ");
	if (verbose >= 3)
		compile_verbose = true;
	if (verbose >= 4)
		tools_verbose = true;

	/* We move into temporary directory, so gcov dumps its files there. */
	if (chdir(temp_dir(talloc_autofree_context())) != 0)
		err(1, "Error changing to %s temporary dir", temp_dir(NULL));

	m = get_manifest(talloc_autofree_context(), dir);

	/* Create a symlink from temp dir back to src dir's test directory. */
	if (symlink(talloc_asprintf(m, "%s/test", dir),
		    talloc_asprintf(m, "%s/test", temp_dir(NULL))) != 0)
		err(1, "Creating test symlink in %s", temp_dir(NULL));

	if (target) {
		struct ccanlint *test;

		test = find_test(target);
		if (!test)
			errx(1, "Unknown test to run '%s'", target);
		skip_unrelated_tests(test);
	}

	/* If you don't pass the compulsory tests, you get a score of 0. */
	while ((i = get_next_test(&compulsory_tests)) != NULL) {
		if (!run_test(i, summary, &score, &total_score, m)) {
			printf("%sTotal score: 0/%u\n", prefix, total_score);
			errx(1, "%s%s failed", prefix, i->name);
		}
	}

	/* --target overrides known FAIL from _info */
	add_info_options(m->info_file, !target);

	while ((i = get_next_test(&normal_tests)) != NULL)
		run_test(i, summary, &score, &total_score, m);

	printf("%sTotal score: %u/%u\n", prefix, score, total_score);
	return 0;
}
