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

#include <ccan/ccan_tokenizer/read_cnumber.c>
#include <ccan/ccan_tokenizer/read_cstring.c>
#include <ccan/ccan_tokenizer/dict.c>
#include <ccan/ccan_tokenizer/ccan_tokenizer.c>
#include <ccan/ccan_tokenizer/queue.c>
#include <ccan/ccan_tokenizer/charflag.c>

#include <ccan/ccan_tokenizer/ccan_tokenizer.h>

#include <ccan/tap/tap.h>

#include <math.h>

#define array_count_pair(type, ...) (const type []){__VA_ARGS__}, sizeof((const type []){__VA_ARGS__})/sizeof(type)

static void test_read_cstring(void) {
	#define next() do {darray_free(str); darray_init(str); csp++;} while(0)
	#define cs (*csp)
	#define verify_quotechar(correct, correct_continuation_offset, quotechar) do { \
		const size_t s = sizeof(correct)-1; \
		p = read_cstring(&str, cs, cs ? strchr(cs, 0) : NULL, quotechar, &mq); \
		ok(str.size==s && str.alloc>s && str.item[s]==0 && \
		!memcmp(str.item, correct, s), \
		"\"%s: Is output correct?", cs); \
		ok(p == cs+correct_continuation_offset, "\"%s: Is continuation pointer correct?", cs); \
		next(); \
	} while(0)
	#define verify(correct, correct_continuation_offset) verify_quotechar(correct, correct_continuation_offset, '"')
	
	const char * const cstrings[] = {
		NULL,
		"",
		"\"",
		"Hello world!\"",
		"Hello world!",
		"\\\\\\f\\e\\b\\0\\a\\r\\n\\w\\t\\v\\\'\\\"\"",
		"\\\\\\f\\e\\b\\0\\a\\r\\n\\w\\t\\v\\\'\\\"\'",
		"الأدب العربي\"",
		"Ends with \\",
		"Tab: '\\011' Space: '\\040' Overflow: '\\777' Ambiguous: '\\1013'\"",
		"\\x50\\x35\\x12\\xEF\\xFE\\x00012\\x345\""
	};
	const char * const *csp = cstrings;
	const char *p;
	darray_char str = darray_new();
	tok_message_queue mq;
	
	queue_init(mq, NULL);
	
	//check null input
	verify("", 0);
	
	//Check an empty input
	verify("", 0);
	
	//Check an empty quote-terminated string
	verify("", 0);
	
	//Check a simple string
	verify("Hello world!", 12);
	
	//Check a simple string without an end quote
	verify("Hello world!", 12);
	
	//Check a collection of single-character sequences
	verify("\\\f\e\b\0\a\r\nw\t\v\'\"", 26);
	
	//Check same collection of single-character sequences, this time using a single quote terminator
	verify_quotechar("\\\f\e\b\0\a\r\nw\t\v\'\"", 26, '\'');
	
	//Check a real UTF-8 string
	verify("\xd8\xa7\xd9\x84\xd8\xa3\xd8\xaf\xd8\xa8\x20\xd8\xa7\xd9\x84\xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a", 23);
	
	//Check string ending in backslash
	verify("Ends with \\", 11);
	
	//Check a series of octal escapes
	verify("Tab: '\t' Space: ' ' Overflow: '\377' Ambiguous: 'A3'", 61);
	
	//Check a series of hex escapes
	verify("\x50\x35\x12\xEF\xFE\x12\x45", 32);
	
	darray_free(str);
	
	//tok_message_queue_dump(&mq);
	
	//Verify the message queue
	if (1)
	{
		struct tok_message m;
		struct tok_message correct_messages[] = {
			{.level=TM_ERROR, .path="tokenize/read_cstring/missing_endquote"},
			{.level=TM_ERROR, .path="tokenize/read_cstring/missing_endquote"},
			{.level=TM_ERROR, .path="tokenize/read_cstring/missing_endquote"},
			{.level=TM_WARN, .path="tokenize/read_cstring/unknown_escape"},
			//{.level=TM_INFO, .path="tokenize/read_cstring/escaped_single_quote"},
			{.level=TM_WARN, .path="tokenize/read_cstring/unknown_escape"},
			//{.level=TM_INFO, .path="tokenize/read_cstring/escaped_double_quote"},
			{.level=TM_ERROR, .path="tokenize/read_cstring/ended_in_backslash"},
			{.level=TM_ERROR, .path="tokenize/read_cstring/missing_endquote"},
			{.level=TM_WARN, .path="tokenize/read_cstring/octal_overflow"},
			{.level=TM_INFO, .path="tokenize/read_cstring/ambiguous_octal"},
			{.level=TM_WARN, .path="tokenize/read_cstring/ambiguous_hex"},
			{.level=TM_WARN, .path="tokenize/read_cstring/ambiguous_hex"},
			{.level=TM_WARN, .path="tokenize/read_cstring/hex_overflow"},
		};
		size_t i, e=sizeof(correct_messages)/sizeof(*correct_messages);
		
		while(queue_count(mq) && queue_next(mq).level==TM_DEBUG)
			queue_skip(mq);
		for (i=0; i<e; i++) {
			if (!queue_count(mq))
				break;
			m = dequeue(mq);
			if (m.level != correct_messages[i].level)
				break;
			if (strcmp(m.path, correct_messages[i].path))
				break;
			while(queue_count(mq) && queue_next(mq).level==TM_DEBUG)
				queue_skip(mq);
		}
		if (i<e)
			printf("Item %zu is incorrect\n", i);
		ok(i==e, "Is message queue correct?");
		ok(!queue_count(mq), "Message queue should be empty now.");
	}
	
	queue_free(mq);
	#undef next
	#undef cs
	#undef verify_quotechar
	#undef verify
}

#if 0
static void p(const char *str) {
	if (str)
		puts(str);
	else
		puts("(null)");
}
#endif

