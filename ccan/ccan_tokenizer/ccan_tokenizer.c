/*
        Copyright (c) 2009  Joseph A. Adams
        All rights reserved.
        
        Redistribution and use in source and binary forms, with or without
        modification, are permitted provided that the following conditions
        are met:
        1. Redistributions of source code must retain the above copyright
           notice, this list of conditions and the following disclaimer.
        2. Redistributions in binary form must reproduce the above copyright
           notice, this list of conditions and the following disclaimer in the
           documentation and/or other materials provided with the distribution.
        3. The name of the author may not be used to endorse or promote products
           derived from this software without specific prior written permission.
        
        THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
        IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
        OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
        IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
        INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
        NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
        DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
        THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
        (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
        THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "ccan_tokenizer.h"

#include <ccan/talloc/talloc.h>

#include <assert.h>

//Shown by operator precedence; based on
// http://tigcc.ticalc.org/doc/opers.html#precedence .

static struct dict_entry c_dictionary[] = {
//1. Highest
	{'(',"("}, {')',")"},
	{'[',"["}, {']',"]"},
	{'{',"{"}, {'}',"}"},
	{'.',"."},
	{PTR_OP,"->"},
	
//2. Unary
	{'!',"!"}, {'~',"~"}, //prefix
	{INC_OP,"++"}, {DEC_OP,"--"}, //prefix or postfix
	// + - & *
	
//3. Multiplicative
	// *
	{'/',"/"}, {'%',"%"},
	
//4. Additive
	// + -
	
//5. Shift
	{LEFT_OP,"<<"}, {RIGHT_OP,">>"},
	
//6. Relational
	{'<',"<"}, {'>',">"},
	{LE_OP,"<="}, {GE_OP,">="},
	
//7. Equality
	{EQ_OP,"=="}, {NE_OP,"!="},
	
//8. Bitwise AND
	// &
//9. Bitwise XOR
	{'^',"^"},
//10. Bitwise OR
	{'|',"|"},

//11. Logical AND
	{AND_OP,"&&"},
//12. Logical OR
	{OR_OP,"||"},

//13. Conditional
	{'?',"?"}, {':',":"},

//14. Assignment
	{'=',"="},
	{MUL_ASSIGN,"*="}, {DIV_ASSIGN,"/="}, {MOD_ASSIGN,"%="},
	{ADD_ASSIGN,"+="}, {SUB_ASSIGN,"-="},
	{AND_ASSIGN,"&="}, {XOR_ASSIGN,"^="}, {OR_ASSIGN,"|="},
	{LEFT_ASSIGN,"<<="}, {RIGHT_ASSIGN,">>="},
	
//15. Comma
	{',',","},

//16. Semicolon
	{';',";"},
	
//Misc
	{ELLIPSIS,"..."},
	{'#',"#"},
	{DOUBLE_POUND,"##"},

//Ambiguous
	//unary or binary
	{'+',"+"}, {'-',"-"},
	{'&',"&"}, {'*',"*"},

//Keywords
	{_BOOL, "_Bool"},
	{_COMPLEX, "_Complex"},
	{_IMAGINARY, "_Imaginary"},
	{BREAK, "break"},
	{CASE, "case"},
	{CHAR, "char"},
	{CONST, "const"},
	{CONTINUE, "continue"},
	{DEFAULT, "default"},
	{DO, "do"},
	{DOUBLE, "double"},
	{ELSE, "else"},
	{ENUM, "enum"},
	{EXTERN, "extern"},
	{FLOAT, "float"},
	{FOR, "for"},
	{GOTO, "goto"},
	{IF, "if"},
	{INLINE, "inline"},
	{INT, "int"},
	{LONG, "long"},
	{REGISTER, "register"},
	{RESTRICT, "restrict"},
	{RETURN, "return"},
	{SHORT, "short"},
	{SIGNED, "signed"},
	{SIZEOF, "sizeof"},
	{STATIC, "static"},
	{STRUCT, "struct"},
	{SWITCH, "switch"},
	{TYPEDEF, "typedef"},
	{UNION, "union"},
	{UNSIGNED, "unsigned"},
	{VOID, "void"},
	{VOLATILE, "volatile"},
	{WHILE, "while"},

//Preprocessor keywords (except those already defined)
	{VA_ARGS, "__VA_ARGS__"},
	{DEFINE, "define"},
	{ELIF, "elif"},
//	{ELSE, "else"},
	{ENDIF, "endif"},
	{ERROR, "error"},
//	{IF, "if"},
	{IFDEF, "ifdef"},
	{IFNDEF, "ifndef"},
	{INCLUDE, "include"},
	{LINE, "line"},
	{PRAGMA, "pragma"},
	{UNDEF, "undef"},
	{WARNING, "warning"},
};

#if 0

struct tokenizer *tokenizer_new(void *ctx) {
	struct tokenizer *t = talloc(ctx, struct tokenizer);
	t->ctx = ctx;
	queue_init(t->mq, t);
	t->dict = dict_build(t, c_dictionary, sizeof(c_dictionary)/sizeof(*c_dictionary));
	
	return t;
}

#endif

static int talloc_darray_destructor(void *ptr);

/*
 * darray(T) *talloc_darray(const void *context);
 *
 * Create a new darray anchored in a talloc buffer.
 * When this pointer is freed, the darray will be freed as well.
 */
