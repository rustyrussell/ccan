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

#ifndef CCAN_TOKENIZER_H
#define CCAN_TOKENIZER_H

#include <ccan/darray/darray.h>
#include "charflag.h"
#include "dict.h"
#include "queue.h"
#include <stdint.h>
#include <errno.h> //for readui

/* Definition of tokens and the token list */

enum token_type {
	TOK_INTEGER,	   //integer (e.g. 5, 1000L, 0x5)
	TOK_FLOATING,	   //floating point number (e.g. 5.0, 7.0f, etc.)
	TOK_OPERATOR,	   //operator (e.g. +, -, (, ), ++, etc.)
	
	#define token_type_is_identifier(type) ((type)>=TOK_KEYWORD && (type)<=TOK_IDENTIFIER)
	TOK_KEYWORD,	   //keyword (e.g. char, _Bool, ifdef)
	TOK_IDENTIFIER,	   //identifier or unprocessed keyword (e.g. int, token, pp_conditions)
	
	TOK_CHAR,	   //character literal (e.g. 'a' or even '1234')
	TOK_STRING,	   //string literal (e.g. "hello" or "zero\0inside")
	TOK_LEADING_POUND, //leading # in a preprocessor directive (e.g. # include)
	TOK_STRING_IQUOTE, // "config.h"
	TOK_STRING_IANGLE, // <stdio.h>
	
	#define token_type_is_ignored(type) ((type)>=TOK_CCOMMENT && (type)<=TOK_WHITE)
	#define token_type_is_comment(type) ((type)>=TOK_CCOMMENT && (type)<=TOK_CPPCOMMENT)
	TOK_CCOMMENT, //C comment (e.g. /* comment */)
	TOK_CPPCOMMENT, //C++ comment (e.g. //comment )
	TOK_WHITE, //whitespace (span of \t\n\v\f\r and space)
	
	TOK_STARTLINE,	//beginning of line (txt/txtsize is always empty)
	TOK_STRAY, //control characters, weird characters, and extended characters where they shouldn't be
};

enum tok_suffix {
	TOK_NOSUFFIX = 0,
	
	TOK_U  = 1,  //unsigned
	TOK_L  = 2,  //long or double-precision float
	TOK_LL = 4,  //long long (note that TOK_L and TOK_LL are mutually exclusive)
	TOK_F  = 8,  //float (single-precision)
	TOK_I  = 16, //imaginary
	
	TOK_UL  = TOK_U | TOK_L,  //unsigned long
	TOK_ULL = TOK_U | TOK_LL, //unsigned long long
	
	//Imaginary combo meals
	TOK_IMAG_U   = TOK_I | TOK_U,
	TOK_IMAG_L   = TOK_I | TOK_L,
	TOK_IMAG_LL  = TOK_I | TOK_LL,
	TOK_IMAG_F   = TOK_I | TOK_F,
	
	TOK_IMAG_UL  = TOK_I | TOK_UL,
	TOK_IMAG_ULL = TOK_I | TOK_ULL,
};

struct tok_integer {
	uint64_t v;
	int base; //one of 2, 8, 10, or 16
	enum tok_suffix suffix;
};

struct tok_floating {
	long double v;
	enum tok_suffix suffix;
};

//Operator/keyword naming conventions taken from Jeff Lee's Yacc grammar:
//http://www.lysator.liu.se/c/ANSI-C-grammar-y.html
enum tok_opkw {
	/* Permute these regularly */
	PTR_OP=128, INC_OP, DEC_OP, LEFT_OP, RIGHT_OP, LE_OP, GE_OP, EQ_OP, NE_OP,
	AND_OP, OR_OP,
	MUL_ASSIGN, DIV_ASSIGN, MOD_ASSIGN,
	ADD_ASSIGN, SUB_ASSIGN,
	AND_ASSIGN, XOR_ASSIGN, OR_ASSIGN,
	LEFT_ASSIGN, RIGHT_ASSIGN,
	ELLIPSIS,
	DOUBLE_POUND,
	
	//Keywords
	_BOOL,
	_COMPLEX,
	_IMAGINARY,
	BREAK,
	CASE,
	CHAR,
	CONST,
	CONTINUE,
	DEFAULT,
	DO,
	DOUBLE,
	ELSE,
	ENUM,
	EXTERN,
	FLOAT,
	FOR,
	GOTO,
	IF,
	INLINE,
	INT,
	LONG,
	REGISTER,
	RESTRICT,
	RETURN,
	SHORT,
	SIGNED,
	SIZEOF,
	STATIC,
	STRUCT,
	SWITCH,
	TYPEDEF,
	UNION,
	UNSIGNED,
	VOID,
	VOLATILE,
	WHILE,
	
	//Preprocessor keywords (except those already defined)
	VA_ARGS,
	#define opkw_is_directive_only(opkw) ((opkw)>=DEFINE && (opkw)<=WARNING)
	#define opkw_is_directive(opkw) (opkw_is_directive_only(opkw) || (opkw)==ELSE || (opkw)==IF)
	DEFINE,
	ELIF,
	//ELSE,
	ENDIF,
	ERROR,
	//IF,
	IFDEF,
	IFNDEF,
	INCLUDE,
	LINE,
	PRAGMA,
	UNDEF,
	WARNING, /* gcc extension */
};

struct token_flags {
	unsigned short
		pp:1, //is token part of a preprocessor line
		pp_directive:1; //does token follow a TOK_LEADING_POUND (e.g. # include)
};

