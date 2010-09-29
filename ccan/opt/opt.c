#include <ccan/opt/opt.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include "private.h"

struct opt_table *opt_table;
unsigned int opt_count;
const char *opt_argv0;

static void check_opt(const struct opt_table *entry)
{
	assert(entry->flags == OPT_HASARG || entry->flags == OPT_NOARG);
	assert(entry->shortopt || entry->longopt);
	assert(entry->shortopt != ':');
	assert(entry->shortopt != '?' || entry->flags == OPT_NOARG);
}

static void add_opt(const struct opt_table *entry)
{
	opt_table = realloc(opt_table, sizeof(opt_table[0]) * (opt_count+1));
	opt_table[opt_count++] = *entry;
}

void _opt_register(const char *longopt, char shortopt, enum opt_flags flags,
		   char *(*cb)(void *arg),
		   char *(*cb_arg)(const char *optarg, void *arg),
		   void *arg, const char *desc)
{
	struct opt_table opt;
	opt.longopt = longopt;
	opt.shortopt = shortopt;
	opt.flags = flags;
	opt.cb = cb;
	opt.cb_arg = cb_arg;
	opt.arg = arg;
	opt.desc = desc;
	check_opt(&opt);
	add_opt(&opt);
}

void opt_register_table(const struct opt_table entry[], const char *desc)
{
	unsigned int i, start = opt_count;

	if (desc) {
		struct opt_table heading = OPT_SUBTABLE(NULL, desc);
		add_opt(&heading);
	}
	for (i = 0; entry[i].flags != OPT_END; i++) {
		if (entry[i].flags == OPT_SUBTABLE)
			opt_register_table(subtable_of(&entry[i]),
					   entry[i].desc);
		else {
			check_opt(&entry[i]);
			add_opt(&entry[i]);
		}
	}
	/* We store the table length in arg ptr. */
	if (desc)
		opt_table[start].arg = (void *)(intptr_t)(opt_count - start);
}

static char *make_optstring(void)
{
	/* Worst case, each one is ":x:", plus nul term. */
	char *str = malloc(1 + opt_count * 2 + 1);
	unsigned int num, i;

	/* This tells getopt_long we want a ':' returned for missing arg. */
	str[0] = ':';
	num = 1;
	for (i = 0; i < opt_count; i++) {
		if (!opt_table[i].shortopt)
			continue;
		str[num++] = opt_table[i].shortopt;
		if (opt_table[i].flags == OPT_HASARG)
			str[num++] = ':';
	}
	str[num] = '\0';
	return str;
}

static struct option *make_options(void)
{
	struct option *options = malloc(sizeof(*options) * (opt_count + 1));
	unsigned int i, num;

	for (num = i = 0; i < opt_count; i++) {
		if (!opt_table[i].longopt)
			continue;
		options[num].name = opt_table[i].longopt;
		options[num].has_arg = (opt_table[i].flags == OPT_HASARG);
		options[num].flag = NULL;
		options[num].val = 0;
		num++;
	}
	memset(&options[num], 0, sizeof(options[num]));
	return options;
}

static struct opt_table *find_short(char shortopt)
{
	unsigned int i;
	for (i = 0; i < opt_count; i++) {
		if (opt_table[i].shortopt == shortopt)
			return &opt_table[i];
	}
	abort();
}

/* We want the index'th long entry. */
static struct opt_table *find_long(int index)
{
	unsigned int i;
	for (i = 0; i < opt_count; i++) {
		if (!opt_table[i].longopt)
			continue;
		if (index == 0)
			return &opt_table[i];
		index--;
	}
	abort();
}

/* glibc does this as:
/tmp/opt-example: invalid option -- 'x'
/tmp/opt-example: unrecognized option '--long'
/tmp/opt-example: option '--someflag' doesn't allow an argument
/tmp/opt-example: option '--s' is ambiguous
/tmp/opt-example: option requires an argument -- 's'
*/
static void parse_fail(void (*errlog)(const char *fmt, ...),
		       char shortopt, const char *longopt, const char *problem)
{
	if (shortopt)
		errlog("%s: -%c: %s", opt_argv0, shortopt, problem);
	else
		errlog("%s: --%s: %s", opt_argv0, longopt, problem);
}

void dump_optstate(void);
void dump_optstate(void)
{
	printf("opterr = %i, optind = %i, optopt = %i, optarg = %s\n",
	       opterr, optind, optopt, optarg);
}

/* Parse your arguments. */
bool opt_parse(int *argc, char *argv[], void (*errlog)(const char *fmt, ...))
{
	char *optstring = make_optstring();
	struct option *options = make_options();
	int ret, longidx = 0;
	struct opt_table *e;

	/* We will do our own error reporting. */
	opterr = 0;
	opt_argv0 = argv[0];

	/* Reset in case we're called more than once. */
	optopt = 0;
	optind = 1;
	while ((ret = getopt_long(*argc, argv, optstring, options, &longidx))
	       != -1) {
		char *problem;
		bool missing = false;

		/* optopt is 0 if it's an unknown long option, *or* if
		 * -? is a valid short option. */
		if (ret == '?') {
			if (optopt || strncmp(argv[optind-1], "--", 2) == 0) {
				parse_fail(errlog, optopt, argv[optind-1]+2,
					   "unrecognized option");
				break;
			}
		} else if (ret == ':') {
			missing = true;
			ret = optopt;
		}

		if (ret != 0)
			e = find_short(ret);
		else
			e = find_long(longidx);

		/* Missing argument */
		if (missing) {
			parse_fail(errlog, e->shortopt, e->longopt,
				   "option requires an argument");
			break;
		}

		if (e->flags == OPT_HASARG)
			problem = e->cb_arg(optarg, e->arg);
		else
			problem = e->cb(e->arg);

		if (problem) {
			parse_fail(errlog, e->shortopt, e->longopt,
				   problem);
			free(problem);
			break;
		}
	}
	free(optstring);
	free(options);
	if (ret != -1)
		return false;

	/* We hide everything but remaining arguments. */
	memmove(&argv[1], &argv[optind], sizeof(argv[1]) * (*argc-optind+1));
	*argc -= optind - 1;

	return ret == -1 ? true : false;
}

void opt_log_stderr(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

char *opt_invalid_argument(const char *arg)
{
	char *str = malloc(sizeof("Invalid argument '%s'") + strlen(arg));
	sprintf(str, "Invalid argument '%s'", arg);
	return str;
}