static void *talloc_darray(const void *context)
{
	void *ret = talloc(context, darray(void));
	darray_init(*(darray(void)*)ret);
	talloc_set_destructor(ret, talloc_darray_destructor);
	return ret;
}

static int talloc_darray_destructor(void *ptr)
{
	darray(void) *arr = ptr;
	free(arr->item);
	return 0;
}

#define MESSAGE_PATH "tokenize/"

static void unbreak_backslash_broken_lines(struct token_list *tl, tok_message_queue *mq) {
	const char *s = tl->orig, *e = s+tl->orig_size;
	darray_char         *txt    = talloc_darray(tl);
	darray(const char*) *olines = talloc_darray(tl);
	darray(const char*) *tlines = talloc_darray(tl);
	
	do {
		const char *line_start = s, *line_end;
		const char *lnw; //last non-white
		size_t start_offset = txt->size;
		
		//scan to the next line and find the last non-white character in the line
		while (s<e && !creturn(*s)) s++;
		line_end = s;
		lnw = s;
		while (lnw>line_start && cspace(lnw[-1])) lnw--;
		if (s<e && creturn(*s)) {
			s++;
			//check for non-standard newlines (i.e. "\r", "\r\n", or "\n\r")
			if (s<e && *s=='\n'+'\r'-s[-1])
				s++;
		}
		
		//add the backslash-break-free version of the text
		if (lnw>line_start && lnw[-1]=='\\' && line_end<e) {
			darray_append_items(*txt, line_start, lnw-1-line_start);
			if (lnw<e && cspace(*lnw)) {
				tok_msg_warn(spaces_after_backslash_break, lnw,
					"Trailing spaces after backslash-broken line");
			}
		} else
			darray_append_items(*txt, line_start, s-line_start);
		
		//add the line starts for this line
		darray_append(*olines, line_start);
		darray_append(*tlines, (const char*)start_offset);
			//Since the txt buffer moves when expanded, we're storing offsets
			//  for now.  Once we're done building txt, we can add the base
			//  of it to all the offsets to make them pointers.
	} while (s<e);
	
	//stick a null terminator at the end of the text
	darray_realloc(*txt, txt->size+1);
	txt->item[txt->size] = 0;
	
	//convert the line start offsets to pointers
	{
		const char **i;
		darray_foreach(i, *tlines)
			*i = txt->item + (size_t)(*i);
	}
	
	tl->olines = olines->item;
	tl->olines_size = olines->size;
	tl->txt = txt->item;
	tl->txt_size = txt->size;
	tl->tlines = tlines->item;
	tl->tlines_size = tlines->size;
}

static void normal_keyword(struct token *tok) {
	if (tok->type==TOK_KEYWORD &&
			(opkw_is_directive_only(tok->opkw) || tok->opkw==VA_ARGS))
		tok->type = TOK_IDENTIFIER;
}

static int define_parmlist_has_ellipsis(struct token *start, struct token *end) {
	while (end>start && token_is_ignored(end-1)) end--;
	return (end-->start && end->type==TOK_OPERATOR && end->opkw==ELLIPSIS);
}

