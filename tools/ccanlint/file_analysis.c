#include "config.h"
#include "ccanlint.h"
#include <ccan/str/str.h>
#include <ccan/take/take.h>
#include <ccan/hash/hash.h>
#include <ccan/htable/htable_type.h>
#include <ccan/noerr/noerr.h>
#include <ccan/foreach/foreach.h>
#include <ccan/asort/asort.h>
#include <ccan/array_size/array_size.h>
#include "../tools.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>

struct list_head *get_ccan_file_docs(struct ccan_file *f)
{
	if (!f->doc_sections) {
		get_ccan_file_lines(f);
		f->doc_sections = extract_doc_sections(f->lines, f->name);
	}
	return f->doc_sections;
}


/**
 * remove_comments - strip comments from a line, return copy.
 * @line: line to copy
 * @in_comment: are we already within a comment (from prev line).
 * @unterminated: are we still in a comment for next line.
 */
static char *remove_comments(const tal_t *ctx,
			     const char *line, bool in_comment,
			     bool *unterminated)
{
	char *p, *ret = tal_arr(ctx, char, strlen(line) + 1);

	p = ret;
	for (;;) {
		if (!in_comment) {
			/* Find first comment. */
			const char *old_comment = strstr(line, "/*");
			const char *new_comment = strstr(line, "//");
			const char *comment;

			if (new_comment && old_comment)
				comment = new_comment < old_comment
					? new_comment : old_comment;
			else if (old_comment)
				comment = old_comment;
			else if (new_comment)
				comment = new_comment;
			else {
				/* Nothing more. */
				strcpy(p, line);
				*unterminated = false;
				break;
			}

			/* Copy up to comment. */
			memcpy(p, line, comment - line);
			p += comment - line;
			line += comment - line + 2;

			if (comment == new_comment) {
				/* We're done: goes to EOL. */
				p[0] = '\0';
				*unterminated = false;
				break;
			}
			in_comment = true;
		}

		if (in_comment) {
			const char *end = strstr(line, "*/");
			if (!end) {
				*unterminated = true;
				p[0] = '\0';
				break;
			}
			line = end+2;
			in_comment = false;
		}
	}
	return ret;
}

static bool is_empty(const char *line)
{
	return strspn(line, " \r\t") == strlen(line);
}

static bool continues(const char *line)
{
	/* Technically, any odd number of these.  But who cares? */
	return strends(line, "\\");
}

static bool parse_hash_if(struct pp_conditions *cond, const char **line)
{
	bool brackets, defined;

	cond->inverse = get_token(line, "!");
	defined = get_token(line, "defined");
	brackets = get_token(line, "(");
	cond->symbol = get_symbol_token(cond, line);
	if (!cond->symbol)
		return false;
	if (brackets && !get_token(line, ")"))
		return false;
	if (!defined)
		cond->type = PP_COND_IF;

	/* FIXME: We just chain them, ignoring operators. */
	if (get_token(line, "||") || get_token(line, "&&")) {
		struct pp_conditions *sub = tal(cond, struct pp_conditions);

		sub->parent = cond->parent;
		sub->type = PP_COND_IFDEF;
		if (parse_hash_if(sub, line))
			cond->parent = sub;
	}

	return true;
}

/* FIXME: Get serious! */
static struct pp_conditions *analyze_directive(struct ccan_file *f,
					       const char *line,
					       struct pp_conditions *parent)
{
	struct pp_conditions *cond = tal(f, struct pp_conditions);
	bool unused;

	line = remove_comments(f, line, false, &unused);

	cond->parent = parent;
	cond->type = PP_COND_IFDEF;

	if (!get_token(&line, "#"))
		abort();

	if (get_token(&line, "if")) {
		if (!parse_hash_if(cond, &line))
			goto unknown;
	} else if (get_token(&line, "elif")) {
		/* Malformed? */
		if (!parent)
			return NULL;
		cond->parent = parent->parent;
		/* FIXME: Not quite true.  This implies !parent, but we don't
		 * do multiple conditionals yet. */
		if (!parse_hash_if(cond, &line))
			goto unknown;
	} else if (get_token(&line, "ifdef")) {
		bool brackets;
		cond->inverse = false;
		brackets = get_token(&line, "(");
		cond->symbol = get_symbol_token(cond, &line);
		if (!cond->symbol)
			goto unknown;
		if (brackets && !get_token(&line, ")"))
			goto unknown;
	} else if (get_token(&line, "ifndef")) {
		bool brackets;
		cond->inverse = true;
		brackets = get_token(&line, "(");
		cond->symbol = get_symbol_token(cond, &line);
		if (!cond->symbol)
			goto unknown;
		if (brackets && !get_token(&line, ")"))
			goto unknown;
	} else if (get_token(&line, "else")) {
		/* Malformed? */
		if (!parent)
			return NULL;

		*cond = *parent;
		cond->inverse = !cond->inverse;
		return cond;
	} else if (get_token(&line, "endif")) {
		tal_free(cond);
		/* Malformed? */
		if (!parent)
			return NULL;
		/* Back up one! */
		return parent->parent;
	} else {
		/* Not a conditional. */
		tal_free(cond);
		return parent;
	}

	if (!is_empty(line))
		goto unknown;
	return cond;

unknown:
	cond->type = PP_COND_UNKNOWN;
	return cond;
}

/* This parser is rough, but OK if code is reasonably neat. */
struct line_info *get_ccan_line_info(struct ccan_file *f)
{
	bool continued = false, in_comment = false;
	struct pp_conditions *cond = NULL;
	unsigned int i;

	if (f->line_info)
		return f->line_info;