static void test_queue(void) {
	#define next() do {queue_free(q); queue_init(q, NULL);} while(0)
	
	const char * const s[] = {
		"zero",
		"one",
		"two",
		"three",
		"four",
		"five",
		"six",
		"seven",
		"eight",
		"nine",
		"ten",
		"eleven",
		"twelve",
		"thirteen",
		"fourteen",
		"fifteen"
	};
	queue(const char*) q;
	queue_init(q, NULL);
	
	enqueue(q, s[0]);
	enqueue(q, s[1]);
	enqueue(q, s[2]);
	enqueue(q, s[3]);
	enqueue(q, s[4]);
	enqueue(q, s[5]);
	ok(queue_count(q) == 6, "Checking queue count");
	
	ok(dequeue_check(q)==s[0] &&
		dequeue_check(q)==s[1] &&
		dequeue_check(q)==s[2], "Dequeuing/checking 3 items");
	ok(queue_count(q) == 3, "Checking queue count");
	
	enqueue(q, s[6]);
	enqueue(q, s[7]);
	enqueue(q, s[8]);
	enqueue(q, s[9]);
	enqueue(q, s[10]);
	enqueue(q, s[11]);
	enqueue(q, s[12]);
	enqueue(q, s[13]);
	enqueue(q, s[14]);
	enqueue(q, s[15]);
	ok(queue_count(q) == 13, "Checking queue count");
	
	ok(dequeue_check(q)==s[3] &&
		dequeue_check(q)==s[4] &&
		dequeue_check(q)==s[5] &&
		dequeue_check(q)==s[6] &&
		dequeue_check(q)==s[7] &&
		dequeue_check(q)==s[8] &&
		dequeue_check(q)==s[9] &&
		dequeue_check(q)==s[10] &&
		dequeue_check(q)==s[11] &&
		dequeue_check(q)==s[12] &&
		dequeue_check(q)==s[13] &&
		dequeue_check(q)==s[14] &&
		dequeue_check(q)==s[15], "Are queue items correct?");
	ok(dequeue_check(q)==NULL && dequeue_check(q)==NULL && queue_count(q)==0, "Does queue run out correctly?");
	
	queue_free(q);
	
	#undef next
}

#define test_dict_single() _test_dict_single(dict, str, sizeof(str)-1, correct, sizeof(correct)/sizeof(*correct))
static void _test_dict_single(struct dict *dict, const char *str, size_t len, int *correct, size_t correct_count) {
	const char *s=str, *e=str+len;
	size_t i;
	struct dict_entry *entry;
	
	for (i=0; s<e && i<correct_count; i++) {
		const char *s_last = s;
		entry = dict_lookup(dict, &s, e);
		if (!entry) {
			if (s_last != s)
				break; //dict_lookup should not modify *sp when it returns NULL
			s++;
			if (correct[i] != -100)
				break;
			continue;
		}
		if (correct[i] != entry->id)
			break;
		if (!*entry->str) {
			if (s_last+1 != s)
				break;
			if (s[-1] != 0)
				break;
		} else {
			size_t len = strlen(entry->str);
			if (s_last+len != s)
				break;
			if (strncmp(entry->str, s-len, len))
				break;
		}
		//printf("Correctly read %s\n", entry->str);
	}
	
	if (s!=e || i!=correct_count) {
		printf("Tokenization failed at ");
		fwrite(s, 1, e-s, stdout);
		printf("\n");
	}
	
	ok(s==e && i==correct_count, "All of the tokens are correct");
}

static void test_dict(void) {
	struct dict_entry dict_orig[] = {
		{-1, ""},
		{0, " "},
		{1, "it"},
		{2, "it's"},
		{3, "a"},
		{4, "beautiful"},
		{5, "be"},
		{6, "day"},
		{7, "d"},
		{8, "in"},
		{9, "the"},
		{10, "t"},
		{11, "neighborhood"},
		{12, "neighbor"},
		{13, "won't"},
		{14, " you"},
		{15, "my"},
		{16, "??"},
		{17, "item"},
		{18, "ip"},
		{19, "\xFF\xFA"},
		{20, "\xFF\xEE"},
		{21, "\x80\x12\x34"},
		{22, "\x80\x32"},
		{23, "\x80\x32\x34"}
	};
	struct dict *dict = dict_build(NULL, dict_orig, sizeof(dict_orig)/sizeof(*dict_orig));
	
	{
		const char *s=NULL, *e=NULL;
		ok(dict_lookup(dict, &s, e)==NULL && s==NULL && e==NULL, "dict_lookup does nothing and returns null on empty input");
	}
	
	{
		const char str[] = "it's a beautiful day in the neighborhood\0won't you be my neighbor?";
		int correct[] = {2,0, 3,0, 4,0, 6,0, 8,0, 9,0, 11,-1, 13, 14,0, 5,0, 15,0, 12, -100};
		test_dict_single();
	}
	
	//check equal-length tokens
	{
		const char str[] = "it'sitem initip";
		int correct[] = {2,17,0, 8,1,18};
		test_dict_single();
	}
	
	//check mostly invalid tokens
	{
		const char str[] = "&^&beaumx yo youthx";
		int correct[] = {-100,-100,-100, 5,3,-100,-100,-100, 0,-100,-100, 14,10,-100,-100};
		test_dict_single();
	}
	
	//check tokens that start with a character greater than 0x7F
	{
		const char str[] = "\x80\x12\x34\x80\x32\x80\x32\x34\xFF\xFA\xFF\xEE";
		int correct[] = {21, 22, 23, 19, 20};
		test_dict_single();
	}
	
	talloc_free(dict);
	
	//make sure dict_build doesn't blow up on an empty dictionary
	dict = dict_build(NULL, NULL, 0);
	talloc_free(dict);
}