//Used to label __VA_ARGS__ as keywords within applicable macro expansions
//Start should follow the DEFINE directive keyword
static void this_is_a_define(struct token *start, struct token *end) {
	struct token *i = start, *pl_start;
	
	//skip past the identifier that is defined
	while (i<end && token_is_ignored(i)) i++;
	if (i >= end)
		return;
	 //TODO:  check i->type to make sure it's an identifier, throw error otherwise
	normal_keyword(i++);
	
	//see if this is actually a variadic macro
	if (!(i<end && i->type==TOK_OPERATOR && i->opkw=='('))
		goto not_va_args;
	pl_start = ++i;
	while (i<end && !(i->type==TOK_OPERATOR && i->opkw==')'))
		normal_keyword(i++);
	if (!define_parmlist_has_ellipsis(pl_start, i++))
		goto not_va_args;
	
	//We have arrived at the macro expansion and know there is a ... argument
	//Thus, we'll only change directive-only keywords to identifiers
	for(; i<end; i++) {
		if (i->type==TOK_KEYWORD && opkw_is_directive_only(i->opkw))
			i->type = TOK_IDENTIFIER;
	}
	
not_va_args:
	while (i < end)
		normal_keyword(i++);
}

//fill the flags field of each token and untangle keywords and such
static void finalize_line(struct token *start, struct token *end) {
	struct token *i = start, *j;
	
	assert(start<end && start->type==TOK_STARTLINE);
	i++;
	
	while (i<end && token_is_ignored(i)) i++;
	
	if (i<end && i->type==TOK_OPERATOR && i->opkw=='#') {
	//preprocessor line
		i->type = TOK_LEADING_POUND;
		
		//set pp on all tokens in this line
		for (j=start; j<end; j++)
			j->flags.pp = 1;
		
		//find the relevant token after the '#'
		for (i++; i<end; i++) {
			if (!token_is_ignored(i)) {
				i->flags.pp_directive = 1;
				if (i->type==TOK_KEYWORD && !opkw_is_directive(i->opkw))
					i->type = TOK_IDENTIFIER;
				//TODO:  Handle invalid preprocessor directives (e.g. #+ )
				
				if (i->type==TOK_KEYWORD && i->opkw==DEFINE) {
					for (j=i+1; j<end; j++)
					this_is_a_define(i+1, end);
				} else {
					while (++i < end)
						normal_keyword(i);
				}
				break;
			}
		}
	} else {
	//normal line
		while (i < end)
			normal_keyword(i++);
	}
}

//fill the list, flags, line, col, orig, and orig_size fields of each token
//convert identifiers mistaken for preprocessor keywords (e.g. ifdef) to identifiers
static void finalize(struct token_list *tl, struct token *start, struct token *end) {
	const char * const *lss = tl->tlines;
	const char * const *lse = lss + tl->tlines_size;
	struct token *i;
	struct token *startline = NULL;
	
	assert(start < end);
	
	tl->first = start;
	tl->last = end-1;
	
	for (i=start; ; i++) {
		//perform a second pass on each line
		if (i >= end || i->type == TOK_STARTLINE) {
			if (startline)
				finalize_line(startline, i);
			startline = i;
		}
		
		if (i >= end) {
			end[-1].orig_size = tl->orig+tl->orig_size - end[-1].orig;
			break;
		}
		
		//set up the list links
		i->prev = i>start ? i-1 : NULL;
		i->next = i+1<end ? i+1 : NULL;
		
		//if i->txt starts on a later line, advance to it
		while (lss+1<lse && i->txt >= lss[1] && i->txt > lss[0])
			lss++;
		
		//set up line, col, orig, and orig_size
		i->line = lss - tl->tlines;
		i->col = i->txt - *lss;
		i->orig = tl->olines[i->line] + i->col;
		if (i > start)
			i[-1].orig_size = i->orig - i[-1].orig;
		
		assert(i->line < tl->olines_size);
		
		//clear the flags
		memset(&i->flags, 0, sizeof(i->flags));
	}
}

#define add(...) do { \
		struct token tok = {__VA_ARGS__}; \
		tok.txt = orig; \
		tok.txt_size = s-orig; \
		darray_append(*arr, tok); \
	} while (0)

#define cstray(c) (ccontrol(c) || cextended(c) || (c)=='@' || (c)=='`' || (c)=='\\')
#define cident(c) (cletter(c) || cdigit(c) || c=='_' || c=='$')
	//believe it or not, $ is a valid character in an identifier