	get_ccan_file_lines(f);
	f->line_info = tal_arr(f->lines, struct line_info,
			       tal_count(f->lines)-1);

	for (i = 0; f->lines[i]; continued = continues(f->lines[i++])) {
		char *p;
		bool still_doc_line;

		/* Current conditions apply to this line. */
		f->line_info[i].cond = cond;
		f->line_info[i].continued = continued;

		if (continued) {
			/* Same as last line. */
			f->line_info[i].type = f->line_info[i-1].type;
			/* Update in_comment. */
			remove_comments(f, f->lines[i], in_comment, &in_comment);
			continue;
		}

		/* Preprocessor directive? */
		if (!in_comment
		    && f->lines[i][strspn(f->lines[i], " \t")] == '#') {
			f->line_info[i].type = PREPROC_LINE;
			cond = analyze_directive(f, f->lines[i], cond);
			continue;
		}

		still_doc_line = (in_comment
				  && f->line_info[i-1].type == DOC_LINE);

		p = remove_comments(f, f->lines[i], in_comment, &in_comment);
		if (is_empty(p)) {
			if (strstarts(f->lines[i], "/**") || still_doc_line)
				f->line_info[i].type = DOC_LINE;
			else
				f->line_info[i].type = COMMENT_LINE;
		} else
			f->line_info[i].type = CODE_LINE;
		tal_free(p);
	}
	return f->line_info;
}

struct symbol {
	struct list_node list;
	const char *name;
	const unsigned int *value;
};

static struct symbol *find_symbol(struct list_head *syms, const char *sym)
{
	struct symbol *i;

	list_for_each(syms, i, list)
		if (streq(sym, i->name))
			return i;
	return NULL;
}

static enum line_compiled get_pp(struct pp_conditions *cond,
				 struct list_head *syms)
{
	struct symbol *sym;
	unsigned int val;
	enum line_compiled parent, ret;

	/* No conditions?  Easy. */
	if (!cond)
		return COMPILED;

	/* Check we get here at all. */
	parent = get_pp(cond->parent, syms);
	if (parent == NOT_COMPILED)
		return NOT_COMPILED;

	if (cond->type == PP_COND_UNKNOWN)
		return MAYBE_COMPILED;

	sym = find_symbol(syms, cond->symbol);
	if (!sym)
		return MAYBE_COMPILED;

	switch (cond->type) {
	case PP_COND_IF:
		/* Undefined is 0. */
		val = sym->value ? *sym->value : 0;
		if (!val == cond->inverse)
			ret = COMPILED;
		else
			ret = NOT_COMPILED;
		break;

	case PP_COND_IFDEF:
		if (cond->inverse == !sym->value)
			ret = COMPILED;
		else
			ret = NOT_COMPILED;
		break;

	default:
		abort();
	}

	/* If parent didn't know, NO == NO, but YES == MAYBE. */
	if (parent == MAYBE_COMPILED && ret == COMPILED)
		ret = MAYBE_COMPILED;
	return ret;
}

static void add_symbol(struct list_head *head,
		       const char *symbol, const unsigned int *value)
{
	struct symbol *sym = tal(head, struct symbol);
	sym->name = symbol;
	sym->value = value;
	list_add(head, &sym->list);
}
	
enum line_compiled get_ccan_line_pp(struct pp_conditions *cond,
				    const char *symbol,
				    const unsigned int *value,
				    ...)
{
	enum line_compiled ret;
	struct list_head *head;
	va_list ap;

	head = tal(NULL, struct list_head);
	list_head_init(head);

	va_start(ap, value);
	add_symbol(head, symbol, value);

	while ((symbol = va_arg(ap, const char *)) != NULL) {
		value = va_arg(ap, const unsigned int *);
		add_symbol(head, symbol, value);
	}
	ret = get_pp(cond, head);
	tal_free(head);
	va_end(ap);
	return ret;
}

static void score_error_vfmt(struct score *score, const char *source,
			     const char *errorfmt, va_list ap)
{

	if (!score->error)
		score->error = tal_strdup(score, "");
	
	if (verbose < 2 && strcount(score->error, "\n") > 5) {
		if (!strends(score->error,
			     "... more (use -vv to see them all)\n")) {
			score->error = tal_strcat(score,
						  take(score->error),
						  "... more (use -vv to see"
						  " them all)\n");
		}
		return;
	}

	tal_append_fmt(&score->error, "%s:", source);
	tal_append_vfmt(&score->error, errorfmt, ap);
	score->error = tal_strcat(score, take(score->error), "\n");
}



void score_error(struct score *score, const char *source,
		 const char *errorfmt, ...)
{
	va_list ap;

	va_start(ap, errorfmt);
	score_error_vfmt(score, source, errorfmt, ap);
	va_end(ap);
}

void score_file_error(struct score *score, struct ccan_file *f, unsigned line,
		      const char *errorfmt, ...)
{
	va_list ap;
	char *source;

	struct file_error *fe = tal(score, struct file_error);
	fe->file = f;
	fe->line = line;
	list_add_tail(&score->per_file_errors, &fe->list);

	if (line)
		source = tal_fmt(score, "%s:%u", f->fullname, line);
	else
		source = tal_fmt(score, "%s", f->fullname);

	va_start(ap, errorfmt);
	score_error_vfmt(score, source, errorfmt, ap);
	va_end(ap);
}


char *get_or_compile_info(const void *ctx UNNEEDED, const char *dir)
{
	struct manifest *m = get_manifest(NULL, dir);

	if (!m->info_file->compiled[COMPILE_NORMAL])
		m->info_file->compiled[COMPILE_NORMAL] = compile_info(m, dir);

	return m->info_file->compiled[COMPILE_NORMAL];
}
