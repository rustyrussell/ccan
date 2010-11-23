#include "ccanlint.h"
#include <ccan/talloc/talloc.h>
#include <ccan/str/str.h>
#include <ccan/str_talloc/str_talloc.h>
#include <ccan/grab_file/grab_file.h>
#include <ccan/noerr/noerr.h>
#include <ccan/foreach/foreach.h>
#include <ccan/asort/asort.h>
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

const char *ccan_dir;

const char *get_ccan_file_contents(struct ccan_file *f)
{
	if (!f->contents) {
		f->contents = grab_file(f, f->fullname, &f->contents_size);
		if (!f->contents)
			err(1, "Reading file %s", f->fullname);
	}
	return f->contents;
}

char **get_ccan_file_lines(struct ccan_file *f)
{
	if (!f->lines)
		f->lines = strsplit(f, get_ccan_file_contents(f),
				    "\n", &f->num_lines);

	return f->lines;
}

struct list_head *get_ccan_file_docs(struct ccan_file *f)
{
	if (!f->doc_sections) {
		get_ccan_file_lines(f);
		f->doc_sections = extract_doc_sections(f->lines, f->num_lines);
	}
	return f->doc_sections;
}

struct ccan_file *new_ccan_file(const void *ctx, const char *dir, char *name)
{
	struct ccan_file *f;

	assert(dir[0] == '/');

	f = talloc(ctx, struct ccan_file);
	f->lines = NULL;
	f->line_info = NULL;
	f->doc_sections = NULL;
	f->compiled = NULL;
	f->name = talloc_steal(f, name);
	f->fullname = talloc_asprintf(f, "%s/%s", dir, f->name);
	f->contents = NULL;
	f->cov_compiled = NULL;
	return f;
}

static void add_files(struct manifest *m, const char *dir)
{
	DIR *d;
	struct dirent *ent;

	if (dir[0])
		d = opendir(dir);
	else
		d = opendir(".");
	if (!d)
		err(1, "Opening directory %s", dir[0] ? dir : ".");

	while ((ent = readdir(d)) != NULL) {
		struct stat st;
		struct ccan_file *f;
		struct list_head *dest;
		bool is_c_src;

		if (ent->d_name[0] == '.')
			continue;

		f = new_ccan_file(m, m->dir,
				  talloc_asprintf(m, "%s%s",
						  dir, ent->d_name));
		if (lstat(f->name, &st) != 0)
			err(1, "lstat %s", f->name);

		if (S_ISDIR(st.st_mode)) {
			f->name = talloc_append_string(f->name, "/");
			add_files(m, f->name);
			continue;
		}
		if (!S_ISREG(st.st_mode)) {
			talloc_free(f);
			continue;
		}

		if (streq(f->name, "_info")) {
			m->info_file = f;
			continue;
		}

		is_c_src = strends(f->name, ".c");
		if (!is_c_src && !strends(f->name, ".h")) {
			dest = &m->other_files;
			continue;
		}

		if (!strchr(f->name, '/')) {
			if (is_c_src)
				dest = &m->c_files;
			else
				dest = &m->h_files;
		} else if (strstarts(f->name, "test/")) {
			if (is_c_src) {
				if (strstarts(f->name, "test/api"))
					dest = &m->api_tests;
				else if (strstarts(f->name, "test/run"))
					dest = &m->run_tests;
				else if (strstarts(f->name, "test/compile_ok"))
					dest = &m->compile_ok_tests;
				else if (strstarts(f->name, "test/compile_fail"))
					dest = &m->compile_fail_tests;
				else
					dest = &m->other_test_c_files;
			} else
				dest = &m->other_test_files;
		} else
			dest = &m->other_files;

		list_add(dest, &f->list);
	}
	closedir(d);
}

static int cmp_names(struct ccan_file *const *a, struct ccan_file *const *b,
		     void *unused)
{
	return strcmp((*a)->name, (*b)->name);
}

static void sort_files(struct list_head *list)
{
	struct ccan_file **files = NULL, *f;
	unsigned int i, num;

	num = 0;
	while ((f = list_top(list, struct ccan_file, list)) != NULL) {
		files = talloc_realloc(NULL, files, struct ccan_file *, num+1);
		files[num++] = f;
		list_del(&f->list);
	}
	asort(files, num, cmp_names, NULL);

	for (i = 0; i < num; i++)
		list_add_tail(list, &files[i]->list);
	talloc_free(files);
}

struct manifest *get_manifest(const void *ctx, const char *dir)
{
	struct manifest *m = talloc(ctx, struct manifest);
	char *olddir;
	unsigned int len;
	struct list_head *list;

	m->info_file = NULL;
	m->compiled = NULL;
	list_head_init(&m->c_files);
	list_head_init(&m->h_files);
	list_head_init(&m->api_tests);
	list_head_init(&m->run_tests);
	list_head_init(&m->compile_ok_tests);
	list_head_init(&m->compile_fail_tests);
	list_head_init(&m->other_test_c_files);
	list_head_init(&m->other_test_files);
	list_head_init(&m->other_files);
	list_head_init(&m->examples);
	list_head_init(&m->mangled_examples);
	list_head_init(&m->deps);