struct dict *tokenizer_dict = NULL;

static void free_tokenizer_dict(void) {
	talloc_free(tokenizer_dict);
}

struct token_list *tokenize(const void *tcontext, const char *orig, size_t orig_size,
				tok_message_queue *mq) {
	struct token_list *tl = talloc(tcontext, struct token_list);
	const char *s, *e;
	size_t stray_count=0, cr_count=0;
	darray(struct token) *arr = talloc_darray(tl);
	int only_pound_include = 0;
	
	if (!tokenizer_dict) {
		tokenizer_dict = dict_build(NULL, c_dictionary,
			sizeof(c_dictionary)/sizeof(*c_dictionary));
		atexit(free_tokenizer_dict);
	}
	
	tl->orig = orig;
	tl->orig_size = orig_size;
	unbreak_backslash_broken_lines(tl, mq);
	tl->filename = NULL;
	
	s = tl->txt;
	e = s + tl->txt_size;
	
	darray_appends_t(*arr, struct token, {
		.type = TOK_STARTLINE,
		.txt = s,
		.txt_size = 0
	} );
	
	while (s<e) {
		const char *orig = s;
		char c = *s++;
		int added_something = 1;
		
		if (cstray(c)) {
			stray_count++;
			while (s<e && cstray(*s)) {
				s++;
				stray_count++;
			}
			add(.type = TOK_STRAY);
			
			/* This has the potential to be very noisy on binary
			   files, but it really is quite useful. */
			tok_msg_error(stray_segment, orig,
				"%zu stray characters", s-orig);
		
		} else if (creturn(c)) {
			//check for non-standard newlines (i.e. "\r", "\r\n", or "\n\r")
			if (s<e && *s=='\n'+'\r'-c) {
				s++;
				cr_count++;
			} else if (c=='\r')
				cr_count++;
			
			add(.type = TOK_WHITE);
			orig = s;
			
			//add a TOK_STARTLINE for the next line unless this is the end of the document
			if (s<e)
				add(.type = TOK_STARTLINE);
			
			only_pound_include = 0;
		
		} else if (cspace(c)) {
			//skip over the remaining whitespace
			while (s<e && cspace(*s)) s++;
			add(.type = TOK_WHITE);
			added_something = 0;
		
		} else if (cdigit(c) || (c=='.' && s<e && cdigit(*s))) {
			struct token tok;
			s = read_cnumber(&tok, s-1, e, mq);
			tok.txt = orig;
			tok.txt_size = s-orig;
			darray_append(*arr, tok);
			
		} else if (csymbol(c) || cident(c)) {
			if (only_pound_include && (c=='"' || c=='<')) { //include string
				char *include;
				char end = c=='"' ? '"' : '>';
				short type = c=='"' ? TOK_STRING_IQUOTE : TOK_STRING_IANGLE;
				
				while (s<e && !creturn(*s) && *s!=end) s++;
				include = talloc_strndup(tl, orig+1, s-(orig+1));
				
				if (s<e && *s==end) {
					s++;
				} else {
					tok_msg_error(include_missing_terminator, orig,
						"Missing terminating %c character", end);
				}
				
				add(.type = type,
					{.include = include});
				
			} else if (c=='\'' || c=='\"') { //character or string literal
				darray_char *string = talloc_darray(tl);
				s = read_cstring(string, s, e, c, mq);
				if (s<e) s++; //advance past endquote (if available)
				add(.type = c=='\'' ? TOK_CHAR : TOK_STRING,
				    {.string = string});
				
				if (c=='\'' && string->size==0) {
					tok_msg_error(empty_char_constant, orig,
						"Empty character constant");
				}
				
			} else if (c=='/' && s<e && (*s=='*' || *s=='/')) { //comment
				if (*s++ == '*') { /* C-style comment */
					const char *comment_start = s-2;
					for (;;s++) {
						if (s+1 >= e) {
							s = e;
							tok_msg_error(unterminated_comment, comment_start,
								"Unterminated comment");
							break;
						}
						if (s[0]=='*' && s[1]=='/') {
							s += 2;
							break;
						}
					}
					add(.type = TOK_CCOMMENT);
				} else { // C++-style comment
					while (s<e && !creturn(*s)) s++;
					add(.type = TOK_CPPCOMMENT);
				}
				added_something = 0;
			
			} else { //operator, keyword, or identifier
				struct dict_entry *ent;
				const char *ident_e = --s;
				while (ident_e<e && cident(*ident_e) ) ident_e++;
				
				ent = dict_lookup(tokenizer_dict, &s, e);
				if (cident(c)) { //keyword or identifier
					if (ent && s==ident_e) {
						add(.type = TOK_KEYWORD,
							{.opkw = ent->id});
						if (ent->id == INCLUDE) {
							//hacky way to lex #include string properly
							struct token *ts = arr->item;
							struct token *tp = ts+arr->size-1;
							while (tp>ts && token_is_ignored(tp-1))
								tp--;
							if (tp>ts && token_is_op(tp-1, '#')) {
								tp--;
								while (tp>ts && token_is_ignored(tp-1))
									tp--;
								if (tp>ts && tp[-1].type==TOK_STARTLINE) {
									only_pound_include = 1;
									continue;
								}
							}
						}
					} else {
						s = ident_e;
						add(.type = TOK_IDENTIFIER);
					}
				} else if (ent) { //operator
					add(.type = TOK_OPERATOR,
					    {.opkw = ent->id});
				} else { //invalid symbol (shouldn't happen)
					tok_msg_bug(unrecognized_symbol, s,
						"Unrecognized symbol \'%c\'", c);
					s++;
					add(.type = TOK_STRAY);
				}
			}
		}
		
		if (added_something)
			only_pound_include = 0;
	}
	
	/*if (stray_count) {
		tok_msg_error(stray_characters, NULL,
			"%lu stray characters in text", (unsigned long)stray_count);
	}*/
	if (cr_count) {
		tok_msg_warn(nonstandard_newlines, NULL,
			"Text contains non-standard line terminators");
	}
	
	finalize(tl, arr->item, arr->item+arr->size);
	
	return tl;
}

