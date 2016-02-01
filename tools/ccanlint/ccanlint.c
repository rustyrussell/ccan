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
#include "../read_config_header.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <ctype.h>
#include <ccan/str/str.h>
#include <ccan/take/take.h>
#include <ccan/opt/opt.h>
#include <ccan/foreach/foreach.h>
#include <ccan/cast/cast.h>
#include <ccan/tlist/tlist.h>
#include <ccan/tal/path/path.h>
#include <ccan/strmap/strmap.h>

typedef STRMAP(struct ccanlint *) ccanlint_map_t;

int verbose = 0;
static ccanlint_map_t tests;
bool safe_mode = false;
bool keep_results = false;
bool non_ccan_deps = false;
bool build_failed = false;
static bool targeting = false;
static unsigned int timeout;

const char *config_header;

const char *ccan_dir;

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

/* Skip, but don't remove. */
static bool skip_test(struct dgraph_node *node, const char *why)
{
	struct ccanlint *c = container_of(node, struct ccanlint, node);
	c->skip = why;
	return true;
}

static const char *dep_failed(struct manifest *m)
{
	return "dependency couldn't run";
}

static bool cannot_run(struct dgraph_node *node, void *all)
{
	struct ccanlint *c = container_of(node, struct ccanlint, node);
	c->can_run = dep_failed;

	return true;
}

struct run_info {
	bool quiet;
	unsigned int score, total;
	struct manifest *m;
	const char *prefix;
	bool pass;
};

static bool run_test(struct dgraph_node *n, struct run_info *run)
{
	struct ccanlint *i = container_of(n, struct ccanlint, node);
	unsigned int timeleft;
	struct score *score;

	if (i->done)
		return true;

	score = tal(run->m, struct score);
	list_head_init(&score->per_file_errors);
	score->error = NULL;
	score->pass = false;
	score->score = 0;
	score->total = 1;

	/* We can see skipped things in two cases:
	 * (1) _info excluded them (presumably because they fail).
	 * (2) A prerequisite failed.
	 */
	if (i->skip) {
		if (verbose)
			printf("%s%s: skipped (%s)\n",
			       run->prefix, i->name, i->skip);
		/* Pass us up to the test which failed, not us. */
		score->pass = true;
		goto out;
	}

	if (i->can_run) {
		i->skip = i->can_run(run->m);
		if (i->skip) {
			/* Test doesn't apply, or can't run?  That's OK. */
			if (verbose > 1)
				printf("%s%s: skipped (%s)\n",
				       run->prefix, i->name, i->skip);
			/* Mark our dependencies to skip. */
			dgraph_traverse_from(&i->node, cannot_run, NULL);
			score->pass = true;
			score->total = 0;
			goto out;
		}
	}

	timeleft = timeout ? timeout : default_timeout_ms;
	i->check(run->m, &timeleft, score);
	if (timeout && timeleft == 0) {
		i->skip = "timeout";
		if (verbose)
			printf("%s%s: skipped (%s)\n",
			       run->prefix, i->name, i->skip);
		/* Mark our dependencies to skip. */
		dgraph_traverse_from(&i->node, skip_test,
				     "dependency timed out");
		score->pass = true;
		score->total = 0;
		goto out;
	}

	assert(score->score <= score->total);
	if ((!score->pass && !run->quiet)
	    || (score->score < score->total && verbose)
	    || verbose > 1) {
		printf("%s%s (%s): %s",
		       run->prefix, i->name, i->key,
		       score->pass ? "PASS" : "FAIL");
		if (score->total > 1)
			printf(" (+%u/%u)", score->score, score->total);
		printf("\n");
	}

	if ((!run->quiet && !score->pass) || verbose) {
		if (score->error) {
			printf("%s%s", score->error,
			       strends(score->error, "\n") ? "" : "\n");
		}
	}
	if (!run->quiet && score->score < score->total && i->handle)
		i->handle(run->m, score);

	if (!score->pass) {
		/* Skip any tests which depend on this one. */
		dgraph_traverse_from(&i->node, skip_test, "dependency failed");
	}

out:
	run->score += score->score;
	run->total += score->total;

	/* FIXME: Free score. */
	run->pass &= score->pass;
	i->done = true;

	if (!score->pass && i->compulsory) {
		warnx("%s%s failed", run->prefix, i->name);
		run->score = 0;
		return false;
	}
	return true;
}

static void register_test(struct ccanlint *test)
{
	if (!strmap_add(&tests, test->key, test))
		err(1, "Adding test %s", test->key);
	test->options = tal_arr(NULL, char *, 1);
	test->options[0] = NULL;
	dgraph_init_node(&test->node);
}