	olddir = talloc_getcwd(NULL);
	if (!olddir)
		err(1, "Getting current directory");

	if (chdir(dir) != 0)
		err(1, "Failed to chdir to %s", dir);

	m->dir = talloc_getcwd(m);
	if (!m->dir)
		err(1, "Getting current directory");

	len = strlen(m->dir);
	while (len && m->dir[len-1] == '/')
		m->dir[--len] = '\0';

	m->basename = strrchr(m->dir, '/');
	if (!m->basename)
		errx(1, "I don't expect to be run from the root directory");
	m->basename++;

	/* We expect the ccan dir to be two levels above module dir. */
	if (!ccan_dir) {
		char *p;
		ccan_dir = talloc_strdup(NULL, m->dir);
		p = strrchr(ccan_dir, '/');
		*p = '\0';
		p = strrchr(ccan_dir, '/');
		*p = '\0';
	}

	add_files(m, "");

	/* Nicer to run tests in a predictable order. */
	foreach_ptr(list, &m->api_tests, &m->run_tests, &m->compile_ok_tests,
		    &m->compile_fail_tests)
		sort_files(list);

	if (chdir(olddir) != 0)
		err(1, "Returning to original directory '%s'", olddir);
	talloc_free(olddir);

	return m;
}


/**
 * remove_comments - strip comments from a line, return copy.
 * @line: line to copy
 * @in_comment: are we already within a comment (from prev line).
 * @unterminated: are we still in a comment for next line.
 */
static char *remove_comments(const char *line, bool in_comment,
			     bool *unterminated)
{
	char *p, *ret = talloc_array(line, char, strlen(line) + 1);

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
	return strspn(line, " \t") == strlen(line);
}

static bool continues(const char *line)
{
	/* Technically, any odd number of these.  But who cares? */
	return strends(line, "\\");
}

/* Get token if it's equal to token. */
bool get_token(const char **line, const char *token)
{
	unsigned int toklen;

	*line += strspn(*line, " \t");
	if (isalnum(token[0]) || token[0] == '_')
		toklen = strspn(*line, IDENT_CHARS);
	else {
		/* FIXME: real tokenizer handles ++ and other multi-chars.  */
		toklen = strlen(token);
	}

	if (toklen == strlen(token) && !strncmp(*line, token, toklen)) {
		*line += toklen;
		return true;
	}
	return false;
}

char *get_symbol_token(void *ctx, const char **line)
{
	unsigned int toklen;
	char *ret;

	*line += strspn(*line, " \t");
	toklen = strspn(*line, IDENT_CHARS);
	if (!toklen)
		return NULL;
	ret = talloc_strndup(ctx, *line, toklen);
	*line += toklen;
	return ret;
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
	return true;
}

/* FIXME: Get serious! */
static struct pp_conditions *analyze_directive(struct ccan_file *f,
					       const char *line,
					       struct pp_conditions *parent)
{
	struct pp_conditions *cond = talloc(f, struct pp_conditions);
	bool unused;

	line = remove_comments(line, false, &unused);

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
		talloc_free(cond);
		/* Malformed? */
		if (!parent)
			return NULL;
		/* Back up one! */
		return parent->parent;
	} else {
		/* Not a conditional. */
		talloc_free(cond);
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
	f->line_info = talloc_array(f->lines, struct line_info, f->num_lines);

	for (i = 0; i < f->num_lines; continued = continues(f->lines[i++])) {
		char *p;
		bool still_doc_line;

		/* Current conditions apply to this line. */
		f->line_info[i].cond = cond;
		f->line_info[i].continued = continued;

		if (continued) {
			/* Same as last line. */
			f->line_info[i].type = f->line_info[i-1].type;
			/* Update in_comment. */
			remove_comments(f->lines[i], in_comment, &in_comment);
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

		p = remove_comments(f->lines[i], in_comment, &in_comment);
		if (is_empty(p)) {
			if (strstarts(f->lines[i], "/**") || still_doc_line)
				f->line_info[i].type = DOC_LINE;
			else
				f->line_info[i].type = COMMENT_LINE;
		} else
			f->line_info[i].type = CODE_LINE;
		talloc_free(p);
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
	struct symbol *sym = talloc(head, struct symbol);
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

	head = talloc(NULL, struct list_head);
	list_head_init(head);

	va_start(ap, value);
	add_symbol(head, symbol, value);

	while ((symbol = va_arg(ap, const char *)) != NULL) {
		value = va_arg(ap, const unsigned int *);
		add_symbol(head, symbol, value);
	}
	ret = get_pp(cond, head);
	talloc_free(head);
	return ret;
}

void score_file_error(struct score *score, struct ccan_file *f, unsigned line,
		      const char *error)
{
	struct file_error *fe = talloc(score, struct file_error);
	fe->file = f;
	fe->line = line;
	fe->error = error;
	list_add_tail(&score->per_file_errors, &fe->list);
}