size_t token_list_count(const struct token_list *tl) {
	size_t ret = 0;
	const struct token *i;
	
	for (i=tl->first; i; i=i->next)
		ret++;
	
	return ret;
}

static size_t find_line(const char *ptr, const char * const *lines, size_t line_count) {
	const char * const *orig = lines;
	const char * const *orig_e = lines+line_count;
	
	while (line_count > 1) {
		size_t middle = line_count>>1;
		if (ptr < lines[middle])
			line_count = middle;
		else {
			lines += middle;
			line_count -= middle;
		}
	}
	
	//select the *last* of equivalent lines
	while (lines+1 < orig_e && lines[0]==lines[1])
		lines++;
	
	// (don't) select the *first* of equivalent lines
	//while (lines>orig && lines<orig_e && lines[-1]==lines[0])
	//	lines--;
	
	return lines - orig;
}

int tok_point_lookup(struct tok_point *out, const char *ptr,
			const struct token_list *tl) {
	size_t line_count = tl->olines_size;
	
	memset(out, 0, sizeof(*out));
	if (!tl)
		return 0;
	
	if (ptr >= tl->txt && ptr <= tl->txt+tl->txt_size) {
		out->txt = ptr;
		out->line = find_line(ptr, tl->tlines, line_count);
		if (out->line < line_count) {
			out->col = ptr - tl->tlines[out->line];
			out->orig = tl->olines[out->line] + out->col;
		} else {
			out->col = 0;
			out->orig = tl->orig + tl->orig_size;
		}
		return 1;
	} else if (ptr >= tl->orig && ptr <= tl->orig+tl->orig_size) {
		out->orig = ptr;
		out->line = find_line(ptr, tl->olines, line_count);
		if (out->line < line_count) {
			const char *tline_start = tl->tlines[out->line];
			const char *tline_end = out->line+1 < line_count ?
				tl->tlines[out->line+1] :
				tl->txt + tl->txt_size;
			
			out->col = ptr - tl->olines[out->line];
			out->txt = tline_start + out->col;
			
			if (out->txt > tline_end)
				out->txt = tline_end;
		} else {
			out->col = 0;
			out->txt = tl->txt + tl->txt_size;
		}
		return 1;
	} else {
		return 0;
	}
}