static void test_charflag(void) {
	char i;
	int correct = 0;
	
	#define CONTROL do { \
		if (ccontrol(i) && !cspace(i) && !creturn(i) && !cwhite(i) && \
			!cdigit(i) && !cletter(i) && !chex(i) && !csymbol(i) && \
			!cextended(i) ) \
			correct++; \
		} while(0)
	#define SPACE do { \
		if (!ccontrol(i) && cspace(i) && !creturn(i) && cwhite(i) && \
			!cdigit(i) && !cletter(i) && !chex(i) && !csymbol(i) && \
			!cextended(i) ) \
			correct++; \
		} while(0)
	#define RETURN do { \
		if (!ccontrol(i) && !cspace(i) && creturn(i) && cwhite(i) && \
			!cdigit(i) && !cletter(i) && !chex(i) && !csymbol(i) && \
			!cextended(i) ) \
			correct++; \
		} while(0)
	#define SYMBOL do { \
		if (!ccontrol(i) && !cspace(i) && !creturn(i) && !cwhite(i) && \
			!cdigit(i) && !cletter(i) && !chex(i) && csymbol(i) && \
			!cextended(i) ) \
			correct++; \
		} while(0)
	#define DIGIT do { \
		if (!ccontrol(i) && !cspace(i) && !creturn(i) && !cwhite(i) && \
			cdigit(i) && !cletter(i) && chex(i) && !csymbol(i) && \
			!cextended(i) ) \
			correct++; \
		} while(0)
	#define LETTER_HEX do { \
		if (!ccontrol(i) && !cspace(i) && !creturn(i) && !cwhite(i) && \
			!cdigit(i) && cletter(i) && chex(i) && !csymbol(i) && \
			!cextended(i) ) \
			correct++; \
		} while(0)
	#define LETTER do { \
		if (!ccontrol(i) && !cspace(i) && !creturn(i) && !cwhite(i) && \
			!cdigit(i) && cletter(i) && !chex(i) && !csymbol(i) && \
			!cextended(i) ) \
			correct++; \
		} while(0)
	#define EXTENDED do { \
		if (!ccontrol(i) && !cspace(i) && !creturn(i) && !cwhite(i) && \
			!cdigit(i) && !cletter(i) && !chex(i) && !csymbol(i) && \
			cextended(i) ) \
			correct++; \
		} while(0)
	
	for (i=0; i<'\t'; i++) CONTROL;
	i = '\t'; SPACE;
	i = '\n'; RETURN;
	i = '\v'; SPACE;
	i = '\f'; SPACE;
	i = '\r'; RETURN;
	for (i='\r'+1; i<' '; i++) CONTROL;
	i = ' '; SPACE;
	for (i='!'; i<='/'; i++) SYMBOL;
	for (i='0'; i<='9'; i++) DIGIT;
	for (i=':'; i<='@'; i++) SYMBOL;
	for (i='A'; i<='F'; i++) LETTER_HEX;
	for (i='G'; i<='Z'; i++) LETTER;
	for (i='['; i<='`'; i++) SYMBOL;
	for (i='a'; i<='f'; i++) LETTER_HEX;
	for (i='g'; i<='z'; i++) LETTER;
	for (i='{'; i<='~'; i++) SYMBOL;
	i = '\x7F'; CONTROL;
	
	ok(correct==128, "ASCII characters have correct charflags");
	correct = 0;
	
	//We do some goofy stuff here to make sure sign extension doesn't cause problems with charflags
	{
		unsigned int ui;
		int si;
		
		for (ui=128; ui<=255; ui++) {
			i = ui;
			EXTENDED;
		}
		for (si=-128; si<0; si++) {
			i = si;
			EXTENDED;
		}
	}
	{
		int i;
		for (i=-128; i<0; i++) EXTENDED;
	}
	{
		unsigned int i;
		for (i=128; i<=255; i++) EXTENDED;
	}
	
	ok(correct==512, "Extended characters have correct charflags");
	
	#undef CONTROL
	#undef SPACE
	#undef RETURN
	#undef SYMBOL
	#undef DIGIT
	#undef LETTER_HEX
	#undef LETTER
	#undef EXTENDED
}

struct readui_test {
	const char *txt;
	size_t txt_size;
	readui_base base;
	
	uint64_t correct_integer;
	int correct_errno;
	size_t correct_advance;
};

#define T(txt, ...) {txt, sizeof(txt)-1, __VA_ARGS__}
#define M (18446744073709551615ULL)

struct readui_test readui_tests[] = {
	//Basic reads
	T("0",READUI_DEC, 0,0,1),
	T(" \t42  ",READUI_DEC, 42,0,4),
	
	//Different bases
	T("BADBEEFDEADBAT",READUI_HEX, 0xBADBEEFDEADBAULL,0,13),
	T("7559",READUI_OCT, 0755,0,3),
	T("01010010110012",READUI_BIN, 2649,0,13),
	T("1000000000",0x7F, 8594754748609397887ULL,0,10),
	
	//Errors
	T("",READUI_DEC, 0,EINVAL,0),
	T("18446744073709551616",
		READUI_DEC,M,ERANGE,20),
	T("1000000000000000000000000",
		READUI_DEC,M,ERANGE,25),
	T("10000000000000000",
		READUI_HEX,M,ERANGE,17),
	T("10000000000000000000000000000000000000000000000000000000000000000",
		READUI_BIN,M,ERANGE,65),
	T("10000000000",
		0x7D,M,ERANGE,11),
	T("9000000000",0x7F, M,ERANGE,10),
	
	//Misc
	T("18446744073709551615",READUI_DEC, M,0,20),
};

static void test_readui_single(struct readui_test *test) {
	uint64_t result_integer;
	int result_errno;
	size_t result_advance;
	
	const char *s = test->txt, *e = s+test->txt_size;
	errno = 0;
	result_integer = readui(&s, e, test->base);
	result_errno = errno;
	result_advance = s-test->txt;
	
	ok(result_integer == test->correct_integer &&
	   result_errno   == test->correct_errno &&
	   result_advance == test->correct_advance,
	   "Testing \"%s\"", test->txt);
}

static void test_readui(void) {
	size_t i, count = sizeof(readui_tests)/sizeof(*readui_tests);
	
	for (i=0; i<count; i++)
		test_readui_single(readui_tests+i);
}

#undef T
#undef M

static void scan_number_sanity_check(const struct scan_number *sn,
		enum token_type type, const char *str_pipes, const char *msg) {
	//If there is a prefix, it should follow
	//the pattern (0 [B X b x]*0..1)
	if (sn->prefix < sn->digits) {
		int len = sn->digits - sn->prefix;
		if (len!=1 && len!=2) {
			fail("%s : Prefix length is %d; should be 1 or 2",
				str_pipes, len);
			return;
		}
		if (sn->prefix[0] != '0') {
			fail("%s : Prefix does not start with 0",
				str_pipes);
			return;
		}
		if (len==2 && !strchr("BXbx", sn->prefix[1])) {
			fail("%s : Prefix is 0%c; should be 0, 0b, or 0x",
				str_pipes, sn->prefix[1]);
			return;
		}
		if (len==1 && type==TOK_FLOATING) {
			fail("%s : Octal prefix appears on floating point number",
				str_pipes);
			return;
		}
	} else {
	//if there is no prefix, the first digit should not be 0
	//  unless this is a floating point number
		if (sn->digits < sn->exponent && sn->digits[0]=='0' &&
				type==TOK_INTEGER) {
			fail("%s : First digit of non-prefix integer is 0",
				str_pipes);
			return;
		}
	}
	
	//Make sure sn->digits contains valid digits and is not empty
	//  (unless prefix is "0")
	{
		const char *s = sn->digits, *e = sn->exponent;
		if (sn->prefix+1 < sn->digits) {
			if (s >= e) {
				fail("%s : 0%c not followed by any digits",
					str_pipes, sn->prefix[1]);
				return;
			}
			if (sn->prefix[1] == 'X' || sn->prefix[1] == 'x') {
				while (s<e && strchr(
					"0123456789ABCDEFabcdef.", *s)) s++;
			} else {
				if (s[0]!='0' && s[0]!='1') {
					fail("%s: Binary prefix not followed by a 0 or 1",
						str_pipes);
					return;
				}
				while (s<e && strchr(
					"0123456789.", *s)) s++;
			}
		} else {
			if (type==TOK_FLOATING && s >= e) {
				fail("%s : sn->digits is empty in a floating point number",
					str_pipes);
				return;
			}
			if (sn->prefix >= sn->digits && s >= e) {
				fail("%s : both sn->prefix and sn->digits are empty",
					str_pipes);
				return;
			}
			while (s<e && strchr("0123456789.", *s)) s++;
		}
		if (s != e) {
			fail("%s : sn->digits is not entirely valid", str_pipes);
			return;
		}
	}
	
	//Make sure exponent follows the rules
	if (sn->exponent < sn->suffix) {
		char c = sn->exponent[0];
		if (type==TOK_INTEGER) {
			fail("%s : sn->exponent is not empty in an integer", str_pipes);
			return;
		}
		if (sn->prefix < sn->digits && (c=='E' || c=='e')) {
			fail("%s : Exponent for hex/binary starts with %c", str_pipes, c);
			return;
		}
		if (sn->prefix >= sn->digits && (c=='P' || c=='p')) {
			fail("%s : Exponent for decimal starts with %c", str_pipes, c);
			return;
		}
	}
	
	pass("%s%s", str_pipes, msg);
	return;
}

