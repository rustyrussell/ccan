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
unsigned int opt_count, opt_num_short, opt_num_short_arg, opt_num_long;
const char *opt_argv0;

/* Returns string after first '-'. */
static const char *first_name(const char *names, unsigned *len)
{
	*len = strcspn(names + 1, "/");
	return names + 1;
}

static const char *next_name(const char *names, unsigned *len)
{
	names += *len;
	if (!names[0])
		return NULL;
	return first_name(names + 1, len);
}

static const char *first_opt(unsigned *i, unsigned *len)
{
	for (*i = 0; *i < opt_count; (*i)++) {
		if (opt_table[*i].flags == OPT_SUBTABLE)
			continue;
		return first_name(opt_table[*i].names, len);
	}
	return NULL;
}

static const char *next_opt(const char *p, unsigned *i, unsigned *len)
{
	for (; *i < opt_count; (*i)++) {
		if (opt_table[*i].flags == OPT_SUBTABLE)
			continue;
		if (!p)
			return first_name(opt_table[*i].names, len);
		p = next_name(p, len);
		if (p)
			return p;
	}
	return NULL;
}

static const char *first_lopt(unsigned *i, unsigned *len)
{
	const char *p;
	for (p = first_opt(i, len); p; p = next_opt(p, i, len)) {
		if (p[0] == '-') {
			/* Skip leading "-" */
			(*len)--;
			p++;
			break;
		}
	}
	return p;
}

static const char *next_lopt(const char *p, unsigned *i, unsigned *len)
{
	for (p = next_opt(p, i, len); p; p = next_opt(p, i, len)) {
		if (p[0] == '-') {
			/* Skip leading "-" */
			(*len)--;
			p++;
			break;
		}
	}
	return p;
}

const char *first_sopt(unsigned *i)
{
	const char *p;
	unsigned int len;

	for (p = first_opt(i, &len); p; p = next_opt(p, i, &len)) {
		if (p[0] != '-')
			break;
	}
	return p;
}

const char *next_sopt(const char *p, unsigned *i)
{
	unsigned int len = 1;
	for (p = next_opt(p, i, &len); p; p = next_opt(p, i, &len)) {
		if (p[0] != '-')
			break;
	}
	return p;
}

static void check_opt(const struct opt_table *entry)
{
	const char *p;
	unsigned len;

	assert(entry->flags == OPT_HASARG || entry->flags == OPT_NOARG);

	assert(entry->names[0] == '-');
	for (p = first_name(entry->names, &len); p; p = next_name(p, &len)) {
		if (*p == '-') {
			assert(len > 1);
			opt_num_long++;
		} else {
			assert(len == 1);
			assert(*p != ':');
			opt_num_short++;
			if (entry->flags == OPT_HASARG) {
				opt_num_short_arg++;
				/* FIXME: -? with ops breaks getopt_long */
				assert(*p != '?');
			}
		}
	}
}

static void add_opt(const struct opt_table *entry)
{
	opt_table = realloc(opt_table, sizeof(opt_table[0]) * (opt_count+1));
	opt_table[opt_count++] = *entry;
}

void _opt_register(const char *names, enum opt_flags flags,
		   char *(*cb)(void *arg),
		   char *(*cb_arg)(const char *optarg, void *arg),
		   void (*show)(char buf[OPT_SHOW_LEN], const void *arg),
		   void *arg, const char *desc)
{
	struct opt_table opt;
	opt.names = names;
	opt.flags = flags;
	opt.cb = cb;
	opt.cb_arg = cb_arg;
	opt.show = show;
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
	char *str = malloc(1 + opt_num_short + opt_num_short_arg + 1);
	const char *p;
	unsigned int i, num = 0;

	/* This tells getopt_long we want a ':' returned for missing arg. */
	str[num++] = ':';
	for (p = first_sopt(&i); p; p = next_sopt(p, &i)) {
		str[num++] = *p;
		if (opt_table[i].flags == OPT_HASARG)
			str[num++] = ':';
	}
	str[num++] = '\0';
	assert(num == 1 + opt_num_short + opt_num_short_arg + 1);
	return str;
}

static struct option *make_options(void)
{
	struct option *options = malloc(sizeof(*options) * (opt_num_long + 1));
	unsigned int i, num = 0, len;
	const char *p;

	for (p = first_lopt(&i, &len); p; p = next_lopt(p, &i, &len)) {
		char *buf = malloc(len + 1);
		memcpy(buf, p, len);
		buf[len] = 0;
		options[num].name = buf;
		options[num].has_arg = (opt_table[i].flags == OPT_HASARG);
		options[num].flag = NULL;
		options[num].val = 0;
		num++;
	}
	memset(&options[num], 0, sizeof(options[num]));
	assert(num == opt_num_long);
	return options;
}

static struct opt_table *find_short(char shortopt)
{
	unsigned int i;
	const char *p;

	for (p = first_sopt(&i); p; p = next_sopt(p, &i)) {
		if (*p == shortopt)
			return &opt_table[i];
	}
	abort();
}

/* We want the index'th long entry. */
static struct opt_table *find_long(int index, const char **name)
{
	unsigned int i, len;
	const char *p;

	for (p = first_lopt(&i, &len); p; p = next_lopt(p, &i, &len)) {
		if (index == 0) {
			*name = p;
			return &opt_table[i];
		}
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
		errlog("%s: --%.*s: %s", opt_argv0,
		       strcspn(longopt, "/"), longopt, problem);
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
	optind = 0;
	while ((ret = getopt_long(*argc, argv, optstring, options, &longidx))
	       != -1) {
		char *problem;
		const char *name;

		/* optopt is 0 if it's an unknown long option, *or* if
		 * -? is a valid short option. */
		if (ret == '?') {
			if (optopt || strncmp(argv[optind-1], "--", 2) == 0) {
				parse_fail(errlog, optopt, argv[optind-1]+2,
					   "unrecognized option");
				break;
			}
		} else if (ret == ':') {
			/* Missing argument: longidx not updated :( */
			parse_fail(errlog, optopt, argv[optind-1]+2,
				   "option requires an argument");
			break;
		}

		if (ret != 0)
			e = find_short(ret);
		else
			e = find_long(longidx, &name);

		if (e->flags == OPT_HASARG)
			problem = e->cb_arg(optarg, e->arg);
		else
			problem = e->cb(e->arg);

		if (problem) {
			parse_fail(errlog, ret, name, problem);
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