static char *escape_string(darray_char *buf, const char *str, size_t size) {
	const char *s = str, *e = s+size;
	darray_from_lit(*buf, "");
	
	for (;s<e;s++) {
		char buffer[8];
		const char *esc = buffer;
		unsigned char c = (unsigned char)*s;
		if (ccontrol(c))
			sprintf(buffer, "\\x%02X", c);
		else switch(c) {
			case '\t': esc = "\\t"; break;
			case '\n': esc = "\\n"; break;
			case '\v': esc = "\\v"; break;
			case '\f': esc = "\\f"; break;
			case '\r': esc = "\\r"; break;
			case '"': esc = "\\\""; break;
			case '\\': esc = "\\\\"; break;
			default:
				buffer[0] = c;
				buffer[1] = 0;
		}
		darray_append_string(*buf, esc);
	}
	
	return buf->item;
}

static int txt_orig_matches(const char *txt, size_t txt_size, const char *orig, size_t orig_size) {
	const char *ts = txt, *te = ts+txt_size;
	const char *os = orig, *oe = os+orig_size;
	
	do {
		const char *ob = os; //start of next backslash break
		const char *obe; //end of next backslash break
		size_t size; //amount of text to compare for this round
		
		while (ob<oe && *ob!='\\') ob++;
		obe = ob;
		if (obe < oe) { //there's a backslash
			obe++;
			while (obe<oe && cspace(*obe)) obe++;
			if (obe<oe && creturn(*obe)) { //there's a backslash-broken line
				obe++;
				if (obe<oe && *obe == '\n'+'\r'-obe[-1])
					obe++;
			} else //this is just a plain old backslash
				ob = obe;
		}
		
		size = ob-os;
		
		if (ts+size > te || memcmp(ts, os, size))
			return 0;
		ts += size;
		os = obe;
	} while (ts<te);
	
	if (ts != te || os != oe)
		return 0;
	
	return 1;
}

static int is_backslash_break(const char **end, const char *s, const char *e) {
	if (s<e && *s == '\\') {
		s++;
		while (s<e && cspace(*s)) s++;
		if (s<e && creturn(*s)) {
			s++;
			if (s<e && *s=='\n'+'\r'-s[-1])
				s++;
			*end = s;
			return 1;
		}
		return 0;
	}
	return 0;
}

#define failed(fmt, ...) do {fprintf(err, fmt "\n", ##__VA_ARGS__); return 0; } while(0)

//tests that should pass on an untainted token list out of the tokenize() function
static int token_list_sanity_check_initial(const struct token_list *tl, FILE *err) {
	struct token *first = tl->first;
	struct token *last = tl->last;
	struct token *i;
	const char *txt=tl->txt, *orig=tl->orig;
	const char *txt_e = txt+tl->txt_size, *orig_e = orig+tl->orig_size;
	
	if ((char*)first > (char*)last ||
		(size_t)((char*)last - (char*)first) % sizeof(struct token))
		failed("Token list pointers don't look right");
	
	//token list should not end with TOK_STARTLINE unless
	//  the document is empty
	if (last!=first && last->type==TOK_STARTLINE)
		return 0;
	
	for (i=first; i; i=i->next) {
		//Verify list links
		if (i != first && i->prev != i-1)
			failed("list.prev is incorrect");
		if (i != last && i->next != i+1)
			failed("list.next is incorrect");
		
		//Make sure txt segments fill the entire tl->txt
		if (i->txt != txt)
			failed("txt does not fill the token list");
		txt += i->txt_size;
		if (txt > txt_e)
			failed("txt is out of bounds");
		
		//Make sure orig segments fill the entire tl->orig
		if (i->orig != orig)
			failed("orig does not fill the token list");
		orig += i->orig_size;
		if (orig > orig_e)
			failed("orig is out of bounds");
	}
	
	if (txt != txt_e)
		return 0;
	if (orig != orig_e)
		return 0;
	
	return 1;
}