static void test_scan_number_single(const char *str_pipes,
				enum token_type type, size_t dots_found) {
	char *str = malloc(strlen(str_pipes)+1);
	const char *expected[5];
	struct scan_number sn;
	enum token_type given_type;
	
	{
		const char *s = str_pipes;
		char *d = str;
		size_t pipes = 0;
		
		expected[0] = d;
		for (;*s;s++) {
			if (*s == ' ')
				continue;
			if (*s == '|') {
				if (++pipes > 4)
					goto fail_too_many_pipes;
				expected[pipes] = d;
			} else
				*d++ = *s;
		}
		*d = 0;
		
		if (pipes < 3)
			goto fail_not_enough_pipes;
		if (pipes == 3)
			expected[4] = d;
	}
	
	given_type = scan_number(&sn, str, strchr(str,0));
	
	if (sn.prefix != expected[0]) {
		fail("%s : sn.prefix is wrong", str_pipes);
		return;
	}
	if (sn.digits != expected[1]) {
		fail("%s : sn.digits is wrong", str_pipes);
		return;
	}
	if (sn.exponent != expected[2]) {
		fail("%s : sn.exponent is wrong", str_pipes);
		return;
	}
	if (sn.suffix != expected[3]) {
		fail("%s : sn.suffix is wrong", str_pipes);
		return;
	}
	if (sn.end != expected[4]) {
		fail("%s : sn.end is wrong", str_pipes);
		return;
	}
	if (given_type != type) {
		fail("%s : Type incorrect", str_pipes);
		return;
	}
	if (sn.dots_found != dots_found) {
		fail("%s : sn.dots_found is %zu; should be %zu", str_pipes,
			sn.dots_found, dots_found);
		return;
	}
	
	scan_number_sanity_check(&sn, type, str_pipes, "");
	
	free(str);
	return;
	
fail_too_many_pipes:
	fail("Too many pipes in the test string \"%s\"; should be 3", str_pipes);
	return;
fail_not_enough_pipes:
	fail("Not enough pipes in the test string \"%s\"; should be 3", str_pipes);
	return;
}

#define T(str, type, dots_found) test_scan_number_single(str,type,dots_found)

static void test_scan_number(void) {
	T("0x | 50.1 | p+1 | f", TOK_FLOATING, 1);
	T("| 100 || L", TOK_INTEGER, 0);
	T("0 ||| b21", TOK_INTEGER, 0);
	T("0b | 101 || L", TOK_INTEGER, 0);
	T("0X | 7Af ||| \t2", TOK_INTEGER, 0);
	T("0|||b", TOK_INTEGER, 0);
	T("0|||x", TOK_INTEGER, 0);
}

#undef T

#define T(string, value, theBase, theSuffix) do { \
	queue_init(mq, NULL); \
	str = (string); \
	type = scan_number(&sn, str, str+sizeof(string)-1); \
	ok(type==TOK_INTEGER, "%s : type==TOK_INTEGER", str); \
	scan_number_sanity_check(&sn, type, str, \
		" : scan_number_sanity_check passed"); \
	read_integer(&integer, &sn, &mq); \
	ok(integer.v==(value) && integer.base==(theBase) && \
		integer.suffix==(theSuffix), \
		"%s : Correct value and suffix", str); \
	} while(0)
#define Q(name) do { \
	if (queue_count(mq)) { \
		const char *path = dequeue(mq).path; \
		ok(!strcmp(path, "tokenize/read_cnumber/" #name), \
			"%s : Dequeued %s", str, path); \
	} \
	} while(0)
#define E() do { \
	ok(queue_count(mq)==0, "%s : Message queue empty", str); \
	if (queue_count(mq)) \
		tok_message_queue_dump(&mq); \
	queue_free(mq); \
	} while(0)

static void test_read_integer(void) {
	struct scan_number sn;
	tok_message_queue mq;
	const char *str;
	enum token_type type;
	struct tok_integer integer;
	
	T("0b0lu", 0, 8, TOK_UL);
	E();
	
	T("1", 1, 10, TOK_NOSUFFIX);
	E();
	
	T("32Q", 32, 10, TOK_NOSUFFIX);
	Q(integer_suffix_invalid);
	E();
	
	T("32i", 32, 10, TOK_I);
	E();
	
	T("0755f", 493, 8, TOK_NOSUFFIX);
	Q(suffix_float_only);
	E();
	
	T("0xDeadBeef", 0xDEADBEEF, 16, TOK_NOSUFFIX);
	E();
	
	T("12345678901234567890$1_LONG.SUFFIX", 12345678901234567890ULL, 10, TOK_NOSUFFIX);
	ok1(sn.end == strchr(str, 0));
	Q(integer_suffix_invalid);
	E();
	
	T("0xDEADBEEFlull", 0xDEADBEEF, 16, TOK_NOSUFFIX);
	Q(integer_suffix_invalid);
	E();
	
	T("0xBALLuu", 0xBA, 16, TOK_NOSUFFIX);
	Q(integer_suffix_invalid);
	E();
	
	T("123456789012345678901", 18446744073709551615ULL, 10, TOK_NOSUFFIX);
	Q(integer_out_of_range);
	E();
	
	T("09", 0, 8, TOK_NOSUFFIX);
	Q(integer_invalid_digits);
	E();
}

#undef T
#undef E

#define Teq(string, equals, theSuffix) do { \
	queue_init(mq, NULL); \
	str = malloc(sizeof(string)); \
	memcpy(str, string, sizeof(string)); \
	type = scan_number(&sn, str, str+sizeof(string)-1); \
	ok(type==TOK_FLOATING, "%s : type==TOK_FLOATING", str); \
	scan_number_sanity_check(&sn, type, str, \
		" : scan_number_sanity_check passed"); \
	read_floating(&floating, &sn, &mq); \
	ok((equals) && \
		floating.suffix==(theSuffix), \
		"%s : Correct value and suffix", str); \
	} while(0)
#define T(string, value, theSuffix) \
	Teq(string, fabsl(floating.v - (value)) <= 0.00000000000000001, theSuffix)
