
//for strtold
#define _ISOC99_SOURCE
#include <stdlib.h>
#undef _ISOC99_SOURCE

#include "ccan_tokenizer.h"

#ifndef ULLONG_MAX
#define ULLONG_MAX 18446744073709551615ULL
#endif

static const char *skipnum(const char *s, const char *e, readui_base base) {
	for (;s<e;s++) {
		unsigned int c = (unsigned char)*s;
		
		if (cdigit(c)) {
			if ( c-'0' >= (base & 0xFF) &&
			    !(base & READUI_ALLOWHIGHERDIGITS) )
				break;
		} else if (c>='A' && c<='Z') {
			if (!(base & READUI_ALLOWCAPLETTERS))
				break;
			if ( c-'A'+10 >= (base & 0xFF) &&
			    !(base & READUI_ALLOWHIGHERDIGITS))
				break;
		} else if (c>='a' && c<='z') {
			if (!(base & READUI_ALLOWLCASELETTERS))
				break;
			if ( c-'a'+10 >= (base & 0xFF) &&
			    !(base & READUI_ALLOWHIGHERDIGITS))
				break;
		} else
			break;
	}
	
	return s;
}

static uint64_t readui_valid(const char *s, const char *e, readui_base base) {
	uint64_t ret = 0;
	uint64_t multiplier = 1;
	uint64_t digit_value;
	
	//64-bit multiplication with overflow checking
	#define multiply(dest, src) do { \
		uint32_t a0 = (uint64_t)(dest) & 0xFFFFFFFF; \
		uint32_t a1 = (uint64_t)(dest) >> 32; \
		uint32_t b0 = (uint64_t)(src) & 0xFFFFFFFF; \
		uint32_t b1 = (uint64_t)(src) >> 32; \
		uint64_t a, b; \
		\
		if (a1 && b1) \
			goto overflowed; \
		a = (uint64_t)a1*b0 + (uint64_t)a0*b1; \
		if (a > 0xFFFFFFFF) \
			goto overflowed; \
		a <<= 32; \
		b = (uint64_t)a0*b0; \
		\
		if (a+b < a) \
			goto overflowed; \
		(dest) = a+b; \
	} while(0)
	
	if (s >= e || ((base&0xFF) < 1)) {
		errno = EINVAL;
		return 0;
	}
	
	while (s<e && *s=='0') s++;
	
	if (e > s) {
		for (;;) {
			char c = *--e;
			
			//this series of if statements takes advantage of the fact that 'a'>'A'>'0'
			if (c >= 'a')
				c -= 'a'-10;
			else if (c >= 'A')
				c -= 'A'-10;
			else
				c -= '0';
			digit_value = c;
			
			//TODO:  Write/find a testcase where temp *= multiplier does overflow
			multiply(digit_value, multiplier);
			
			if (ret+digit_value < ret)
				goto overflowed;
			ret += digit_value;
			
			if (e <= s)
				break;
			
			multiply(multiplier, base & 0xFF);
		}
	}
	errno = 0;
	return ret;
	
overflowed:
	errno = ERANGE;
	return ULLONG_MAX;
	
	#undef multiply
}

uint64_t readui(const char **sp, const char *e, readui_base base) {
	const char *s = *sp;
	
	while (s<e && cwhite(*s)) s++;
	e = skipnum(s, e, base);
	
	*sp = e;
	return readui_valid(s, e, base);
}


#define MESSAGE_PATH "tokenize/read_cnumber/"

struct scan_number {
/*
 * Each of the pointers points to the first character of a given component.
 * Consider 0x50.1p+1f .  It would be broken down into:
 */
	const char *prefix;   // 0x
	const char *digits;   // 50.1
	const char *exponent; // p+1
	const char *suffix;   // f
	const char *end;
	size_t dots_found;    // 1
};

/*
 * Scans past all the characters in a number token, fills the struct, and
 * returns one of TOK_INTEGER or TOK_FLOATING to indicate the type.
 *
 * First character must be [0-9 '.']
 */