static bool get_test(const char *member, struct ccanlint *i,
		     struct ccanlint **ret)
{
	if (tlist_empty(&i->node.edge[DGRAPH_TO])) {
		*ret = i;
		return false;
	}
	return true;
}

/**
 * get_next_test - retrieves the next test to be processed
 **/
static inline struct ccanlint *get_next_test(void)
{
	struct ccanlint *i = NULL;

	strmap_iterate(&tests, get_test, &i);
	if (i)
		return i;

	if (strmap_empty(&tests))
		return NULL;

	errx(1, "Can't make process; test dependency cycle");
}

static struct ccanlint *find_test(const char *key)
{
	return strmap_get(&tests, key);
}

bool is_excluded(const char *name)
{
	return find_test(name)->skip != NULL;
}

static bool init_deps(const char *member, struct ccanlint *c, void *unused)
{
	char **deps = tal_strsplit(NULL, c->needs, " ", STR_EMPTY_OK);
	unsigned int i;

	for (i = 0; deps[i]; i++) {
		struct ccanlint *dep;

		dep = find_test(deps[i]);
		if (!dep)
			errx(1, "BUG: unknown dep '%s' for %s",
			     deps[i], c->key);
		dgraph_add_edge(&dep->node, &c->node);
	}
	tal_free(deps);
	return true;
}

static bool check_names(const char *member, struct ccanlint *c,
			ccanlint_map_t *names)
{
	if (!strmap_add(names, c->name, c))
		err(1, "Duplicate name %s", c->name);
	return true;
}

static void init_tests(void)
{
	ccanlint_map_t names;
	struct ccanlint **table;
	size_t i, num;

	strmap_init(&tests);

	table = autodata_get(ccanlint_tests, &num);
	for (i = 0; i < num; i++)
		register_test(table[i]);
	autodata_free(table);

	strmap_iterate(&tests, init_deps, NULL);

	/* Check for duplicate names. */
	strmap_init(&names);
	strmap_iterate(&tests, check_names, &names);
	strmap_clear(&names);
}

static bool reset_test(struct dgraph_node *node, void *unused)
{
	struct ccanlint *c = container_of(node, struct ccanlint, node);
	c->skip = NULL;
	c->done = false;
	return true;
}

static void reset_tests(struct dgraph_node *all)
{
	dgraph_traverse_to(all, reset_test, NULL);
}

static bool print_deps(const char *member, struct ccanlint *c, void *unused)
{
	if (!tlist_empty(&c->node.edge[DGRAPH_FROM])) {
		struct dgraph_edge *e;

		printf("These depend on %s:\n", c->key);
		dgraph_for_each_edge(&c->node, e, DGRAPH_FROM) {
			struct ccanlint *to = container_of(e->n[DGRAPH_TO],
							   struct ccanlint,
							   node);
			printf("\t%s\n", to->key);
		}
	}
	return true;
}

static void print_test_depends(void)
{
	printf("Tests:\n");

	strmap_iterate(&tests, print_deps, NULL);
}


static void show_tmpdir(const char *dir)
{
	printf("You can find ccanlint working files in '%s'\n", dir);
}

static char *keep_tests(void *unused)
{
	keep_results = true;

	/* Don't automatically destroy temporary dir. */
	keep_temp_dir();
	tal_add_destructor(temp_dir(), show_tmpdir);
	return NULL;
}

static bool remove_test(struct dgraph_node *node, const char *why)
{
	struct ccanlint *c = container_of(node, struct ccanlint, node);
	c->skip = why;
	dgraph_clear_node(node);
	return true;
}

static char *exclude_test(const char *testname, void *unused)
{
	struct ccanlint *i = find_test(testname);
	if (!i)
		return tal_fmt(NULL, "No test %s to --exclude", testname);

	/* Remove this, and everything which depends on it. */
	dgraph_traverse_from(&i->node, remove_test, "excluded on command line");
	remove_test(&i->node, "excluded on command line");
	return NULL;
}

static void skip_test_and_deps(struct ccanlint *c, const char *why)
{
	/* Skip this, and everything which depends on us. */
	dgraph_traverse_from(&c->node, skip_test, why);
	skip_test(&c->node, why);
}

static char *list_tests(void *arg)
{
	struct ccanlint *i;

	printf("Tests:\n");
	/* This makes them print in topological order. */
	while ((i = get_next_test()) != NULL) {
		printf("   %-25s %s\n", i->key, i->name);
		dgraph_clear_node(&i->node);
		strmap_del(&tests, i->key, NULL);
	}
	exit(0);
}

static bool draw_test(const char *member, struct ccanlint *c, const char *style)
{
	/*
	 * todo: escape labels in case ccanlint test keys have
	 *       characters interpreted as GraphViz syntax.
	 */
	printf("\t\"%p\" [label=\"%s\"%s]\n", c, c->key, style);
	return true;
}