#define E() do { \
	ok(queue_count(mq)==0, "%s : Message queue empty", str); \
	if (queue_count(mq)) \
		tok_message_queue_dump(&mq); \
	queue_free(mq); \
	free(str); \
	} while(0)

static void test_read_floating(void) {
	struct scan_number sn;
	tok_message_queue mq;
	char *str; //str is a malloced copy so read_floating can do its null terminator trick
	enum token_type type;
	struct tok_floating floating;
	
	T("1.0", 1.0, TOK_NOSUFFIX);
	E();
	
	T("0.0", 0.0, TOK_NOSUFFIX);
	E();
	
	T("0755e1", 7550.0, TOK_NOSUFFIX);
	E();
	
	T("0xD.Bp0", 0xD.Bp0, TOK_NOSUFFIX);
	E();
	
	//GCC doesn't throw any errors or warnings for this odd case,
	//but we call it an error to be consistent with strtold
	T("0x.p0", 0.0, TOK_NOSUFFIX);
	Q(floating_invalid_digits);
	E();
	
	T("32.0Q", 32.0, TOK_NOSUFFIX);
	Q(floating_suffix_invalid);
	E();
	
	T("32.0Li", 32.0, TOK_IMAG_L);
	E();
	
	T("32.0LL", 32.0, TOK_NOSUFFIX);
	Q(suffix_integer_only);
	E();
	
	Teq("0xDEAD.BEEF", floating.v==0.0, TOK_NOSUFFIX);
	Q(hex_float_no_exponent);
	E();
	
	T("0b101.0p0", 0, TOK_NOSUFFIX);
	Q(binary_float);
	E();
	
	/* If any of the following three tests fails, consider increasing
	   the e+ and e- values. */
	
	Teq("1.e+4933", isinf(floating.v), TOK_NOSUFFIX);
	Q(floating_out_of_range);
	E();
	
	/* for some reason, strtold sets errno=EDOM on x86, and
	   on my PowerPC G4 on Fedora 10, the same phenomenon occurs
	   but the exponents are e+309, e-324, and e-325 */
	Teq("1.e-4951", floating.v==0.0, TOK_NOSUFFIX);
	Q(floating_out_of_range);
	E();
	
	Teq("1.e-4952", floating.v==0.0, TOK_NOSUFFIX);
	Q(floating_out_of_range);
	E();
	
}

#undef Teq
#undef T
#undef Q
#undef E

struct tokenizer_test {
	const char *txt;
	size_t txt_size;
	
	const struct token *tokens;
	size_t token_count;
};

#define T(txt, ...) {txt, sizeof(txt)-1, array_count_pair(struct token, __VA_ARGS__)}
#define string(txt) {.string=(darray_char[1]){{.item = (char *)(txt), .size = sizeof(txt)-1}}}
#define opkw(v) {.opkw = (v)}
#define txt(t) .txt = (t), .txt_size = sizeof(t)-1
#define integer(...) {.integer={__VA_ARGS__}}
#define floating(...) {.floating={__VA_ARGS__}}
#define space {.type = TOK_WHITE, .txt = " ", .txt_size = 1}
#define startline {.type = TOK_STARTLINE}
#define include(str) {.include = (char *)(str)}

struct tokenizer_msg_test {
	struct tokenizer_test test;
	
	const char * const *messages;
	size_t message_count;
};

#define M(...) array_count_pair(const char *, __VA_ARGS__)