static enum token_type scan_number(struct scan_number *sn,
					const char *s, const char *e) {
	enum token_type type;
	
	sn->dots_found = 0;
	
	sn->prefix = s;
	sn->digits = s;
	if (s+3<=e && s[0]=='0') {
		if (s[1]=='X' || s[1]=='x') {
		//hexadecimal
			s += 2;
			sn->digits = s;
			for (;s<e;s++) {
				if (*s == '.')
					sn->dots_found++;
				else if (!chex(*s))
					break;
			}
			goto done_scanning_digits;
		} else if (s[1]=='B' || s[1]=='b') {
		//binary
			s += 2;
			if (*s!='0' && *s!='1')
				s -= 2;
			sn->digits = s;
		}
	}
	
	//binary, decimal, or octal
	for (;s<e;s++) {
		if (*s == '.')
			sn->dots_found++;
		else if (!cdigit(*s))
			break;
	}

done_scanning_digits:
	
	sn->exponent = s;
	if (s<e && (
		(sn->prefix==sn->digits && (*s=='E' || *s=='e')) ||
		(sn->prefix < sn->digits && (*s=='P' || *s=='p'))
	)) {
		s++;
		if (s<e && (*s=='+' || *s=='-'))
			s++;
		while (s<e && cdigit(*s)) s++;
	}
	
	sn->suffix = s;
	while (s<e && (cdigit(*s) || cletter(*s) ||
		*s=='.' || *s=='_' || *s=='$')) s++;
	
	sn->end = s;
	
	//Now we're done scanning, but now we want to know what type this is
	type = TOK_INTEGER;
	if (sn->dots_found)
		type = TOK_FLOATING;
	if (sn->exponent < sn->suffix)
		type = TOK_FLOATING;
	
	//if this is an octal, make the leading 0 a prefix
	if (type==TOK_INTEGER && sn->prefix==sn->digits &&
			sn->digits < s && sn->digits[0]=='0')
		sn->digits++;
	
	return type;
}

static enum tok_suffix read_number_suffix(const char *s, const char *e,
			enum token_type type, tok_message_queue *mq) {
	const char *orig_s = s;
	enum tok_suffix sfx = 0;
	
	//read the suffix in pieces
	while (s<e) {
		enum tok_suffix sfx_prev = sfx;
		char c = *s++;
		if (c>='a' && c<='z')
			c -= 'a'-'A';
		
		if (c=='L') {
			if (s<e && (*s=='L' || *s=='l')) {
				s++;
				sfx |= TOK_LL;
				
				//TOK_L and TOK_LL are mutually exclusive
				if (sfx & TOK_L)
					goto invalid;
			} else {
				sfx |= TOK_L;
			}
		}
		else if (c=='U')
			sfx |= TOK_U;
		else if (c=='F')
			sfx |= TOK_F;
		else if (c=='I')
			sfx |= TOK_I;
		else
			goto invalid;
		
		if (sfx == sfx_prev)
			goto invalid; //suffix piece was repeated
	}
	
	//make sure the suffix is appropriate for this number type
	if (type==TOK_INTEGER && (sfx & TOK_F)) {
		tok_msg_error(suffix_float_only, orig_s,
		"Suffix only valid for floating point numbers");
		sfx = TOK_NOSUFFIX;
	}
	if (type==TOK_FLOATING && (sfx & (TOK_U | TOK_LL))) {
		tok_msg_error(suffix_integer_only, orig_s,
		"Suffix only valid for integers");
		sfx = TOK_NOSUFFIX;
	}
	
	return sfx;
	
invalid:
	if (type==TOK_INTEGER)
		tok_msg_error(integer_suffix_invalid, orig_s,
				"Integer suffix invalid");
	else
		tok_msg_error(floating_suffix_invalid, orig_s,
				"Floating point suffix invalid");
	return TOK_NOSUFFIX;
}