static void test_dgraph_vertices(const char *style)
{
	strmap_iterate(&tests, draw_test, style);
}

static bool draw_edges(const char *member, struct ccanlint *c, void *unused)
{
	struct dgraph_edge *e;

	dgraph_for_each_edge(&c->node, e, DGRAPH_FROM) {
		struct ccanlint *to = container_of(e->n[DGRAPH_TO],
						   struct ccanlint,
						   node);
		printf("\t\"%p\" -> \"%p\"\n", c->name, to->name);
	}
	return true;
}

static void test_dgraph_edges(void)
{
	strmap_iterate(&tests, draw_edges, NULL);
}

static char *test_dependency_graph(void *arg)
{
	puts("digraph G {");

	test_dgraph_vertices("");
	test_dgraph_edges();

	puts("}");

	exit(0);
}

static void add_options(struct ccanlint *test, char **options,
			unsigned int num_options)
{
	unsigned int num;

	if (!test->options)
		num = 0;
	else
		/* -1, because last one is NULL. */
		num = tal_count(test->options) - 1;

	tal_resize(&test->options, num + num_options + 1);
	memcpy(&test->options[num], options, (num_options + 1)*sizeof(char *));
}

void add_info_options(struct ccan_file *info)
{
	struct doc_section *d;
	unsigned int i;
	struct ccanlint *test;

	list_for_each(get_ccan_file_docs(info), d, list) {
		if (!streq(d->type, "ccanlint"))
			continue;

		for (i = 0; i < d->num_lines; i++) {
			char **words = tal_strsplit(d, d->lines[i], " \t",
						    STR_NO_EMPTY);
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
				if (!targeting)
					skip_test_and_deps(test,
							   "excluded in _info"
							   " file");
			} else {
				if (!test->takes_options)
					warnx("%s: %s doesn't take options",
					      info->fullname, words[0]);
				add_options(test, words+1, tal_count(words)-1);
			}
		}
	}
}

/* If options are of form "filename:<option>" they only apply to that file */
char **per_file_options(const struct ccanlint *test, struct ccan_file *f)
{
	char **ret;
	unsigned int i, j = 0;

	/* Fast path. */
	if (!test->options[0])
		return test->options;

	ret = tal_arr(f, char *, tal_count(test->options));
	for (i = 0; test->options[i]; i++) {
		char *optname;

		if (!test->options[i] || !strchr(test->options[i], ':')) {
			optname = test->options[i];
		} else if (strstarts(test->options[i], f->name)
			   && test->options[i][strlen(f->name)] == ':') {
			optname = test->options[i] + strlen(f->name) + 1;
		} else
			continue;

		/* FAIL overrides anything else. */
		if (streq(optname, "FAIL")) {
			ret = tal_arr(f, char *, 2);
			ret[0] = (char *)"FAIL";
			ret[1] = NULL;
			return ret;
		}
		ret[j++] = optname;
	}
	ret[j] = NULL;

	/* Shrink it to size so tal_array_length() works as expected. */
	tal_resize(&ret, j + 1);
	return ret;
}

static char *opt_set_const_charp(const char *arg, const char **p)
{
	return opt_set_charp(arg, cast_const2(char **, p));
}

static char *opt_set_target(const char *arg, struct dgraph_node *all)
{
	struct ccanlint *t = find_test(arg);
	if (!t)
		return tal_fmt(NULL, "unknown --target %s", arg);

	targeting = true;
	dgraph_add_edge(&t->node, all);
	return NULL;
}

static bool run_tests(struct dgraph_node *all,
		      bool summary,
		      bool deps_fail_ignore,
		      struct manifest *m,
		      const char *prefix)
{
	struct run_info run;
	const char *comment = "";

	run.quiet = summary;
	run.m = m;
	run.prefix = prefix;
	run.score = run.total = 0;
	run.pass = true;

	non_ccan_deps = build_failed = false;

	dgraph_traverse_to(all, run_test, &run);

	/* We can completely fail if we're missing external stuff: ignore */
	if (deps_fail_ignore && non_ccan_deps && build_failed) {
		comment = " (missing non-ccan dependencies?)";
		run.pass = true;
	} else if (!run.pass) {
		comment = " FAIL!";
	}
	printf("%sTotal score: %u/%u%s\n",
	       prefix, run.score, run.total, comment);

	return run.pass;
}

static bool add_to_all(const char *member, struct ccanlint *c,
		       struct dgraph_node *all)
{
	/* If we're excluded on cmdline, don't add. */
	if (!c->skip)
		dgraph_add_edge(&c->node, all);
	return true;
}