int token_list_sanity_check(const struct token_list *tl, FILE *err) {
	struct token *first = tl->first;
	struct token *last = tl->last;
	struct token *i;
	int initial = 1;
	
	if (tl->first == NULL || tl->last == NULL)
		failed("Token list is completely empty");
	
	if (first->type!=TOK_STARTLINE ||
	    first->txt!=tl->txt || first->txt_size!=0 ||
	    first->orig!=tl->orig || first->orig_size!=0 ||
	    first->line!=0 || first->col!=0)
		failed("Token list does not start with a valid TOK_STARTLINE");
	
	if (first->prev!=NULL || last->next!=NULL)
		failed("Token edge links are not NULL");
	
	for (i=first; i; i=i->next) {
		//Verify line,col
		if (tl->tlines[i->line] + i->col != i->txt)
			failed("line,col is wrong against txt");
		if (tl->olines[i->line] + i->col != i->orig)
			failed("line,col is wrong against orig");
		
		//Make sure tokens have proper sizes
		if (i->type!=TOK_STARTLINE && (i->txt_size==0 || i->orig_size==0 || i->txt_size > i->orig_size) )
			failed("Token is empty");
		if (i->type==TOK_STARTLINE && (i->txt_size!=0 || i->orig_size!=0) )
			failed("TOK_STARTLINE is non-empty");
		
		//Make sure TOK_WHITE actually contains white tokens
		if (i->type==TOK_WHITE) {
			const char *s = i->txt, *e = s+i->txt_size;
			while (s<e && cwhite(*s)) s++;
			if (s != e)
				failed("TOK_WHITE does not contain only white characters");
		}
		
		//Make sure txt and orig match exactly except for backslash line breaks
		if (!txt_orig_matches(i->txt, i->txt_size, i->orig, i->orig_size)) {
			darray_char buf = darray_new();
			fprintf(err,
				"txt and orig do not match:\n"
				"\ttxt  = \"%s\"\n",
				escape_string(&buf, i->txt, i->txt_size) );
			fprintf(err, "\torig = \"%s\"\n",
				escape_string(&buf, i->orig, i->orig_size) );
			
			darray_free(buf);
			return 0;
		}
		
		//Make sure tok_point_lookup returns correct point
		{
			struct tok_point tok_point;
			const char *t=i->txt, *o=i->orig, *e=o+i->orig_size, *p;
			size_t line=i->line, col=i->col;
			
			#define check(ptr) do { \
				if (tok_point_lookup(&tok_point, ptr, tl)) { \
					if (tok_point.txt != t || tok_point.orig != o) \
						failed("tok_point_lookup on txt reported incorrect txt/orig (orig is %d, should be %d)", \
						(int)(tok_point.orig-i->orig), (int)(o-i->orig)); \
					if (tok_point.line != line || tok_point.col != col) \
						failed("tok_point_lookup on txt reported incorrect line/col (off by %d, %d)", \
						(int)(tok_point.line-line), (int)(tok_point.col-col)); \
				} else if (initial) {\
					failed("tok_point_lookup failed on initial token list"); \
				} \
			} while(0)
			
			for (;;) {
				while (is_backslash_break(&p, o, e)) {
					while (o<p) {
						check(o);
						o++;
						col++;
					}
					col = 0;
					line++;
				}
				if (o >= e)
					break;
				do {
					if (creturn(*o)) {
						p = o+1;
						if (p<e && *p=='\n'+'\r'-p[-1])
							p++;
						while (o<p) {
							check(o);
							check(t);
							t++, o++, col++;
						}
						line++;
						col = 0;
					} else {
						check(o);
						check(t);
						o++, t++, col++;
					}
				} while (o<e && *o!='\\');
			}
			
			#undef check
		}
	};
	
	//Verify olines and tlines
	{
		const char *s = tl->orig, *e = s+tl->orig_size;
		size_t i, line_count = tl->olines_size;
		
		//both line arrays should be exactly the same size
		if (tl->olines_size != tl->tlines_size)
			return 0;
		
		for (i=0; s<e; i++) {
			const char *line_start = s, *line_end;
			size_t tline_size, oline_size;
			const char *p;
			
			if (i+1 < line_count)
				tline_size = tl->tlines[i+1] - tl->tlines[i];
			else
				tline_size = tl->txt+tl->txt_size - tl->tlines[i];
			
			while (s<e && !creturn(*s)) s++;
			line_end = s;
			if (s<e) {
				s++;
				if (s<e && *s=='\n'+'\r'-s[-1])
					s++;
			}
			
			oline_size = s-line_start;
			
			//verify that olines elements are correct
			if (line_start != tl->olines[i])
				return 0;
			
			//verify that tlines elements are in range
			p = tl->tlines[i];
			if (p < tl->txt || p+tline_size > tl->txt+tl->txt_size)
				return 0;
			
			//verify that original lines have sizes >= the unbroken lines
			if (oline_size < tline_size)
				return 0;
			
			//if sizes are inconsistent, make sure it is due to a backslash escape
			if (oline_size > tline_size) {
				p = line_start+tline_size;
				if (*p++ != '\\')
					return 0;
				while (p<e && cspace(*p)) p++;
				if (p != line_end)
					return 0;
			}
			
			//make sure the text of both copies match
			if ( memcmp(
				tl->olines[i],
				tl->tlines[i],
				tline_size) )
				return 0;
		}
	}
	
	if (initial && !token_list_sanity_check_initial(tl, err))
		failed("Initial sanity checks failed.  Has the list been modified after it was returned from tokenize() ?");
	
	return 1;
}

