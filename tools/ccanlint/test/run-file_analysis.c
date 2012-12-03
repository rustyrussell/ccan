#include "tools/ccanlint/ccanlint.h"
#include "ccan/tap/tap.h"
#include "tools/ccanlint/file_analysis.c"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>

/* This is our test file. */
struct test {
	enum line_info_type type;
	bool continued;
	const char *line;
};

static struct test testfile[] = {
	{ PREPROC_LINE,	false, "#ifndef TEST_H" },
	{ PREPROC_LINE,	false, "#define TEST_H" },
	{ DOC_LINE,	false, "/**" },
	{ DOC_LINE,	false, " * Comment here." },
	{ DOC_LINE,	false, " * Comment here too." },
	{ DOC_LINE,	false, " */" },
	{ COMMENT_LINE,	false, "// Normal one-line comment" },
	{ COMMENT_LINE,	false, "  // Spaced one-line comment" },
	{ COMMENT_LINE,	false, "/* Normal one-line comment */" },
	{ COMMENT_LINE,	false, "  /* Spaced one-line comment */" },
	{ COMMENT_LINE,	false, "  /* Spaced two-line comment" },
	{ COMMENT_LINE,	false, "  continued comment */" },
	{ CODE_LINE,	false, "extern int x;"},
	{ CODE_LINE,	false, "extern int y; // With new-style comment"},
	{ CODE_LINE,	false, "extern int z; /* With old-style comment */"},
	{ CODE_LINE,	false, "extern int v; /* With two-line comment"},
	{ COMMENT_LINE,	false, "		 Second line of comment"},
	{ COMMENT_LINE, false, "/* comment1  */ // comment 2"},
	{ COMMENT_LINE, false, "/* comment1  */ /* comment 2 */ "},
	{ CODE_LINE,	false, "/* comment1  */ code; /* comment 2 */ "},
	{ CODE_LINE,	false, "/* comment1  */ code; // comment 2"},
	{ COMMENT_LINE,	false, "/* comment start  \\"},
	{ COMMENT_LINE,	true,  "   comment finish */"},
	{ PREPROC_LINE,	false, "#define foo \\"},
	{ PREPROC_LINE,	true,  "	(bar + \\"},
	{ PREPROC_LINE,	true,  "	 baz)"},
	{ CODE_LINE,	false, "extern int \\"},
	{ CODE_LINE,	true,  "#x;"},

	/* Variants of the same thing. */
	{ PREPROC_LINE,	false, "#ifdef BAR"},
	{ CODE_LINE,	false, "BAR"},
	{ PREPROC_LINE,	false, "#else"},
	{ CODE_LINE,	false, "!BAR"},
	{ PREPROC_LINE,	false, "#endif"},

	{ PREPROC_LINE,	false, "#if defined BAR"},
	{ CODE_LINE,	false, "BAR"},
	{ PREPROC_LINE,	false, "#else"},
	{ CODE_LINE,	false, "!BAR"},
	{ PREPROC_LINE,	false, "#endif"},

	{ PREPROC_LINE,	false, "#if defined(BAR)"},
	{ CODE_LINE,	false, "BAR"},
	{ PREPROC_LINE,	false, "#else"},
	{ CODE_LINE,	false, "!BAR"},
	{ PREPROC_LINE,	false, "#endif"},

	{ PREPROC_LINE,	false, "#if !defined(BAR)"},
	{ CODE_LINE,	false, "!BAR"},
	{ PREPROC_LINE,	false, "#else"},
	{ CODE_LINE,	false, "BAR"},
	{ PREPROC_LINE,	false, "#endif"},

	{ PREPROC_LINE,	false, "#if HAVE_FOO"},
	{ CODE_LINE,	false, "HAVE_FOO"},
	{ PREPROC_LINE,	false, "#elif HAVE_BAR"},
	{ CODE_LINE,	false, "HAVE_BAR"},
	{ PREPROC_LINE,	false, "#else"},
	{ CODE_LINE,	false, "neither"},
	{ PREPROC_LINE,	false, "#endif /* With a comment. */"},

	{ PREPROC_LINE,	false, "#endif /* TEST_H */" },
};

#define NUM_LINES (sizeof(testfile)/sizeof(testfile[0]))

static const char *line_type_name(enum line_info_type type)
{
	switch (type) {
	case PREPROC_LINE: return "PREPROC_LINE";
	case CODE_LINE: return "CODE_LINE";
	case DOC_LINE: return "DOC_LINE";
	case COMMENT_LINE: return "COMMENT_LINE";
	default: return "**INVALID**";
	}
}