struct tokenizer_test tokenizer_tests[] = {
	{ "", 0, 0 },
	T("\n",
		{.type = TOK_WHITE, txt("\n")}
	),
	T("\na",
		{.type = TOK_WHITE, txt("\n")},
		startline,
		{.type = TOK_IDENTIFIER, txt("a")}
	),
	T("int n = c++;",
		{.type = TOK_KEYWORD,
			opkw(INT),
			txt("int")
		}, space,
		{.type = TOK_IDENTIFIER,
			txt("n")
		}, space,
		{.type = TOK_OPERATOR,
			opkw('='),
			txt("=")
		}, space,
		{.type = TOK_IDENTIFIER,
			txt("c")
		},
		{.type = TOK_OPERATOR,
			opkw(INC_OP),
			txt("++")
		},
		{.type = TOK_OPERATOR,
			opkw(';'),
			txt(";")
		}
	),
	T(".5 42 ",
		{.type = TOK_FLOATING,
			floating(.5, TOK_NOSUFFIX),
			txt(".5")
		}, space,
		{.type = TOK_INTEGER,
			integer(42, 10, TOK_NOSUFFIX),
			txt("42")
		}, space,
	),
	//Make sure TOK_STRAY doesn't take over the universe
	T("``AS IS'' AND",
		{.type = TOK_STRAY,
			txt("``")
		},
		{.type = TOK_IDENTIFIER,
			txt("AS")
		}, space,
		{.type = TOK_IDENTIFIER,
			txt("IS")
		},
		{.type = TOK_CHAR,
			string(""),
			txt("\'\'")
		}, space,
		{.type = TOK_IDENTIFIER,
			txt("AND")
		}
	),
	//Make sure starting with 0 doesn't result in skipping whitespace
	T("0 .05 0 500",
		{.type = TOK_INTEGER,
			integer(0, 8, TOK_NOSUFFIX),
			txt("0")
		}, space,
		{.type = TOK_FLOATING,
			floating(.05, TOK_NOSUFFIX),
			txt(".05")
		}, space,
		{.type = TOK_INTEGER,
			integer(0, 8, TOK_NOSUFFIX),
			txt("0")
		}, space,
		{.type = TOK_INTEGER,
			integer(500, 10, TOK_NOSUFFIX),
			txt("500")
		}
	),
	//Make sure a simple preprocessor directive works
	T("\t/*comment*/ #include \"include.h\"\n",
		{.flags={1,0}, .type=TOK_WHITE, txt("\t")},
		{.flags={1,0}, .type=TOK_CCOMMENT, txt("/*comment*/")},
		{.flags={1,0}, .type=TOK_WHITE, txt(" ")},
		{.flags={1,0}, .type=TOK_LEADING_POUND, txt("#")},
		{.flags={1,1}, .type=TOK_KEYWORD, opkw(INCLUDE), txt("include")},
		{.flags={1,0}, .type=TOK_WHITE, txt(" ")},
		{.flags={1,0}, .type=TOK_STRING_IQUOTE, include("include.h"), txt("\"include.h\"")},
		{.flags={1,0}, .type=TOK_WHITE, txt("\n")}
	),
	//Make sure __VA_ARGS__ is lexed correctly
	T("if #define __VA_ARGS__=0X5FULL;\n"
	  " #define __VA_ARGS__(__VA_ARGS__, ...\t)__VA_ARGS__ bar int define",
		{.type=TOK_KEYWORD, opkw(IF), txt("if")},
		space,
		{.type=TOK_OPERATOR, opkw('#'), txt("#")},
		{.type=TOK_IDENTIFIER, txt("define")},
		space,
		{.type=TOK_IDENTIFIER, txt("__VA_ARGS__")},
		{.type=TOK_OPERATOR, opkw('='), txt("=")},
		{.type=TOK_INTEGER, integer(0x5F,16,TOK_ULL), txt("0X5FULL")},
		{.type=TOK_OPERATOR, opkw(';'), txt(";")},
		{.type=TOK_WHITE, txt("\n")},
		{.flags={1,0}, .type=TOK_STARTLINE},
		{.flags={1,0}, .type=TOK_WHITE, txt(" ")},
		{.flags={1,0}, .type=TOK_LEADING_POUND, txt("#")},
		{.flags={1,1}, .type=TOK_KEYWORD, opkw(DEFINE), txt("define")},
		{.flags={1,0}, .type=TOK_WHITE, txt(" ")},
		{.flags={1,0}, .type=TOK_IDENTIFIER, txt("__VA_ARGS__")},
		{.flags={1,0}, .type=TOK_OPERATOR, opkw('('), txt("(")},
		{.flags={1,0}, .type=TOK_IDENTIFIER, txt("__VA_ARGS__")},
		{.flags={1,0}, .type=TOK_OPERATOR, opkw(','), txt(",")},
		{.flags={1,0}, .type=TOK_WHITE, txt(" ")},
		{.flags={1,0}, .type=TOK_OPERATOR, opkw(ELLIPSIS), txt("...")},
		{.flags={1,0}, .type=TOK_WHITE, txt("\t")},
		{.flags={1,0}, .type=TOK_OPERATOR, opkw(')'), txt(")")},
		{.flags={1,0}, .type=TOK_KEYWORD, opkw(VA_ARGS), txt("__VA_ARGS__")},
		{.flags={1,0}, .type=TOK_WHITE, txt(" ")},
		{.flags={1,0}, .type=TOK_IDENTIFIER, txt("bar")},
		{.flags={1,0}, .type=TOK_WHITE, txt(" ")},
		{.flags={1,0}, .type=TOK_KEYWORD, opkw(INT), txt("int")},
		{.flags={1,0}, .type=TOK_WHITE, txt(" ")},
		{.flags={1,0}, .type=TOK_IDENTIFIER, txt("define")},
	),
	//__VA_ARGS__ is an identifier if no ... operator is in the parameter list or if there is no parameter list
	T("#define foo __VA_ARGS__ bar int define\n#define foo() __VA_ARGS__ bar int define",
		{.flags={1,0}, .type=TOK_LEADING_POUND, txt("#")},
		{.flags={1,1}, .type=TOK_KEYWORD, opkw(DEFINE), txt("define")},
		{.flags={1,0}, .type=TOK_WHITE, txt(" ")},
		{.flags={1,0}, .type=TOK_IDENTIFIER, txt("foo")},
		{.flags={1,0}, .type=TOK_WHITE, txt(" ")},
		{.flags={1,0}, .type=TOK_IDENTIFIER, txt("__VA_ARGS__")},
		{.flags={1,0}, .type=TOK_WHITE, txt(" ")},
		{.flags={1,0}, .type=TOK_IDENTIFIER, txt("bar")},
		{.flags={1,0}, .type=TOK_WHITE, txt(" ")},
		{.flags={1,0}, .type=TOK_KEYWORD, opkw(INT), txt("int")},
		{.flags={1,0}, .type=TOK_WHITE, txt(" ")},
		{.flags={1,0}, .type=TOK_IDENTIFIER, txt("define")},
		{.flags={1,0}, .type=TOK_WHITE, txt("\n")},
		
		{.flags={1,0}, .type=TOK_STARTLINE},
		{.flags={1,0}, .type=TOK_LEADING_POUND, txt("#")},
		{.flags={1,1}, .type=TOK_KEYWORD, opkw(DEFINE), txt("define")},
		{.flags={1,0}, .type=TOK_WHITE, txt(" ")},
		{.flags={1,0}, .type=TOK_IDENTIFIER, txt("foo")},
		{.flags={1,0}, .type=TOK_OPERATOR, opkw('('), txt("(")},
		{.flags={1,0}, .type=TOK_OPERATOR, opkw(')'), txt(")")},
		{.flags={1,0}, .type=TOK_WHITE, txt(" ")},
		{.flags={1,0}, .type=TOK_IDENTIFIER, txt("__VA_ARGS__")},
		{.flags={1,0}, .type=TOK_WHITE, txt(" ")},
		{.flags={1,0}, .type=TOK_IDENTIFIER, txt("bar")},
		{.flags={1,0}, .type=TOK_WHITE, txt(" ")},
		{.flags={1,0}, .type=TOK_KEYWORD, opkw(INT), txt("int")},
		{.flags={1,0}, .type=TOK_WHITE, txt(" ")},
		{.flags={1,0}, .type=TOK_IDENTIFIER, txt("define")}
	),
	
