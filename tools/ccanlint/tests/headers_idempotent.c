#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/str/str.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <string.h>
#include <ctype.h>

static const char explain[] 
= "Headers usually start with the C preprocessor lines to prevent multiple\n"
  "inclusions.  These look like the following:\n"
  "#ifndef CCAN_<MODNAME>_H\n"
  "#define CCAN_<MODNAME>_H\n"
  "...\n"
  "#endif /* CCAN_<MODNAME>_H */\n";

static void fix_name(char *name)
{
	unsigned int i;

	for (i = 0; name[i]; i++) {
		if (cisalnum(name[i]))
			name[i] = toupper(name[i]);
		else
			name[i] = '_';
	}
}

static void handle_idem(struct manifest *m, struct score *score)
{
	struct file_error *e;

	list_for_each(&score->per_file_errors, e, list) {
		char *name, *q, *tmpname;
		FILE *out;
		unsigned int i;

		/* Main header gets CCAN_FOO_H, others CCAN_FOO_XXX_H */
		if (strstarts(e->file->name, m->basename)
		    || strlen(e->file->name) == strlen(m->basename) + 2)
			name = tal_fmt(score, "CCAN_%s_H", m->modname);
		else
			name = tal_fmt(score, "CCAN_%s_%s",
				       m->modname, e->file->name);
		fix_name(name);

		q = tal_fmt(score,
			    "Should I wrap %s in #ifndef/#define %s for you?",
			    e->file->name, name);
		if (!ask(q))
			continue;

		tmpname = temp_file(score, ".h", e->file->name);
		out = fopen(tmpname, "w");
		if (!out)
			err(1, "Opening %s", tmpname);
		if (fprintf(out, "#ifndef %s\n#define %s\n", name, name) < 0)
			err(1, "Writing %s", tmpname);

		for (i = 0; e->file->lines[i]; i++)
			if (fprintf(out, "%s\n", e->file->lines[i]) < 0)
				err(1, "Writing %s", tmpname);

		if (fprintf(out, "#endif /* %s */\n", name) < 0)
			err(1, "Writing %s", tmpname);
		
		if (fclose(out) != 0)
			err(1, "Closing %s", tmpname);

		if (!move_file(tmpname, e->file->fullname))
			err(1, "Moving %s to %s", tmpname, e->file->fullname);
	}
}

static void check_idem(struct ccan_file *f, struct score *score)
{
	struct line_info *line_info;
	unsigned int i, first_preproc_line;
	const char *line, *sym;

	line_info = get_ccan_line_info(f);
	if (tal_count(f->lines) < 4)
		/* FIXME: We assume small headers probably uninteresting. */
		return;

	for (i = 0; f->lines[i]; i++) {
		if (line_info[i].type == DOC_LINE
		    || line_info[i].type == COMMENT_LINE)
			continue;
		if (line_info[i].type == CODE_LINE) {
			score_file_error(score, f, i+1,
					 "Expect first non-comment line to be"
					 " #ifndef.");
			return;
		} else if (line_info[i].type == PREPROC_LINE)
			break;
	}

	/* No code at all?  Don't complain. */
	if (!f->lines[i])
		return;

	first_preproc_line = i;
	for (i = first_preproc_line+1; f->lines[i]; i++) {
		if (line_info[i].type == DOC_LINE
		    || line_info[i].type == COMMENT_LINE)
			continue;
		if (line_info[i].type == CODE_LINE) {
			score_file_error(score, f, i+1,
					 "Expect second non-comment line to be"
					 " #define.");
			return;
		} else if (line_info[i].type == PREPROC_LINE)
			break;
	}

	/* No code at all?  Weird. */
	if (!f->lines[i])
		return;

	/* We expect a condition around this line. */
	if (!line_info[i].cond) {
		score_file_error(score, f, first_preproc_line+1,
				 "Expected #ifndef");
		return;
	}

	line = f->lines[i];

	/* We expect the condition to be ! IFDEF <symbol>. */
	if (line_info[i].cond->type != PP_COND_IFDEF
	    || !line_info[i].cond->inverse) {
		score_file_error(score, f, first_preproc_line+1,
				 "Expected #ifndef");
		return;
	}

	/* And this to be #define <symbol> */
	if (!get_token(&line, "#"))
		abort();
	if (!get_token(&line, "define")) {
		score_file_error(score, f, i+1,
				 "expected '#define %s'",
				 line_info[i].cond->symbol);
		return;
	}
	sym = get_symbol_token(f, &line);
	if (!sym || !streq(sym, line_info[i].cond->symbol)) {
		score_file_error(score, f, i+1,
				 "expected '#define %s'",
				 line_info[i].cond->symbol);
		return;
	}

	/* Record this for use in depends_accurate */
	f->idempotent_cond = line_info[i].cond;

	/* Rest of code should all be covered by that conditional. */
	for (i++; f->lines[i]; i++) {
		unsigned int val = 0;
		if (line_info[i].type == DOC_LINE
		    || line_info[i].type == COMMENT_LINE)
			continue;
		if (get_ccan_line_pp(line_info[i].cond, sym, &val, NULL)
		    != NOT_COMPILED) {
			score_file_error(score, f, i+1, "code outside"
					 " idempotent region");
			return;
		}
	}
}

static void check_idempotent(struct manifest *m,
			     unsigned int *timeleft, struct score *score)
{
	struct ccan_file *f;

	/* We don't fail ccanlint for this. */
	score->pass = true;

	list_for_each(&m->h_files, f, list) {
		check_idem(f, score);
	}
	if (!score->error) {
		score->score = score->total;
	}
}

struct ccanlint headers_idempotent = {
	.key = "headers_idempotent",
	.name = "Module headers are #ifndef/#define wrapped",
	.check = check_idempotent,
	.handle = handle_idem,
	.needs = "info_exists main_header_exists"
};

REGISTER_TEST(headers_idempotent);
