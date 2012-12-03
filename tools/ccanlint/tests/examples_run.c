#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/foreach/foreach.h>
#include <ccan/str/str.h>
#include <ccan/cast/cast.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>

static const char *can_run(struct manifest *m)
{
	struct list_head *list;

	if (safe_mode)
		return "Safe mode enabled";
	foreach_ptr(list, &m->examples, &m->mangled_examples)
		if (!list_empty(list))
			return NULL;
	return "No examples";
}

/* Very dumb scanner, allocates %s-strings. */
static bool scan_forv(const void *ctx,
		      const char *input, const char *fmt, va_list *args)
{
	va_list ap;
	bool ret;

	if (input[0] == '\0' || fmt[0] == '\0')
		return input[0] == fmt[0];

	va_copy(ap, *args);

	if (cisspace(fmt[0])) {
		/* One format space can swallow many input spaces */
		ret = false;
		while (cisspace(input[0])) {
			if (scan_forv(ctx, ++input, fmt+1, &ap)) {
				ret = true;
				break;
			}
		}
	} else if (fmt[0] != '%') {
		if (toupper(input[0]) != toupper(fmt[0]))
			ret = false;
		else
			ret = scan_forv(ctx, input+1, fmt+1, &ap);
	} else {
		char **p = va_arg(ap, char **);
		unsigned int len;

		ret = false;
		assert(fmt[1] == 's');
		for (len = 1; input[len-1]; len++) {
			ret = scan_forv(ctx, input + len, fmt+2, &ap);
			if (ret) {
				*p = tal_strndup(ctx, input, len);
				ret = true;
				break;
			}
		}
	}
	va_end(ap);
	return ret;
}

static bool scan_for(const void *ctx, const char *input, const char *fmt, ...)
{
	bool ret;
	va_list ap;

	va_start(ap, fmt);
	ret = scan_forv(ctx, input, fmt, &ap);
	va_end(ap);
	return ret;
}

static char *find_expect(struct ccan_file *file,
			 char **lines, char **input, bool *exact,
			 unsigned *line)
{
	char *expect;
	const char *fmt;

	for (; lines[*line]; (*line)++) {
		char *p = lines[*line] + strspn(lines[*line], " \t");
		if (!strstarts(p, "//"))
			continue;
		p += strspn(p, "/ ");
		foreach_ptr(fmt,
			    "given '%s', outputs '%s'",
			    "given '%s' outputs '%s'",
			    "given \"%s\", outputs \"%s\"",
			    "given \"%s\" outputs \"%s\"") {
			if (scan_for(file, p, fmt, input, &expect)) {
				*exact = true;
				return expect;
			}
		}

		foreach_ptr(fmt,
			    "given '%s', output contains '%s'",
			    "given '%s' output contains '%s'",
			    "given \"%s\", output contains \"%s\"",
			    "given \"%s\" output contains \"%s\"") {
			if (scan_for(file, p, fmt, input, &expect)) {
				*exact = false;
				return expect;
			}
		}

		foreach_ptr(fmt, "outputs '%s'", "outputs \"%s\"") {
			if (scan_for(file, p, fmt, &expect)) {
				*input = cast_const(char *, "");
				*exact = true;
				return expect;
			}
		}

		foreach_ptr(fmt,
			    "given '%s', output contains '%s'",
			    "given '%s' output contains '%s'",
			    "given \"%s\", output contains \"%s\"",
			    "given \"%s\" output contains \"%s\"") {
			if (scan_for(file, p, fmt, input, &expect)) {
				*exact = false;
				return expect;
			}
		}

		/* Unquoted versions... we can get this wrong! */
		foreach_ptr(fmt,
			    "given %s, outputs '%s'",
			    "given '%s', outputs %s",
			    "given %s, outputs \"%s\"",
			    "given \"%s\", outputs %s",
			    "given %s, outputs %s",
			    "given %s outputs '%s'",
			    "given '%s' outputs %s",
			    "given %s outputs \"%s\"",
			    "given \"%s\" outputs %s",
			    "given %s outputs %s") {
			if (scan_for(file, p, fmt, input, &expect)) {
				*exact = true;
				return expect;
			}
		}

		foreach_ptr(fmt,
			    "given %s, output contains '%s'",
			    "given '%s', output contains %s",
			    "given %s, output contains \"%s\"",
			    "given \"%s\", output contains %s",
			    "given %s, output contains %s",
			    "given %s output contains '%s'",
			    "given '%s' output contains %s",
			    "given %s output contains \"%s\"",
			    "given \"%s\" output contains %s",
			    "given %s output contains %s") {
			if (scan_for(file, p, fmt, input, &expect)) {
				*exact = false;
				return expect;
			}
		}

		foreach_ptr(fmt,
			    "outputs '%s'",
			    "outputs \"%s\"",
			    "outputs %s") {
			if (scan_for(file, p, fmt, &expect)) {
				*input = cast_const(char *, "");
				*exact = true;
				return expect;
			}
		}

		foreach_ptr(fmt,
			    "output contains '%s'",
			    "output contains \"%s\"",
			    "output contains %s") {
			if (scan_for(file, p, fmt, &expect)) {
				*input = cast_const(char *, "");
				*exact = false;
				return expect;
			}
		}
	}		
	return NULL;
}

static char *trim(char *string)
{
	while (strends(string, "\n"))
	       string[strlen(string)-1] = '\0';
	return string;
}

static char *unexpected(struct ccan_file *i, const char *input,
			const char *expect, bool exact)
{
	char *output, *cmd;
	bool ok;
	unsigned int default_time = default_timeout_ms;

	cmd = tal_fmt(i, "echo '%s' | %s %s",
		      input, i->compiled[COMPILE_NORMAL], input);

	output = run_with_timeout(i, cmd, &ok, &default_time);
	if (!ok)
		return tal_fmt(i, "Exited with non-zero status\n");

	if (exact) {
		if (streq(output, expect) || streq(trim(output), expect))
			return NULL;
	} else {
		if (strstr(output, expect))
			return NULL;
	}
	return output;
}

static void run_examples(struct manifest *m,
			 unsigned int *timeleft, struct score *score)
{
	struct ccan_file *i;
	struct list_head *list;

	score->total = 0;
	score->pass = true;

	foreach_ptr(list, &m->examples, &m->mangled_examples) {
		list_for_each(list, i, list) {
			char **lines, *expect, *input, *output;
			unsigned int linenum = 0;
			bool exact;

			lines = get_ccan_file_lines(i);

			for (expect = find_expect(i, lines, &input, &exact,
						  &linenum);
			     expect;
			     linenum++,
				     expect = find_expect(i, lines, &input,
							  &exact, &linenum)) {
				if (i->compiled[COMPILE_NORMAL] == NULL)
					continue;

				score->total++;
				output = unexpected(i, input, expect, exact);
				if (!output) {
					score->score++;
					continue;
				}
				score_file_error(score, i, linenum+1,
						 "output '%s' didn't %s '%s'\n",
						 output,
						 exact ? "match" : "contain",
						 expect);
				score->pass = false;
			}
		}
	}
}

/* FIXME: Test with reduced features, valgrind! */
struct ccanlint examples_run = {
	.key = "examples_run",
	.name = "Module examples with expected output give that output",
	.check = run_examples,
	.can_run = can_run,
	.needs = "examples_compile"
};

REGISTER_TEST(examples_run);