	//Test various integer suffixen
	T("1 1u 1l 1ul 1lu 1ll 1ull 1llu 1U 1L 1UL 1LU 1LL 1ULL 1LLU "
	  "1uq 1lq 1llq 1ulq 1luq 1f 1i",
		{.type=TOK_INTEGER, integer(1, 10, TOK_NOSUFFIX), txt("1")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_U), txt("1u")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_L), txt("1l")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_UL), txt("1ul")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_UL), txt("1lu")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_LL), txt("1ll")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_ULL), txt("1ull")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_ULL), txt("1llu")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_U), txt("1U")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_L), txt("1L")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_UL), txt("1UL")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_UL), txt("1LU")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_LL), txt("1LL")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_ULL), txt("1ULL")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_ULL), txt("1LLU")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_NOSUFFIX), txt("1uq")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_NOSUFFIX), txt("1lq")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_NOSUFFIX), txt("1llq")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_NOSUFFIX), txt("1ulq")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_NOSUFFIX), txt("1luq")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_NOSUFFIX), txt("1f")}, space,
		{.type=TOK_INTEGER, integer(1, 10, TOK_I), txt("1i")}
	),
	//Test non-standard newlines
	T("\n\r\n \r\n\rint",
		{.type=TOK_WHITE, txt("\n\r")}, startline,
		{.type=TOK_WHITE, txt("\n")}, startline,
		space,
		{.type=TOK_WHITE, txt("\r\n")}, startline,
		{.type=TOK_WHITE, txt("\r")}, startline,
		{.type=TOK_KEYWORD, opkw(INT), txt("int")}
	),
	//Test backslash-broken lines
	T("oner\\ \nous",
		{.type=TOK_IDENTIFIER, txt("onerous")}
	),
	T("\\\n\\\n\\\n\\",
		{.type=TOK_STRAY, txt("\\")}
	),
	T("in\\\nt i\\;\nf\\ \r\nor (i=0; i<10; i++) {\\",
		{.type=TOK_KEYWORD, opkw(INT), txt("int")}, space,
		{.type=TOK_IDENTIFIER, txt("i")},
		{.type=TOK_STRAY, txt("\\")},
		{.type=TOK_OPERATOR, opkw(';'), txt(";")},
		{.type=TOK_WHITE, txt("\n")},
		
		startline,
		{.type=TOK_KEYWORD, opkw(FOR), txt("for")}, space,
		{.type=TOK_OPERATOR, opkw('('), txt("(")},
		{.type=TOK_IDENTIFIER, txt("i")},
		{.type=TOK_OPERATOR, opkw('='), txt("=")},
		{.type=TOK_INTEGER, integer(0,8,0), txt("0")},
		{.type=TOK_OPERATOR, opkw(';'), txt(";")}, space,
		{.type=TOK_IDENTIFIER, txt("i")},
		{.type=TOK_OPERATOR, opkw('<'), txt("<")},
		{.type=TOK_INTEGER, integer(10,10,0), txt("10")},
		{.type=TOK_OPERATOR, opkw(';'), txt(";")}, space,
		{.type=TOK_IDENTIFIER, txt("i")},
		{.type=TOK_OPERATOR, opkw(INC_OP), txt("++")},
		{.type=TOK_OPERATOR, opkw(')'), txt(")")}, space,
		{.type=TOK_OPERATOR, opkw('{'), txt("{")},
		{.type=TOK_STRAY, txt("\\")}
	),
	//More preprocessor directive tests
	T("#apple\n#pragma\n#const\n#define \t\n#define foo(x",
		{.flags={1,0}, .type=TOK_LEADING_POUND, txt("#")},
		{.flags={1,1}, .type=TOK_IDENTIFIER, txt("apple")},
		{.flags={1,0}, .type=TOK_WHITE, txt("\n")},
		
		{.flags={1,0}, .type=TOK_STARTLINE},
		{.flags={1,0}, .type=TOK_LEADING_POUND, txt("#")},
		{.flags={1,1}, .type=TOK_KEYWORD, opkw(PRAGMA), txt("pragma")},
		{.flags={1,0}, .type=TOK_WHITE, txt("\n")},
		
		{.flags={1,0}, .type=TOK_STARTLINE},
		{.flags={1,0}, .type=TOK_LEADING_POUND, txt("#")},
		{.flags={1,1}, .type=TOK_IDENTIFIER, txt("const")},
		{.flags={1,0}, .type=TOK_WHITE, txt("\n")},
		
		{.flags={1,0}, .type=TOK_STARTLINE},
		{.flags={1,0}, .type=TOK_LEADING_POUND, txt("#")},
		{.flags={1,1}, .type=TOK_KEYWORD, opkw(DEFINE), txt("define")},
		{.flags={1,0}, .type=TOK_WHITE, txt(" \t")},
		{.flags={1,0}, .type=TOK_WHITE, txt("\n")},
		
		{.flags={1,0}, .type=TOK_STARTLINE},
		{.flags={1,0}, .type=TOK_LEADING_POUND, txt("#")},
		{.flags={1,1}, .type=TOK_KEYWORD, opkw(DEFINE), txt("define")},
		{.flags={1,0}, .type=TOK_WHITE, txt(" ")},
		{.flags={1,0}, .type=TOK_IDENTIFIER, txt("foo")},
		{.flags={1,0}, .type=TOK_OPERATOR, opkw('('), txt("(")},
		{.flags={1,0}, .type=TOK_IDENTIFIER, txt("x")}
	),
	T("#define",
		{.flags={1,0}, .type=TOK_LEADING_POUND, txt("#")},
		{.flags={1,1}, .type=TOK_KEYWORD, opkw(DEFINE), txt("define")}
	),
	T("#define foo",
		{.flags={1,0}, .type=TOK_LEADING_POUND, txt("#")},
		{.flags={1,1}, .type=TOK_KEYWORD, opkw(DEFINE), txt("define")},
		{.flags={1,0}, .type=TOK_WHITE, txt(" ")},
		{.flags={1,0}, .type=TOK_IDENTIFIER, txt("foo")}
	),
	T("`#define foo",
		{.type=TOK_STRAY, txt("`")},
		{.type=TOK_OPERATOR, opkw('#'), txt("#")},
		{.type=TOK_IDENTIFIER, txt("define")},
		space,
		{.type=TOK_IDENTIFIER, txt("foo")}
	)
};

struct tokenizer_msg_test tokenizer_msg_tests[] = {
	{T("/* Unterminated C comment",
		{.type=TOK_CCOMMENT, txt("/* Unterminated C comment")}
	), M(
		"unterminated_comment"
	)},
	{T("\"\n\"\"\n",
		{.type=TOK_STRING, string("\n"), txt("\"\n\"")},
		{.type=TOK_STRING, string("\n"), txt("\"\n")}
	), M(
		"read_cstring/quote_newlines",
		"read_cstring/missing_endquote"
	)},
};

#undef T
#undef string
#undef opkw
#undef txt
#undef integer
#undef floating
#undef M
#undef include