static void read_integer(struct tok_integer *out, const struct scan_number *sn,
			tok_message_queue *mq) {
	/*
	Assertions about an integer's struct scan_number:
		prefix is empty or [0 0B 0b 0X 0x]
		sn->digits is not empty (i.e. sn->digits < sn->exponent)
			*unless* the prefix is "0"
		has no exponent
		suffix is [0-9 A-Z a-z '.']*
		dots_found == 0
	*/
	readui_base base = READUI_DEC;
	const char *tokstart = sn->prefix;
	const char *s = sn->digits, *e = sn->exponent;
	
	if (sn->prefix+1 < sn->digits) {
		if (sn->prefix[1]=='X' || sn->prefix[1]=='x')
			base = READUI_HEX;
		else
			base = READUI_OCT;
	} else if (sn->prefix < sn->digits) {
		base = READUI_OCT;
	}
	
	if (s>=e && base==READUI_OCT) {
		//octal contains no digits
		out->v = 0;
		out->base = 8;
		goto suffix;
	}
	
	out->v = readui(&s, sn->exponent, base);
	out->base = base & 0xFF;
	
	if (s != e || errno == EINVAL) {
		tok_msg_error(integer_invalid_digits, tokstart,
			"Integer constant contains invalid digits");
	} else if (errno) {
		if (errno == ERANGE) {
			tok_msg_error(integer_out_of_range, tokstart,
				"Integer constant out of range");
		} else {
			tok_msg_bug(readui_unknown, tokstart,
				"Unknown error returned by readui");
		}
	}
	
suffix:
	out->suffix =
		read_number_suffix(sn->suffix, sn->end, TOK_INTEGER, mq);
	
	return;
}

static void read_floating(struct tok_floating *out, const struct scan_number *sn,
			tok_message_queue *mq) {
	/*
	Assertions about a float's struct scan_number:
		prefix is empty or [0B 0b 0X 0x] (note: no octal prefix 0)
		sn->digits not empty, ever
		exponent may or may not exist
		If exponent exists, it is valid and formatted as:
			( [E P e p] ['+' '-']*0..1 [0-9]* )
		An exponent starts with E if this is decimal, P if it is hex/binary
		suffix is [0-9 A-Z a-z '.']*
		dots_found can be anything
	*/
	const char *tokstart = sn->prefix;
	const char *s = sn->prefix, *e = sn->suffix;
	char borrow = *sn->end;
	//long double strtold(const char *nptr, char **endptr);
	
	out->v = 0.0;
	out->suffix = TOK_NOSUFFIX;
	
	if (sn->prefix < sn->digits) {
		if (sn->prefix[1]=='B' || sn->prefix[1]=='b') {
			tok_msg_error(binary_float, tokstart,
				"Binary floating point constants not allowed");
			return;
		}
		if (sn->exponent >= sn->suffix) {
			tok_msg_error(hex_float_no_exponent, tokstart,
				"Hex floating point constant missing exponent");
			return;
		}
	}
	
	
	/* Stick a null terminator at the end of the input so strtold
	 * won't read beyond the given input.
	 *
	 * This is thread-safe because the input is from
	 * token_list.txt, which was generated in the
	 * tokenize function which is still running.
	 */
	*(char*)sn->end = 0;
	errno = 0;
	out->v = strtold(s, (char**)&s);
	//don't forget to set it back
	*(char*)sn->end = borrow;
	
	if (errno) {
		//for some reason, strtold may errno to EDOM to indicate underrun
		//open test/run.c and search "floating_out_of_range" for more details
		if (errno == ERANGE || errno == EDOM) {
			tok_msg_error(floating_out_of_range, tokstart,
				"Floating point constant out of range");
		} else {
			tok_msg_bug(strtold_unknown, tokstart,
				"Unknown error returned by strtold");
		}
	}
	
	if (s != e) {
		tok_msg_error(floating_invalid_digits, tokstart,
			"Floating point constant contains invalid digits");
	}
	
	out->suffix =
		read_number_suffix(sn->suffix, sn->end, TOK_FLOATING, mq);
}

char *read_cnumber(struct token *tok, const char *s, const char *e, tok_message_queue *mq) {
	struct scan_number sn;
	
	tok->type = scan_number(&sn, s, e);
	if (tok->type == TOK_INTEGER)
		read_integer(&tok->integer, &sn, mq);
	else
		read_floating(&tok->floating, &sn, mq);
	
	return (char*)sn.end;
}

#undef MESSAGE_PATH