/* This just tests parser for the moment. */
int main(int argc, char *argv[])
{
	unsigned int i;
	struct line_info *line_info;
	struct ccan_file *f = tal(NULL, struct ccan_file);

	plan_tests(NUM_LINES * 2 + 2 + 86);

	f->num_lines = NUM_LINES;
	f->line_info = NULL;
	f->lines = tal_array(f, char *, f->num_lines);
	for (i = 0; i < f->num_lines; i++)
		f->lines[i] = tal_strdup(f->lines, testfile[i].line);
	
	line_info = get_ccan_line_info(f);
	ok1(line_info == f->line_info);
	for (i = 0; i < f->num_lines; i++) {
		ok(f->line_info[i].type == testfile[i].type,
		   "Line %u:'%s' type %s should be %s",
		   i, testfile[i].line,
		   line_type_name(f->line_info[i].type),
		   line_type_name(testfile[i].type));
		ok(f->line_info[i].continued == testfile[i].continued,
		   "Line %u:'%s' continued should be %s",
		   i, testfile[i].line,
		   testfile[i].continued ? "TRUE" : "FALSE");
	}

	/* Should cache. */
	ok1(get_ccan_line_info(f) == line_info);

	/* Expect line 1 condition to be NULL. */
	ok1(line_info[0].cond == NULL);
	/* Line 2, should depend on TEST_H being undefined. */
	ok1(line_info[1].cond != NULL);
	ok1(line_info[1].cond->type == PP_COND_IFDEF);
	ok1(line_info[1].cond->inverse);
	ok1(line_info[1].cond->parent == NULL);
	ok1(streq(line_info[1].cond->symbol, "TEST_H"));

	/* Every line BAR should depend on BAR being defined. */
	for (i = 0; i < f->num_lines; i++) {
		if (!streq(testfile[i].line, "BAR"))
			continue;
		ok1(line_info[i].cond->type == PP_COND_IFDEF);
		ok1(!line_info[i].cond->inverse);
		ok1(streq(line_info[i].cond->symbol, "BAR"));
		ok1(line_info[i].cond->parent == line_info[1].cond);
	}

	/* Every line !BAR should depend on BAR being undefined. */
	for (i = 0; i < f->num_lines; i++) {
		if (!streq(testfile[i].line, "!BAR"))
			continue;
		ok1(line_info[i].cond->type == PP_COND_IFDEF);
		ok1(line_info[i].cond->inverse);
		ok1(streq(line_info[i].cond->symbol, "BAR"));
		ok1(line_info[i].cond->parent == line_info[1].cond);
	}
	
	/* Every line HAVE_BAR should depend on HAVE_BAR being set. */
	for (i = 0; i < f->num_lines; i++) {
		if (!streq(testfile[i].line, "HAVE_BAR"))
			continue;
		ok1(line_info[i].cond->type == PP_COND_IF);
		ok1(!line_info[i].cond->inverse);
		ok1(streq(line_info[i].cond->symbol, "HAVE_BAR"));
		ok1(line_info[i].cond->parent == line_info[1].cond);
	}
	
	/* Every line HAVE_FOO should depend on HAVE_FOO being set. */
	for (i = 0; i < f->num_lines; i++) {
		if (!streq(testfile[i].line, "HAVE_FOO"))
			continue;
		ok1(line_info[i].cond->type == PP_COND_IF);
		ok1(!line_info[i].cond->inverse);
		ok1(streq(line_info[i].cond->symbol, "HAVE_FOO"));
		ok1(line_info[i].cond->parent == line_info[1].cond);
	}

	/* Now check using interface. */
	for (i = 0; i < f->num_lines; i++) {
		unsigned int val = 1;
		if (streq(testfile[i].line, "BAR")) {
			/* If we don't know if the TEST_H was undefined,
			 * best we get is a MAYBE. */
			ok1(get_ccan_line_pp(line_info[i].cond, "BAR", &val,
					     NULL) == MAYBE_COMPILED);
			ok1(get_ccan_line_pp(line_info[i].cond, "BAR", NULL,
					     NULL) == NOT_COMPILED);
			ok1(get_ccan_line_pp(line_info[i].cond, "BAR", &val,
					     "TEST_H", NULL,
					     NULL) == COMPILED);
			ok1(get_ccan_line_pp(line_info[i].cond, "BAR", NULL,
					     "TEST_H", NULL,
					     NULL) == NOT_COMPILED);
		} else if (streq(testfile[i].line, "!BAR")) {
			ok1(get_ccan_line_pp(line_info[i].cond, "BAR", &val,
					     NULL) == NOT_COMPILED);
			ok1(get_ccan_line_pp(line_info[i].cond, "BAR", NULL,
					     NULL) == MAYBE_COMPILED);
			ok1(get_ccan_line_pp(line_info[i].cond, "BAR", &val,
					     "TEST_H", NULL,
					     NULL) == NOT_COMPILED);
			ok1(get_ccan_line_pp(line_info[i].cond, "BAR", NULL,
					     "TEST_H", NULL,
					     NULL) == COMPILED);
		} else if (streq(testfile[i].line, "HAVE_BAR")) {
			ok1(get_ccan_line_pp(line_info[i].cond, "HAVE_BAR",
					     &val, NULL) == MAYBE_COMPILED);
			ok1(get_ccan_line_pp(line_info[i].cond, "HAVE_BAR",
					     &val, "TEST_H", NULL,
					     NULL) == COMPILED);
			val = 0;
			ok1(get_ccan_line_pp(line_info[i].cond, "HAVE_BAR",
					     &val, NULL) == NOT_COMPILED);
			ok1(get_ccan_line_pp(line_info[i].cond, "HAVE_BAR",
					     &val, "TEST_H", NULL,
					     NULL) == NOT_COMPILED);
		} else if (streq(testfile[i].line, "HAVE_FOO")) {
			ok1(get_ccan_line_pp(line_info[i].cond, "HAVE_FOO",
					     &val, NULL) == MAYBE_COMPILED);
			ok1(get_ccan_line_pp(line_info[i].cond, "HAVE_FOO",
					     &val, "TEST_H", NULL,
					     NULL) == COMPILED);
			val = 0;
			ok1(get_ccan_line_pp(line_info[i].cond, "HAVE_FOO",
					     &val, NULL) == NOT_COMPILED);
			ok1(get_ccan_line_pp(line_info[i].cond, "HAVE_FOO",
					     &val, "TEST_H", NULL,
					     NULL) == NOT_COMPILED);
		}
	}

	return exit_status();
}