#undef failed

static char *sprint_token_flags(char buf[3], struct token_flags flags) {
	buf[0] = flags.pp ? 'p' : '-';
	buf[1] = flags.pp_directive ? 'D' : '-';
	buf[2] = 0;
	return buf;
}

void token_list_dump(const struct token_list *tl, FILE *f) {
	struct token *tok;
	darray_char buf = darray_new();
	size_t i = 0;
	char buf2[8];
	const char *token_type_str[] = {
		"TOK_INTEGER      ",
		"TOK_FLOATING     ",
		"TOK_OPERATOR     ",
		"TOK_KEYWORD      ",
		"TOK_IDENTIFIER   ",
		"TOK_CHAR         ",
		"TOK_STRING       ",
		"TOK_LEADING_POUND",
		"TOK_STRING_IQUOTE",
		"TOK_STRING_IANGLE",
		"TOK_CCOMMENT     ",
		"TOK_CPPCOMMENT   ",
		"TOK_WHITE        ",
		"TOK_STARTLINE    ",
		"TOK_STRAY        "
	};
	
	for (tok=tl->first; tok; tok=tok->next) {
		fprintf(f, "%lu\t%s\t%s\t\"%s\"", (unsigned long)(i++),
			token_type_str[tok->type],
			sprint_token_flags(buf2, tok->flags),
			escape_string(&buf, tok->txt, tok->txt_size));
		#if 1 //print tok->orig
		fprintf(f, "\t\"%s\"\n", escape_string(&buf, tok->orig, tok->orig_size));
		#else
		fprintf(f, "\n");
		#endif
	}
	
	darray_free(buf);
}

void tok_message_print(struct tok_message *m, struct token_list *tl) {
	struct tok_point pt;
	int resolved = tok_point_lookup(&pt, m->location, tl);
	
	if (tl->filename) {
		printf("%s:%s", tl->filename, resolved ? "" : " ");
	}
	
	if (resolved) {
		printf("%zu:%zu %s: %s\n",
			pt.line+1, pt.col+1,
			m->level==TM_DEBUG ? "debug" :
			m->level==TM_INFO ? "info" :
			m->level==TM_WARN ? "warning" :
			m->level==TM_ERROR ? "error" :
			m->level==TM_BUG ? "BUG" :
			"???",
			m->message);
	} else {
		printf("%s: %s\n",
			m->level==TM_DEBUG ? "debug" :
			m->level==TM_INFO ? "info" :
			m->level==TM_WARN ? "warning" :
			m->level==TM_ERROR ? "error" :
			m->level==TM_BUG ? "BUG" :
			"???",
			m->message);
	}
}

void tok_message_dump(struct tok_message *m) {
	printf("%s: %s: %s\n",
		m->level==TM_DEBUG ? "debug" :
		m->level==TM_INFO ? "info" :
		m->level==TM_WARN ? "warning" :
		m->level==TM_ERROR ? "error" :
		m->level==TM_BUG ? "BUG" :
		"???", m->path, m->message);
}

void tok_message_add(tok_message_queue *mq, enum tok_message_level level,
	const char *path, const char *loc, const char *fmt, ...) {
	struct tok_message msg = {.level=level, .path=path, .location=loc};
	va_list ap;
	
	if (!mq)
		return;
	
	va_start(ap, fmt);
	msg.message = talloc_vasprintf(mq->item, fmt, ap);
	va_end(ap);
	
	enqueue(*mq, msg);
}

void tok_message_queue_dump(const tok_message_queue *mq) {
	size_t i;
	for (i=0; i<queue_count(*mq); i++)
		tok_message_dump(&queue_item(*mq, i));
}


#undef add
#undef cstray
#undef cident
