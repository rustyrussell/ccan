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
#include <ccan/grab_file/grab_file.h>
#include <ccan/cast/cast.h>
#include <ccan/tlist/tlist.h>
#include <ccan/strmap/strmap.h>

struct ccanlint_map {
	STRMAP_MEMBERS(struct ccanlint *);
};

int verbose = 0;
static struct ccanlint_map tests;
bool safe_mode = false;
static bool targeting = false;
static struct btree *cmdline_exclude;
static struct btree *info_exclude;
static unsigned int timeout;
static struct dgraph_node all;

/* These are overridden at runtime if we can find config.h */
const char *compiler = NULL;
const char *cflags = NULL;

const char *config_header;

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

static bool skip_node(struct dgraph_node *to, const char *failmsg)
{
	struct ccanlint *c = container_of(to, struct ccanlint, node);
	if (!c->skip) {
		if (failmsg) {
			c->skip = failmsg;
			c->skip_fail = true;
		} else {
			c->skip = "dependency was skipped";
		}
	}
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
	const char *skip;
	struct score *score;

	if (i->done)
		return true;

	score = talloc(run->m, struct score);
	list_head_init(&score->per_file_errors);
	score->error = NULL;
	score->pass = false;
	score->score = 0;
	score->total = 1;

	skip = should_skip(run->m, i);

	if (skip) {
	skip:
		if (verbose)
			printf("%s%s: skipped (%s)\n",
			       run->prefix, i->name, skip);

		/* If we're skipping this because a prereq failed, we fail:
		 * count it as a score of 1. */
		if (i->skip_fail)
			run->total++;

		dgraph_traverse_from(&i->node, skip_node,
				     i->skip_fail ? "dependency failed" : NULL);
		dgraph_clear_node(&i->node);
		score->pass = i->skip_fail ? false : true;
		goto out;
	}

	timeleft = timeout ? timeout : default_timeout_ms;
	i->check(run->m, i->keep_results, &timeleft, score);
	if (timeout && timeleft == 0) {
		skip = "timeout";
		goto skip;
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

	run->score += score->score;
	run->total += score->total;

	if (!score->pass) {
		/* Skip any tests which depend on this one. */
		dgraph_traverse_from(&i->node, skip_node, "dependency failed");
	}
	dgraph_clear_node(&i->node);

out:
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
	test->options = talloc_array(NULL, char *, 1);
	test->options[0] = NULL;
	dgraph_init_node(&test->node);
	dgraph_add_edge(&test->node, &all);
	test->done = false;
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
	return btree_lookup(cmdline_exclude, name) != NULL
		|| btree_lookup(info_exclude, name) != NULL
		|| find_test(name)->skip != NULL;
}

static bool reset_deps(const char *member, struct ccanlint *c, void *unused)
{
	char **deps = strsplit(NULL, c->needs, " ");
	unsigned int i;

	c->skip = NULL;
	c->skip_fail = false;
	for (i = 0; deps[i]; i++) {
		struct ccanlint *dep;

		dep = find_test(deps[i]);
		if (!dep)
			errx(1, "BUG: unknown dep '%s' for %s",
			     deps[i], c->key);
		dgraph_add_edge(&dep->node, &c->node);
	}
	talloc_free(deps);
	return true;
}

static bool check_names(const char *member, struct ccanlint *c,
			struct ccanlint_map *names)
{
	if (!strmap_add(names, c->name, c))
		err(1, "Duplicate name %s", c->name);
	return true;
}

#undef REGISTER_TEST
#define REGISTER_TEST(name, ...) extern struct ccanlint name
#include "generated-testlist"

static void init_tests(void)
{
	struct ccanlint_map names;

	/* This is the default target. */
	dgraph_init_node(&all);

	strmap_init(&tests);

#undef REGISTER_TEST
#define REGISTER_TEST(name) register_test(&name)
#include "generated-testlist"

	strmap_iterate(&tests, reset_deps, NULL);

	/* Check for duplicate names. */
	strmap_init(&names);
	strmap_iterate(&tests, check_names, &names);
	strmap_clear(&names);
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


static int show_tmpdir(const char *dir)
{
	printf("You can find ccanlint working files in '%s'\n", dir);
	return 0;
}

static bool keep_one_test(const char *member, struct ccanlint *c, void *unused)
{
	c->keep_results = true;
	return true;
}

static char *keep_test(const char *testname, void *unused)
{
	if (streq(testname, "all")) {
		strmap_iterate(&tests, keep_one_test, NULL);
	} else {
		struct ccanlint *i = find_test(testname);
		if (!i)
			errx(1, "No test %s to --keep", testname);
		keep_one_test(testname, i, NULL);
	}

	/* Don't automatically destroy temporary dir. */
	talloc_set_destructor(temp_dir(NULL), show_tmpdir);
	return NULL;
}

static char *skip_test(const char *testname, void *unused)
{
	btree_insert(cmdline_exclude, testname);
	return NULL;
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

/* Remove empty lines. */
static char **collapse(char **lines, unsigned int *nump)
{
	unsigned int i, j;
	for (i = j = 0; lines[i]; i++) {
		if (lines[i][0])
			lines[j++] = lines[i];
	}
	lines[j] = NULL;
	if (nump)
		*nump = j;
	return lines;
}


static void add_options(struct ccanlint *test, char **options,
			unsigned int num_options)
{
	unsigned int num;

	if (!test->options)
		num = 0;
	else
		/* -1, because last one is NULL. */
		num = talloc_array_length(test->options) - 1;

	test->options = talloc_realloc(NULL, test->options,
				       char *,
				       num + num_options + 1);
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
			unsigned int num_words;
			char **words = collapse(strsplit(d, d->lines[i], " \t"),
						&num_words);
			if (num_words == 0)
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
					btree_insert(info_exclude, words[0]);
			} else {
				if (!test->takes_options)
					warnx("%s: %s doesn't take options",
					      info->fullname, words[0]);
				add_options(test, words+1, num_words-1);
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

	ret = talloc_array(f, char *, talloc_array_length(test->options));
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
			ret = talloc_array(f, char *, 2);
			ret[0] = (char *)"FAIL";
			ret[1] = NULL;
			return ret;
		}
		ret[j++] = optname;
	}
	ret[j] = NULL;

	/* Shrink it to size so talloc_array_length() works as expected. */
	return talloc_realloc(NULL, ret, char *, j + 1);
}

static char *demangle_string(char *string)
{
	unsigned int i;
	const char mapfrom[] = "abfnrtv";
	const char mapto[] = "\a\b\f\n\r\t\v";

	if (!strchr(string, '"'))
		return NULL;
	string = strchr(string, '"') + 1;
	if (!strrchr(string, '"'))
		return NULL;
	*strrchr(string, '"') = '\0';

	for (i = 0; i < strlen(string); i++) {
		if (string[i] == '\\') {
			char repl;
			unsigned len = 0;
			const char *p = strchr(mapfrom, string[i+1]);
			if (p) {
				repl = mapto[p - mapfrom];
				len = 1;
			} else if (strlen(string+i+1) >= 3) {
				if (string[i+1] == 'x') {
					repl = (string[i+2]-'0')*16
						+ string[i+3]-'0';
					len = 3;
				} else if (cisdigit(string[i+1])) {
					repl = (string[i+2]-'0')*8*8
						+ (string[i+3]-'0')*8
						+ (string[i+4]-'0');
					len = 3;
				}
			}
			if (len == 0) {
				repl = string[i+1];
				len = 1;
			}

			string[i] = repl;
			memmove(string + i + 1, string + i + len + 1,
				strlen(string + i + len + 1) + 1);
		}
	}

	return string;
}


static void read_config_header(void)
{
	char *fname = talloc_asprintf(NULL, "%s/config.h", ccan_dir);
	char **lines;
	unsigned int i;

	config_header = grab_file(NULL, fname, NULL);
	if (!config_header) {
		talloc_free(fname);
		return;
	}

	lines = strsplit(config_header, config_header, "\n");
	for (i = 0; i < talloc_array_length(lines) - 1; i++) {
		char *sym;
		const char **line = (const char **)&lines[i];

		if (!get_token(line, "#"))
			continue;
		if (!get_token(line, "define"))
			continue;
		sym = get_symbol_token(lines, line);
		if (streq(sym, "CCAN_COMPILER") && !compiler) {
			compiler = demangle_string(lines[i]);
			if (!compiler)
				errx(1, "%s:%u:could not parse CCAN_COMPILER",
				     fname, i+1);
			if (verbose > 1)
				printf("%s: compiler set to '%s'\n",
				       fname, compiler);
		} else if (streq(sym, "CCAN_CFLAGS") && !cflags) {
			cflags = demangle_string(lines[i]);
			if (!cflags)
				errx(1, "%s:%u:could not parse CCAN_CFLAGS",
				     fname, i+1);
			if (verbose > 1)
				printf("%s: compiler flags set to '%s'\n",
				       fname, cflags);
		}
	}
	if (!compiler)
		compiler = CCAN_COMPILER;
	if (!cflags)
		compiler = CCAN_CFLAGS;
}

static char *opt_set_const_charp(const char *arg, const char **p)
{
	return opt_set_charp(arg, cast_const2(char **, p));
}

static char *opt_set_target(const char *arg, struct ccanlint **t)
{
	*t = find_test(arg);
	if (!*t)
		return talloc_asprintf(NULL, "unknown --target %s", arg);
	return NULL;
}

static bool run_tests(struct ccanlint *target,
		      bool summary,
		      struct manifest *m,
		      const char *prefix)
{
	struct run_info run;

	run.quiet = summary;
	run.m = m;
	run.prefix = prefix;
	run.score = run.total = 0;
	run.pass = true;

	if (target) {
		dgraph_traverse_to(&target->node, run_test, &run);
		if (run.pass)
			run_test(&target->node, &run);
	} else
		dgraph_traverse_to(&all, run_test, &run);

	printf("%sTotal score: %u/%u\n",
	       prefix, run.score, run.total);

	return run.pass;
}

int main(int argc, char *argv[])
{
	bool summary = false, pass = true;
	unsigned int i;
	struct manifest *m;
	struct ccanlint *target = NULL;
	const char *prefix = "";
	char *dir = talloc_getcwd(NULL), *base_dir = dir, *testlink;
	
	cmdline_exclude = btree_new(btree_strcmp);
	info_exclude = btree_new(btree_strcmp);

	opt_register_early_noarg("--verbose|-v", opt_inc_intval, &verbose,
				 "verbose mode (up to -vvvv)");
	opt_register_noarg("-n|--safe-mode", opt_set_bool, &safe_mode,
			 "do not compile anything");
	opt_register_noarg("-l|--list-tests", list_tests, NULL,
			 "list tests ccanlint performs (and exit)");
	opt_register_noarg("--test-dep-graph", test_dependency_graph, NULL,
			 "print dependency graph of tests in Graphviz .dot format");
	opt_register_arg("-k|--keep <testname>", keep_test, NULL, NULL,
			 "keep results of <testname>"
			 " (can be used multiple times, or 'all')");
	opt_register_noarg("--summary|-s", opt_set_bool, &summary,
			   "simply give one line summary");
	opt_register_arg("-x|--exclude <testname>", skip_test, NULL, NULL,
			 "exclude <testname> (can be used multiple times)");
	opt_register_arg("-t|--timeout <milleseconds>", opt_set_uintval,
			 NULL, &timeout,
			 "ignore (terminate) tests that are slower than this");
	opt_register_arg("--target <testname>", opt_set_target,
			 NULL, &target,
			 "only run one test (and its prerequisites)");
	opt_register_arg("--compiler <compiler>", opt_set_const_charp,
			 NULL, &compiler, "set the compiler");
	opt_register_arg("--cflags <flags>", opt_set_const_charp,
			 NULL, &cflags, "set the compiler flags");
	opt_register_noarg("-?|-h|--help", opt_usage_and_exit,
			   "\nA program for checking and guiding development"
			   " of CCAN modules.",
			   "This usage message");

	/* Do verbose before anything else... */
	opt_early_parse(argc, argv, opt_log_stderr_exit);

	/* We move into temporary directory, so gcov dumps its files there. */
	if (chdir(temp_dir(talloc_autofree_context())) != 0)
		err(1, "Error changing to %s temporary dir", temp_dir(NULL));

	init_tests();

	if (verbose >= 3) {
		compile_verbose = true;
		print_test_depends();
	}
	if (verbose >= 4)
		tools_verbose = true;

	opt_parse(&argc, argv, opt_log_stderr_exit);

	/* This links back to the module's test dir. */
	testlink = talloc_asprintf(NULL, "%s/test", temp_dir(NULL));

	/* Defaults to pwd. */
	if (argc == 1) {
		i = 1;
		goto got_dir;
	}

	for (i = 1; i < argc; i++) {
		unsigned int score, total_score;

		dir = argv[i];

		if (dir[0] != '/')
			dir = talloc_asprintf_append(NULL, "%s/%s",
						     base_dir, dir);
		while (strends(dir, "/"))
			dir[strlen(dir)-1] = '\0';

	got_dir:
		if (dir != base_dir)
			prefix = talloc_append_string(talloc_basename(NULL,dir),
						      ": ");

		init_tests();

		m = get_manifest(talloc_autofree_context(), dir);

		/* FIXME: This has to come after we've got manifest. */
		if (i == 1)
			read_config_header();

		/* Create a symlink from temp dir back to src dir's
		 * test directory. */
		unlink(testlink);
		if (symlink(talloc_asprintf(m, "%s/test", dir), testlink) != 0)
			err(1, "Creating test symlink in %s", temp_dir(NULL));

		score = total_score = 0;
		if (!run_tests(target, summary, m, prefix))
			pass = false;
	}
	return pass ? 0 : 1;
}
