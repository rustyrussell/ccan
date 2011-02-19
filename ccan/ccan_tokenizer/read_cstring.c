#include "ccan_tokenizer.h"

static char *strdup_rng(const char *s, const char *e) {
	char *ret = malloc(e-s+1);
	memcpy(ret, s, e-s);
	ret[e-s] = 0;
	return ret;
}

#define MESSAGE_PATH "tokenize/read_cstring/"

//Reads a C string starting at s until quoteChar is found or e is reached
//  Returns the pointer to the terminating quote character or e if none was found
char *read_cstring(darray_char *out, const char *s, const char *e, char quoteChar, tok_message_queue *mq) {
	const char * const tokstart = s;
	const char *p;
	int has_endquote=0, has_newlines=0;
	
	//tok_msg_debug(called, s, "Called read_cstring on `%s`", s);
	
	#define append(startptr,endptr) darray_append_items(*out, startptr, (endptr)-(startptr))
	#define append_char(theChar) darray_append(*out, theChar)
	#define append_zero() do {darray_append(*out, 0); out->size--;} while(0)
	
	p = s;
	while (p<e) {
		char c = *p++;
		if (c == '\\') {
			append(s, p-1);
			s = p;
			if (p >= e) {
				append_char('\\');
				tok_msg_error(ended_in_backslash, p-1,
					"read_cstring input ended in backslash");
				break;
			}
			c = *p++;
			if (c>='0' && c<='9') {
				unsigned int octal = c-'0';
				size_t digit_count = 0;
				while (p<e && *p>='0' && *p<='9') {
					octal <<= 3;
					octal += (*p++) - '0';
					if (++digit_count >= 2)
						break;
				}
				if (p<e && *p>='0' && *p<='9') {
					tok_msg_info(ambiguous_octal, s-2,
						"Octal followed by digit");
				}
				if (octal > 0xFF) {
					tok_msg_warn(octal_overflow, s-2,
						"Octal out of range");
				}
				c = octal;
			} else {
				switch (c) {
					case 'x': {
						size_t digit_count = 0;
						size_t zero_count = 0;
						unsigned int hex = 0;
						while (p<e && *p=='0') p++, zero_count++;
						for (;p<e;digit_count++) {
							c = *p++;
							if (c>='0' && c<='9')
								c -= '0';
							else if (c>='A' && c<='F')
								c -= 'A'-10;
							else if (c>='a' && c<='f')
								c -= 'a'-10;
							else {
								p--;
								break;
							}
							hex <<= 4;
							hex += c;
						}
						if (zero_count+digit_count > 2) {
							char *hex_string = strdup_rng(s-2, p);
							tok_msg_warn(ambiguous_hex, s-2,
								"Hex escape '%s' is ambiguous", hex_string);
							if (digit_count > 2)
								tok_msg_warn(hex_overflow, s-2,
									"Hex escape '%s' out of range", hex_string);
							free(hex_string);
						}
						c = hex & 0xFF;
					}	break;
					case 'a':
						c=0x7;
						break;
					case 'b':
						c=0x8;
						break;
					case 'e':
						c=0x1B;
						break;
					case 'f':
						c=0xC;
						break;
					case 'n':
						c=0xA;
						break;
					case 'r':
						c=0xD;
						break;
					case 't':
						c=0x9;
						break;
					case 'v':
						c=0xB;
						break;
					case '\\':
						break;
					default:
						if (c == quoteChar)
							break;
						if (c=='\'' && quoteChar=='"') {
							/* tok_msg_info(escaped_single_quote, s-2,
								"Single quote characters need not be escaped within double quotes"); */
							break;
						}
						if (c=='"' && quoteChar=='\'') {
							/* tok_msg_info(escaped_double_quote, s-2,
								"Double quote characters need not be escaped within single quotes"); */
							break;
						}
						if (c=='?') // \? is needed in some situations to avoid building a trigraph
							break;
						tok_msg_warn(unknown_escape, s-2,
							"Unknown escape sequence '\\%c'", c);
						break;
				}
			}
			s = p;
			append_char(c);
		} else if (c == quoteChar) {
			p--;
			has_endquote = 1;
			break;
		} else if (creturn(c)) {
			has_newlines = 1;
		}
	}
	append(s, p);
	append_zero();
	if (!has_endquote) {
		tok_msg_error(missing_endquote, tokstart,
			"Missing endquote on %s literal",
			quoteChar=='\'' ? "character" : "string");
	} else if (has_newlines) {
		tok_msg_warn(quote_newlines, tokstart,
			"%s literal contains newline character(s)",
			quoteChar=='\'' ? "Character" : "String");
	}
	return (char*)p;
	
	#undef append
	#undef append_char
	#undef append_zero
}

#undef MESSAGE_PATH