struct token {
	struct token *prev, *next;
	
	struct token_flags flags;
	short type; //enum token_type
	union {
		struct tok_integer integer;
		struct tok_floating floating;
		int opkw; //operator or keyword ID (e.g. '+', INC_OP (++), ADD_ASSIGN (+=))
		darray_char *string; //applies to TOK_CHAR and TOK_STRING
		char *include; //applies to TOK_STRING_IQUOTE and TOK_STRING_IANGLE
	};
	
	//text this token represents (with backslash-broken lines merged)
	const char *txt;
	size_t txt_size;
	
	//text this token represents (untouched)
	const char *orig;
	size_t orig_size;
	
	//zero-based line and column number of this token
	size_t line, col;
};

//keywords such as int, long, etc. may be defined over, making them identifiers in a sense
static inline int token_is_identifier(const struct token *tok) {
	return token_type_is_identifier(tok->type);
}

static inline int token_is_ignored(const struct token *tok) {
	return token_type_is_ignored(tok->type);
}

static inline int token_is_op(const struct token *tok, int opkw) {
	return tok->type==TOK_OPERATOR && tok->opkw==opkw;
}

static inline int token_is_kw(const struct token *tok, int opkw) {
	return tok->type==TOK_KEYWORD && tok->opkw==opkw;
}

static inline int token_txt_is(const struct token *tok, const char *str) {
	size_t len = strlen(str);
	return tok->txt_size==len && !memcmp(tok->txt, str, len);
}

struct token_list {
	struct token *first, *last;
	
	//Points to original input as given
	const char *orig;
	size_t orig_size;
	
	//position of the start of each real line with respect to orig
	const char * const *olines;
	size_t olines_size;
	
	//Copy of original input without backslash-broken lines
	const char *txt;
	size_t txt_size;
	
	//position of the start of each real line with respect to txt
	const char * const *tlines;
	size_t tlines_size;
	
	//Set me so tok_message_print will know what file name to display
	const char *filename;
};

extern struct dict *tokenizer_dict;

typedef queue(struct tok_message) tok_message_queue;

//the token_list is allocated as a child of tcontext
struct token_list *tokenize(const void *tcontext, const char *orig, size_t orig_size, tok_message_queue *mq);

size_t token_list_count(const struct token_list *tl);

//used for debugging
int token_list_sanity_check(const struct token_list *tl, FILE *err);
void token_list_dump(const struct token_list *tl, FILE *f);

/* tok_point_lookup is used to locate a pointer that is within a token list's
   txt or orig fields */

struct tok_point {
	const char *txt, *orig;
	size_t line, col;
};

//returns nonzero if the pointer could be resolved
int tok_point_lookup(struct tok_point *out, const char *ptr,
			const struct token_list *tl);


/* Tokenizer message queue; used to gather and report warnings, errors, etc. */

enum tok_message_level {TM_DEBUG, TM_INFO, TM_WARN, TM_ERROR, TM_BUG};

struct tok_message {
	enum tok_message_level level;
	const char *path;
		//Unique slash-delimited name of the message
		//e.g. tokenize/read_cstring/ambiguous_octal
	const char *message;
		//Human-readable description
		//e.g. `Octal \007 followed by digit`
	const char *location;
		//Pointer (typically within the token list's txt or orig) of the error
};

#define tok_msg_debug(name, loc, fmt, ...) tok_message_add(mq, TM_DEBUG, MESSAGE_PATH #name, loc, fmt, ##__VA_ARGS__)
#define tok_msg_info(name, loc, fmt, ...) tok_message_add(mq, TM_INFO, MESSAGE_PATH #name, loc, fmt, ##__VA_ARGS__)
#define tok_msg_warn(name, loc, fmt, ...) tok_message_add(mq, TM_WARN, MESSAGE_PATH #name, loc, fmt, ##__VA_ARGS__)
#define tok_msg_error(name, loc, fmt, ...) tok_message_add(mq, TM_ERROR, MESSAGE_PATH #name, loc, fmt, ##__VA_ARGS__)
#define tok_msg_bug(name, loc, fmt, ...) tok_message_add(mq, TM_BUG, MESSAGE_PATH #name, loc, fmt, ##__VA_ARGS__)

void tok_message_add(tok_message_queue *mq, enum tok_message_level level,
	const char *path, const char *loc, const char *fmt, ...);

void tok_message_print(struct tok_message *m, struct token_list *tl);

void tok_message_dump(struct tok_message *m);
void tok_message_queue_dump(const tok_message_queue *mq);


/* Miscellaneous internal components */

char *read_cstring(darray_char *out, const char *s, const char *e, char quoteChar, tok_message_queue *mq);
char *read_cnumber(struct token *tok, const char *s, const char *e, tok_message_queue *mq);


typedef unsigned int readui_base;

#define READUI_ALLOWHIGHERDIGITS 256
#define READUI_ALLOWCAPLETTERS 512
#define READUI_ALLOWLCASELETTERS 1024
#define READUI_ALLOWLETTERS (READUI_ALLOWCAPLETTERS | READUI_ALLOWLCASELETTERS)

#define READUI_DEC      ((readui_base)(10))
#define READUI_HEX      ((readui_base)(16 | READUI_ALLOWLETTERS))
#define READUI_OCT      ((readui_base)(8))
#define READUI_BIN      ((readui_base)(2))

uint64_t readui(const char **sp, const char *e, readui_base base);

#endif