static bool test_module(struct dgraph_node *all,
			const char *dir, const char *prefix, bool summary,
			bool deps_fail_ignore)
{
	struct manifest *m = get_manifest(autofree(), dir);
	char *testlink = path_join(NULL, temp_dir(), "test");

	/* Create a symlink from temp dir back to src dir's
	 * test directory. */
	unlink(testlink);
	if (symlink(path_join(m, dir, "test"), testlink) != 0)
		err(1, "Creating test symlink in %s", temp_dir());

	return run_tests(all, summary, deps_fail_ignore, m, prefix);
}

int main(int argc, char *argv[])
{
	bool summary = false, pass = true, deps_fail_ignore = false;
	unsigned int i;
	const char *prefix = "";
	char *cwd = path_cwd(NULL), *dir;
	struct ccanlint top;  /* cannot_run may try to set ->can_run */
	const char *override_compiler = NULL, *override_cflags = NULL;
	
	/* Empty graph node to which we attach everything else. */
	dgraph_init_node(&top.node);

	opt_register_early_noarg("--verbose|-v", opt_inc_intval, &verbose,
				 "verbose mode (up to -vvvv)");
	opt_register_noarg("-n|--safe-mode", opt_set_bool, &safe_mode,
			 "do not compile anything");
	opt_register_noarg("-l|--list-tests", list_tests, NULL,
			 "list tests ccanlint performs (and exit)");
	opt_register_noarg("--test-dep-graph", test_dependency_graph, NULL,
			 "print dependency graph of tests in Graphviz .dot format");
	opt_register_noarg("-k|--keep", keep_tests, NULL,
			 "do not delete ccanlint working files");
	opt_register_noarg("--summary|-s", opt_set_bool, &summary,
			   "simply give one line summary");
	opt_register_arg("-x|--exclude <testname>", exclude_test, NULL, NULL,
			 "exclude <testname> (can be used multiple times)");
	opt_register_arg("--timeout <milleseconds>", opt_set_uintval,
			 NULL, &timeout,
			 "ignore (terminate) tests that are slower than this");
	opt_register_arg("-t|--target <testname>", opt_set_target, NULL,
			 &top.node,
			 "only run one test (and its prerequisites)");
	opt_register_arg("--compiler <compiler>", opt_set_const_charp,
			 NULL, &override_compiler, "set the compiler");
	opt_register_arg("--cflags <flags>", opt_set_const_charp,
			 NULL, &override_cflags, "set the compiler flags");
	opt_register_noarg("--deps-fail-ignore", opt_set_bool,
			   &deps_fail_ignore,
			   "don't fail if external dependencies are missing");
	opt_register_noarg("-?|-h|--help", opt_usage_and_exit,
			   "\nA program for checking and guiding development"
			   " of CCAN modules.",
			   "This usage message");

	/* Do verbose before anything else... */
	opt_early_parse(argc, argv, opt_log_stderr_exit);

	/* We move into temporary directory, so gcov dumps its files there. */
	if (chdir(temp_dir()) != 0)
		err(1, "Error changing to %s temporary dir", temp_dir());

	init_tests();

	if (verbose >= 3) {
		compile_verbose = true;
		print_test_depends();
	}
	if (verbose >= 4)
		tools_verbose = true;

	opt_parse(&argc, argv, opt_log_stderr_exit);

	if (!targeting)
		strmap_iterate(&tests, add_to_all, &top.node);

	if (argc == 1)
		dir = cwd;
	else
		dir = path_simplify(NULL, take(path_join(NULL, cwd, argv[1])));

	ccan_dir = find_ccan_dir(dir);
	if (!ccan_dir)
		errx(1, "Cannot find ccan/ base directory in %s", dir);
	config_header = read_config_header(ccan_dir, verbose > 1);

	/* We do this after read_config_header has set compiler & cflags */
	if (override_cflags)
		cflags = override_cflags;
	if (override_compiler)
		compiler = override_compiler;

	if (argc == 1)
		pass = test_module(&top.node, cwd, "",
				   summary, deps_fail_ignore);
	else {
		for (i = 1; i < argc; i++) {
			dir = path_canon(NULL,
					 take(path_join(NULL, cwd, argv[i])));
			if (!dir)
				err(1, "Cannot get canonical name of '%s'",
				    argv[i]);

			prefix = path_join(NULL, ccan_dir, "ccan");
			prefix = path_rel(NULL, take(prefix), dir);
			prefix = tal_strcat(NULL, take(prefix), ": ");

			pass &= test_module(&top.node, dir, prefix, summary,
					    deps_fail_ignore);
			reset_tests(&top.node);
		}
	}
	return pass ? 0 : 1;
}