static void test_tokenizer_single(struct tokenizer_test *t, tok_message_queue *mq) {
	struct token_list *tl;
	size_t i, count = t->token_count, gen_count;
	const struct token *tok_gen, *tok_correct;
	int success = 1;
	char *txt = talloc_memdup(NULL, t->txt, t->txt_size);
	size_t txt_size = t->txt_size;
	#define failed(fmt, ...) do { \
		printf("Error: " fmt "\n", ##__VA_ARGS__); \
		success = 0; \
		goto done; \
	} while(0)
	
	tl = tokenize(txt, txt, txt_size, mq);
	
	if (tl->orig != txt || tl->orig_size != txt_size)
		failed("tokenize() did not replicate orig/orig_size from arguments");
	if (!token_list_sanity_check(tl, stdout))
		failed("Sanity check failed");
	
	gen_count = token_list_count(tl);
	if (gen_count != count+1)
		failed("Incorrect number of tokens (%zu, should be %zu)\n",
			gen_count, count+1);
	
	tok_gen = tl->first->next; //skip the beginning TOK_STARTLINE
	tok_correct = t->tokens;
	for (i=0; i<count; i++, tok_gen=tok_gen->next, tok_correct++) {
		if (tok_gen->type != tok_correct->type)
			failed("Token \"%s\": Incorrect type", tok_correct->txt);
		{
			struct token_flags g=tok_gen->flags, c=tok_correct->flags;
			if (g.pp!=c.pp || g.pp_directive!=c.pp_directive)
				failed("Token \"%s\": Incorrect flags", tok_correct->txt);
		}
		switch (tok_gen->type) {
			case TOK_INTEGER:
				if (tok_gen->integer.v != tok_correct->integer.v ||
				    tok_gen->integer.base != tok_correct->integer.base ||
				    tok_gen->integer.suffix != tok_correct->integer.suffix)
					failed("Token \"%s\": Integer value/base/suffix incorrect", tok_correct->txt);;
				break;
			case TOK_FLOATING:
				if (fabsl(tok_gen->floating.v - tok_correct->floating.v) > 0.00000000000000001 ||
				    tok_gen->floating.suffix != tok_correct->floating.suffix)
					failed("Token \"%s\": Floating point value/suffix incorrect", tok_correct->txt);
				break;
			case TOK_OPERATOR:
				if (tok_gen->opkw != tok_correct->opkw)
					failed("Token \"%s\": Operator opkw incorrect", tok_correct->txt);
				break;
			case TOK_KEYWORD:
				if (tok_gen->opkw != tok_correct->opkw)
					failed("Token \"%s\": Keyword opkw incorrect", tok_correct->txt);
				break;
			case TOK_CHAR:
			case TOK_STRING:
				//anything using string
				if (tok_gen->string->size != tok_correct->string->size ||
					memcmp(tok_gen->string->item, tok_correct->string->item,
					tok_gen->string->size) ||
					tok_gen->string->item[tok_gen->string->size] != 0 )
					failed("Token \"%s\": String value incorrect", tok_correct->txt);
				break;
			case TOK_STRING_IQUOTE:
			case TOK_STRING_IANGLE:
				if (strcmp(tok_gen->include, tok_correct->include))
					failed("Token \"%s\": #include string incorrect", tok_correct->txt);
				break;
			case TOK_IDENTIFIER:
			case TOK_CCOMMENT:
			case TOK_CPPCOMMENT:
			case TOK_WHITE:
			case TOK_STARTLINE:
			case TOK_STRAY:
				break;
		}
		if (tok_gen->type!=TOK_STARTLINE && (
			tok_gen->txt_size != tok_correct->txt_size ||
			memcmp(tok_gen->txt, tok_correct->txt, tok_gen->txt_size))
			)
			failed("Token \"%s\": txt incorrect", tok_correct->txt);
	}
	
	#undef failed
done:
	ok(success==1, "Tokenize %s", t->txt);
	
	if (!success)
		token_list_dump(tl, stdout);
	
	talloc_free(txt);
}

static void test_tokenizer_file(const char *file_name, tok_message_queue *mq) {
	FILE *f = fopen(file_name, "rb");
	darray_char *text = talloc_darray(NULL);
	const size_t inc = 1024;
	struct token_list *tl;
	
	if (!f) {
		fail("Could not read file '%s': %s", file_name, strerror(errno));
		goto end;
	}
	
	for (;;) {
		size_t read_len;
		
		darray_realloc(*text, text->size+inc+1);
		read_len = fread(text->item+text->size, 1, inc, f);
		text->size += read_len;
		text->item[text->size] = 0;
		
		if (read_len < inc)
			break;
		
	}
	if (ferror(f)) {
		fail("Error reading file '%s': %s", file_name, strerror(errno));
		goto end;
	}
	
	tl = tokenize(text, text->item, text->size, mq);
	tl->filename = file_name;
	
	//printf("File '%s' has %zu tokens\n", file_name, token_list_count(tl));
	//token_list_dump(tl, stdout);
	
	if (!token_list_sanity_check(tl, stdout)) {
		fail("Sanity check failed for file '%s'", file_name);
		goto end;
	}
	
	pass("File '%s' has %zu tokens", file_name, token_list_count(tl));
	
	/*while (queue_count(*mq)) {
		struct tok_message msg = dequeue(*mq);
		tok_message_print(&msg, tl);
	}*/
	
end:
	talloc_free(text);
	if (f)
		fclose(f);
}

static void test_tokenizer(void) {
	tok_message_queue mq;
	size_t i, count;
	int has_warn_or_worse = 0;
	
	queue_init(mq, NULL);
	
	count = sizeof(tokenizer_tests)/sizeof(*tokenizer_tests);
	for (i=0; i<count; i++) {
		test_tokenizer_single(tokenizer_tests+i, &mq);
		while (queue_count(mq)) {
			struct tok_message msg = dequeue(mq);
			(void) msg;
			//tok_message_dump(&msg);
		}
	}
	
	count = sizeof(tokenizer_msg_tests)/sizeof(*tokenizer_msg_tests);
	for (i=0; i<count; i++) {
		size_t j;
		test_tokenizer_single(&tokenizer_msg_tests[i].test, &mq);
		
		if (queue_count(mq) != tokenizer_msg_tests[i].message_count) {
			fail("Incorrect number of messages from tokenize()");
			while (queue_count(mq))
				(void) dequeue(mq);
			goto msg_fail;
		}
		
		for (j=0; queue_count(mq); j++) {
			struct tok_message msg = dequeue(mq);
			const char *base = "tokenize/";
			size_t baselen = strlen(base);
			//tok_message_dump(&msg);
			
			if (strncmp(msg.path, base, baselen)) {
				fail("Message from tokenize() doesn't start with \"%s\"",
					base);
				goto msg_fail;
			}
			if (strcmp(msg.path+baselen,
					tokenizer_msg_tests[i].messages[j])) {
				fail("Incorrect message %s, should be %s",
					msg.path+baselen, tokenizer_msg_tests[i].messages[j]);
				goto msg_fail;
			}
		}
		
		pass("Messages from tokenize() are correct");
	msg_fail:;
	}
	
	test_tokenizer_file("test/run.c", &mq);
	
	while (queue_count(mq)) {
		struct tok_message msg = dequeue(mq);
		if (msg.level >= TM_WARN) {
			has_warn_or_worse = 1;
			tok_message_dump(&msg);
		}
		//else tok_message_dump(&msg);
	}
	
	ok(has_warn_or_worse==0, "Tokenizing run.c generated%s warnings, errors, or bugs",
		has_warn_or_worse ? "" : " no");
	
	queue_free(mq);
}

#include <unistd.h>

int main(void)
{
	plan_tests(195);
	
	diag("* Checking queue...");
	test_queue();
	
	diag("* Checking read_cstring...");
	test_read_cstring();
	
	diag("* Checking dict...");
	test_dict();
	
	diag("* Checking charflag...");
	test_charflag();
	
	diag("* Checking readui...");
	test_readui();
	
	diag("* Checking scan_number...");
	test_scan_number();
	
	diag("* Checking read_integer...");
	test_read_integer();
	
	diag("* Checking read_floating...");
	test_read_floating();
	
	diag("* Checking tokenizer...");
	test_tokenizer();
	
	/* This exits depending on whether all tests passed */
	return exit_status();
}
